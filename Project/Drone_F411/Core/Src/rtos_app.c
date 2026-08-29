/**
 * freertos.c — FreeRTOS 任务创建与调度管理
 *
 * 任务布局:
 *   ImuRead  (prio 6, 500Hz) 零偏校准 + IMU 采集 → xImuQueue
 *   Attitude (prio 5, 500Hz) 滤波 + Mahony 姿态解算 → g_att 共享区
 *   Control  (prio 4, 200Hz) 安全链 + 状态机 + 串级PID + 混控 + IWDG喂狗
 *   Comm     (prio 3)        USART1 字节 → BLE 协议解析 → 控制目标/解锁/急停
 *
 * 修复记录:
 *   1. 修复 vControlTask 前注释块丢失开头定界符导致的语法错误
 *   2. g_att / g_att_ready 原在 vAttitudeTask 中先用后声明 → 提前定义
 *   3. 零偏校准从 Attitude 任务移到 ImuRead 任务开头:
 *      原实现两任务并发读同一条 I2C (HAL I2C 句柄无锁), 存在总线竞争;
 *      现在校准期间 ImuRead 是唯一 I2C 使用者, 天然串行化
 *   4. 姿态任务 200Hz→500Hz, 与 IMU 采样率匹配:
 *      原 500Hz 生产 / 200Hz 消费, 队列 ~7s 后恒满丢帧
 *   5. 新增 Comm 任务, 打通 BLE 指令链路 (原协议解析器无人喂数据)
 *   6. 新增姿态丢失 failsafe: 连续 100 拍 (0.5s) 无新姿态 → 急停
 *      (原实现 IMU 挂掉后电机保持最后占空比, 极危险)
 *   7. 实现 MallocFailed / StackOverflow Hook (原配置开启但未实现, 链接报错)
 *   8. IWDG 喂狗与 Safety_Update 状态记录接入控制循环
 */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "rtos_app.h"
#include "mpu6050_i2c.h"
#include "data_process.h"
#include "control.h"
#include "safety.h"
#include "motor_pwm.h"
#include "ble_protocol.h"
#include "usart.h"
#include "battery.h"
#include <stdio.h>

/* =============================================================
 * 全局句柄
 * ============================================================= */
QueueHandle_t     xImuQueue;      /* IMU 原始数据: ImuRead → Attitude */
QueueHandle_t     xByteQueue;     /* UART 字节: USART1 ISR → Comm */
static SemaphoreHandle_t xImuReadySem; /* 校准完成: ImuRead → Attitude */

/* 姿态共享区: Attitude(优先级5, 写) → Control(优先级4, 读)
 * 写方优先级更高不会被读方抢占, 整写; 读方在临界区内整读 */
static attitude_t g_att;
static volatile uint8_t g_att_ready = 0;

/* 前向声明 */
static void vImuReadTask(void *pv);
static void vAttitudeTask(void *pv);
static void vControlTask(void *pv);
static void vCommTask(void *pv);

/* 任务参数 */
#define ATT_PRINT_DIV    250     /* 每 250 拍 (0.5s @500Hz) 打印一次姿态 */
#define ATT_LOST_LIMIT   100     /* 连续 100 拍 (0.5s) 无姿态 → 急停 */

/* =============================================================
 * freertos_start() — 创建任务并启动调度器
 * ============================================================= */
void freertos_start(void)
{
	printf("[RTOS] Starting FreeRTOS...\r\n");

	xImuQueue    = xQueueCreate(10, sizeof(imu_data_t));
	xByteQueue   = xQueueCreate(64, sizeof(uint8_t));
	xImuReadySem = xSemaphoreCreateBinary();
	if (!xImuQueue || !xByteQueue || !xImuReadySem) {
		printf("[FATAL] Queue/Sem create failed\r\n");
		while (1);
	}

	BaseType_t ret;
	ret = xTaskCreate(vImuReadTask,  "ImuRead",  256, NULL, 6, NULL);
	if (ret != pdPASS) { printf("[FATAL] ImuRead fail\r\n");  while (1); }

	ret = xTaskCreate(vAttitudeTask, "Attitude", 512, NULL, 5, NULL);
	if (ret != pdPASS) { printf("[FATAL] Attitude fail\r\n"); while (1); }

	ret = xTaskCreate(vControlTask,  "Control",  512, NULL, 4, NULL);
	if (ret != pdPASS) { printf("[FATAL] Control fail\r\n");  while (1); }

	ret = xTaskCreate(vCommTask,     "Comm",     384, NULL, 3, NULL);
	if (ret != pdPASS) { printf("[FATAL] Comm fail\r\n");     while (1); }

	printf("[RTOS] Scheduler start\r\n");
	vTaskStartScheduler();

	while (1);
}

/* =============================================================
 * IMU 读取任务 (500Hz)
 * 上电先做零偏校准 (0.5s, 期间喂 IWDG), 完成后放行 Attitude 任务,
 * 之后 ImuRead 是运行期唯一的 I2C 使用者, 无并发竞争
 * ============================================================= */
static void vImuReadTask(void *pv)
{
	imu_data_t imu;
	TickType_t lastWake = xTaskGetTickCount();
	(void)pv;

	DataProcess_Init(250);          /* 250 样本 × 2ms = 0.5s 静置校准 */
	xSemaphoreGive(xImuReadySem);

	for (;;) {
		if (MPU6050_Read(&imu) == 0) {
			xQueueSend(xImuQueue, &imu, 0);
		}
		vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(2));
	}
}

/* =============================================================
 * 姿态解算任务 (500Hz) — 滤波 + Mahony → 姿态角/垂直加速度
 * ============================================================= */
static void vAttitudeTask(void *pv)
{
	imu_data_t imu;
	attitude_t att;
	TickType_t lastWake = xTaskGetTickCount();
	uint32_t cnt = 0;
	(void)pv;

	/* 等待零偏校准完成 (由 ImuRead 放行) */
	xSemaphoreTake(xImuReadySem, portMAX_DELAY);
	printf("[ATT] calibration done, tracking\r\n");

	for (;;) {
		if (xQueueReceive(xImuQueue, &imu, pdMS_TO_TICKS(2)) == pdTRUE) {
			DataProcess_Update(&imu, &att);

			/* 共享姿态给控制任务 (本任务优先级更高, 整写安全) */
			g_att = att;
			g_att_ready = 1;

			if (++cnt >= ATT_PRINT_DIV) {
				cnt = 0;
				printf("[ATT] R=%+6.1f P=%+6.1f Y=%+6.1f deg\r\n",
				       att.roll * 57.2958f,
				       att.pitch * 57.2958f,
				       att.yaw * 57.2958f);
			}
		}
		vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(2));
	}
}

/* =============================================================
 * 控制任务 (200Hz) — 串级PID + 混控 + 电机输出 + IWDG + 安全记录
 * ============================================================= */
static void vControlTask(void *pv)
{
	TickType_t lastWake = xTaskGetTickCount();
	attitude_t attCopy;
	uint32_t attMiss = 0;
	float az, gyro[3], motor[4];
	(void)pv;

	Control_Init();

	for (;;) {
		IWDG_Feed();                    /* 每拍喂狗: 任务卡死 → 1s 复位 */

		if (g_att_ready) {
			/* 临界区整读并清标志 (写方优先级更高, 不会被本任务打断写入) */
			taskENTER_CRITICAL();
			attCopy = g_att;
			g_att_ready = 0;
			taskEXIT_CRITICAL();
			attMiss = 0;

			az = DataProcess_GetVerticalAccel();
			DataProcess_GetGyro(gyro);
			Control_Update(&attCopy, gyro, az);

			/* 安全状态检查: 每拍喂狗 + 电池电压 + 姿态安全 */
			Motor_GetLast(motor);
			Safety_Update(attCopy.roll, attCopy.pitch, attCopy.yaw,
			              motor[0], motor[1], motor[2], motor[3],
			              Battery_GetVoltage(), HAL_GetTick());

			/* 低压保护联动: 电压过低 → 油门归零自动降落 */
			if (g_safety.status == SAFE_LOW_BATTERY) {
				control_target_t t = {0};
				Control_SetTarget(&t);   /* throttle=0 → 状态机进 LANDING */
				printf("[BAT] LOW -> auto land\r\n");
			}
		} else if (++attMiss >= ATT_LOST_LIMIT) {
			/* 0.5s 无新姿态: IMU/解算链路故障 → 急停
			 * (否则电机会冻结在最后占空比) */
			printf("[CTRL] ATTITUDE LOST -> EMERGENCY\r\n");
			Control_EmergencyStop();
			attMiss = 0;
		}
		vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(5));
	}
}

/* =============================================================
 * 通信任务 — USART1 字节流 → BLE 帧 → 控制指令分发
 * ============================================================= */
static void vCommTask(void *pv)
{
	uint8_t byte, ack[8];
	(void)pv;

	BLE_Protocol_Init();
	USART1_StartRx();                  /* 此后 USART1 中断开始喂数据 */
	printf("[COMM] BLE link ready\r\n");

	for (;;) {
		if (xQueueReceive(xByteQueue, &byte, portMAX_DELAY) != pdTRUE)
			continue;

		if (!BLE_Protocol_Feed(byte))
			continue;               /* 未凑成完整帧 */

		/* 完整且校验通过的帧 → 分发 */
		Safety_NotifyCmd();
		uint8_t cmd = BLE_GetCmd();
		const uint8_t *data = BLE_GetData();
		uint8_t len = BLE_GetDataLen();

		switch (cmd) {

		case BLE_CMD_SETPOINT: {
			BleSetpoint_t sp;
			if (BLE_ParseSetpoint(&sp)) {
				control_target_t t = {
					.throttle        = sp.throttle,
					.target_roll     = sp.roll,
					.target_pitch    = sp.pitch,
					.target_yaw_rate = sp.yaw_rate,
				};
				Control_SetTarget(&t);
			} else {
				printf("[COMM] bad SETPOINT payload\r\n");
			}
			break;
		}

		case BLE_CMD_ARM:
			if (len == 1) {
				Control_Armed(data[0]);
				Safety_SetArmed(data[0]);
				Control_KeepAlive();
			}
			break;

		case BLE_CMD_EMERGENCY:
			Control_EmergencyStop();
			Control_KeepAlive();
			break;

		case BLE_CMD_PING: {
			int n = BLE_BuildAck(cmd, 0, ack);
			HAL_UART_Transmit(&huart1, ack, (uint16_t)n, 20);
			Control_KeepAlive();
			break;
		}

		default:
			Control_KeepAlive();  /* 收到任何有效帧都视为链路存活 */
			break;
		}
	}
}

/* =============================================================
 * FreeRTOS Hook (FreeRTOSConfig.h 已开启, 必须实现)
 * ============================================================= */
void vApplicationMallocFailedHook(void)
{
	Motor_Stop();
	printf("[RTOS] MALLOC FAILED!\r\n");
	taskDISABLE_INTERRUPTS();
	for (;;);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
	(void)xTask;
	Motor_Stop();
	printf("[RTOS] STACK OVERFLOW in %s!\r\n", pcTaskName);
	taskDISABLE_INTERRUPTS();
	for (;;);
}
