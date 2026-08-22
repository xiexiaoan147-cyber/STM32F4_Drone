/**
 * freertos.c — FreeRTOS 任务创建与调度管理
 */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "freertos.h"
#include "mpu6050_i2c.h"
#include "data_process.h"
#include "control.h"
#include <stdio.h>

/* 队列句柄 */
QueueHandle_t xImuQueue;

/* 前向声明 */
static void vImuReadTask(void *pv);
static void vAttitudeTask(void *pv);
static void vControlTask(void *pv);

/* =============================================================
 * freertos_start() — 创建任务并启动调度器
 * ============================================================= */
void freertos_start(void)
{
	printf("[RTOS] Starting FreeRTOS...\r\n");

	xImuQueue = xQueueCreate(10, sizeof(imu_data_t));
	if (!xImuQueue) {
		printf("[FATAL] Queue create failed\r\n");
		while (1);
	}

	BaseType_t ret;
	ret = xTaskCreate(vImuReadTask,   "ImuRead",   256, NULL, 6, NULL);
	if (ret != pdPASS) { printf("[FATAL] ImuRead fail\r\n"); while (1); }

	ret = xTaskCreate(vAttitudeTask, "Attitude",  512, NULL, 5, NULL);
	if (ret != pdPASS) { printf("[FATAL] Attitude fail\r\n"); while (1); }

	ret = xTaskCreate(vControlTask, "Control",   512, NULL, 4, NULL);
	if (ret != pdPASS) { printf("[FATAL] Control fail\r\n"); while (1); }

	printf("[RTOS] Scheduler start\r\n");
	vTaskStartScheduler();

	while (1);
}

/* =============================================================
 * IMU 读取任务 (500Hz) — 采集原始数据入队
 * ============================================================= */
static void vImuReadTask(void *pv)
{
	imu_data_t imu;
	TickType_t lastWake = xTaskGetTickCount();

	for (;;) {
		if (MPU6050_Read(&imu) == 0) {
			xQueueSend(xImuQueue, &imu, 0);
		}
		vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(2));
	}
}

/* =============================================================
 * 姿态解算任务 (200Hz) — 数据处理 + Mahony
 * ============================================================= */
static void vAttitudeTask(void *pv)
{
	imu_data_t imu;
	attitude_t att;
	TickType_t lastWake = xTaskGetTickCount();
	uint32_t cnt = 0;

	/* 上电零偏校准 (静止 0.4s) */
	DataProcess_Init(200);

	for (;;) {
		if (xQueueReceive(xImuQueue, &imu, 0) == pdTRUE) {
			DataProcess_Update(&imu, &att);

			/* 共享姿态给控制任务 */
			g_att = att;
			g_att_ready = 1;

			if (++cnt >= 100) {   /* 每 0.5s 打印 */
				cnt = 0;
				printf("[ATT] R=%+6.1f P=%+6.1f Y=%+6.1f deg\r\n",
				       att.roll * 57.2958f,
				       att.pitch * 57.2958f,
				       att.yaw * 57.2958f);
			}
		}
		vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(5));
	}
}

/* =============================================================
 * 姿态结果共享区 (Attitude任务写, Control任务读)
 * ============================================================= */
static attitude_t g_att;
static volatile uint8_t g_att_ready = 0;

 * 控制任务 (200Hz) — 串级PID + 混控 + 电机输出
 * ============================================================= */
static void vControlTask(void *pv)
{
	TickType_t lastWake = xTaskGetTickCount();

	Control_Init();

	for (;;) {
		if (g_att_ready) {
			g_att_ready = 0;
			float az = DataProcess_GetVerticalAccel();
			float gyro[3];
			DataProcess_GetGyro(gyro);
			Control_Update(&g_att, gyro, az);
		}
		vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(5));
	}
}
