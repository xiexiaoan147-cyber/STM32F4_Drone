/**
 * control.c — 飞控控制实现 (状态机 + 串级PID + X型混控 + 安全链)
 *
 * 每 5ms (200Hz):
 *   安全链 → 状态机/油门更新 → 串级PID → 混控 → 电机
 * 悬停: az 闭环 (松杆后微调油门直到垂直加速度=0)
 *
 * ============================================================
 * 逻辑漏洞修复记录 (相对初版):
 *   1. 积分项补上 ×dt (原裸累加误差, 增益隐含依赖 200Hz 控制周期);
 *      抗饱和改为 "I 项输出限幅 + 积分器回算", INT_LIMIT 语义明确为
 *      I 项最大输出权限 (±15% 油门), 原实现最大只有 0.15% 形同虚设
 *   2. D 项改为测量值(角速度)微分, 消除打杆瞬间的 derivative kick;
 *      历史值每拍刷新, 避免地面长时间停转后首次起飞出现 D 尖峰
 *   3. 外环输出补上 RATE_MAX 限幅 (原常量定义了但从未使用)
 *   4. 姿态超限加 200ms 持续时间去抖 (原单帧超限立即断电机,
 *      传感器噪声尖峰会在空中误杀); NaN/Inf 传感器数据直接急停
 *      (原实现 NaN 与任何比较均为假, 会绕过全部保护进入混控)
 *   5. HOVER→LANDING 改为独立计时 hover_enter_ms
 *      (原判据与失控判据共用 last_cmd_ms, 而 HOVER 期间指令必然
 *      在刷新 last_cmd_ms, 该规则永远不可能触发, 属死逻辑)
 *   6. FLY_LANDING 落地(油门降到0)后自动上锁回 FLY_IDLE
 *      (原状态机无出口, 会永远停留在 LANDING 态)
 *   7. 急停可恢复: Control_Armed(0) 清除 emergency 锁存并复位 PID;
 *      急停未解除时拒绝重新解锁
 *   8. tgt 读取改为临界区快照 (4×float 拷贝非原子,
 *      跨任务读写会读到"半新半旧"的目标)
 *   9. 油门为 0 (地面/停转)时不跑 PID 并清积分
 *      (防止起飞前机身倾斜时积分器持续漂移)
 *  10. ki_rate 量纲修正为 1/s, 默认值 0.01→2.0
 *      (与旧实现等效: 旧 0.01×200Hz = 2.0/s, 强度不变、量纲正确)
 * ============================================================
 */
#include "control.h"
#include "mixer.h"
#include "motor_pwm.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <math.h>

/* ============================================================
 * 参数
 * 注意: ki_rate 单位 1/s, 与 ∫err·dt 配合;
 * 旧默认 0.01 是配合"裸累加"的写法, 等效强度 = 0.01×200Hz = 2.0
 * ============================================================ */
static pid_params_t pid =
{
	.kp_angle  = 6.0f,
	.kp_rate   = 0.15f,
	.ki_rate   = 2.0f,
	.kd_rate   = 0.005f,
};

/* 角速度环积分状态 (err·s) */
static float int_roll  = 0.0f;
static float int_pitch = 0.0f;
static float int_yaw   = 0.0f;

/* D 项历史: 上一拍测量角速度 (测量值微分) */
static float    prev_gyro[3] = {0, 0, 0};
static uint8_t  d_ready = 0;         /* 首拍只建历史不输出 D */

/* 控制目标与状态 */
static control_target_t tgt;
static flight_state_t state = FLY_IDLE;
static uint8_t  armed = 0;
static uint8_t  emergency = 0;       /* 急停锁存 (上锁可清除) */
static uint32_t last_cmd_ms = 0;
static uint32_t now_ms = 0;

/* 状态机计时器 */
static uint32_t hover_enter_ms = 0;  /* 进入 HOVER 的时刻 (自动降落计时) */
static uint32_t att_over_ms    = 0;  /* 姿态超限开始时刻 (去抖) */

/* 油门状态 */
static float actual_throttle = 0.0f; /* 状态机裁决后的实际输出油门 */
static float az = 0.0f;              /* 最近一拍垂直加速度 (HOVER 闭环用) */

/* 限幅常量 */
#define CTRL_DT             0.005f                /* 控制周期 200Hz */
#define RATE_MAX_DEG        200.0f                /* 期望角速度限幅 */
#define RATE_MAX_RAD        (RATE_MAX_DEG * 0.017453293f)
#define CTRL_ANGLE_MAX_RAD  0.52f                 /* 目标角限幅 ±30° (与 BLE 协议一致) */
#define INT_LIMIT           0.15f                 /* I 项输出限幅 (±15% 油门权限) */
#define ANGLE_LIMIT_RAD     0.7854f               /* 姿态保护限 45° */
#define ATT_LIMIT_HOLD_MS   200                   /* 姿态超限持续时间 (去抖) */
#define CMD_TIMEOUT_MS      1000                  /* 失控判定 */
#define HOVER_AUTO_LAND_MS  3000                  /* HOVER 持续 3s 自动缓降落地 */
#define LAND_DESCENT_RATE   0.3f                  /* 降落: 油门下降速率 /s */
#define THROTTLE_MIN        0.30f                 /* HOVER 油门下限 */
#define THROTTLE_MAX        0.85f                 /* HOVER 油门上限 */
#define AZ_GAIN             0.0002f               /* az → 油门 闭环增益 */
#define THROTTLE_ARM_MIN    0.05f                 /* 油门高于此值判"推杆" */

/* ============================================================
 * 小工具
 * ============================================================ */
static inline float clampf(float x, float lo, float hi)
{
	if (x < lo) return lo;
	if (x > hi) return hi;
	return x;
}

/* 复位 PID 动态状态 (不动参数和目标) */
static void pid_state_reset(void)
{
	int_roll = int_pitch = int_yaw = 0.0f;
	prev_gyro[0] = prev_gyro[1] = prev_gyro[2] = 0.0f;
	d_ready = 0;
	att_over_ms = 0;
}

/* I 项: 积分(×dt) + 输出限幅抗饱和 (超限时回算积分器, 等价条件积分) */
static float integral_out(float *integ, float err, float ki)
{
	if (ki <= 0.0f) {
		*integ = 0.0f;
		return 0.0f;
	}
	*integ += err * CTRL_DT;
	float out = ki * (*integ);
	if (out > INT_LIMIT) {
		out = INT_LIMIT;
		*integ = INT_LIMIT / ki;
	} else if (out < -INT_LIMIT) {
		out = -INT_LIMIT;
		*integ = -INT_LIMIT / ki;
	}
	return out;
}

/* ============================================================
 * 初始化
 * ============================================================ */
void Control_Init(void)
{
	tgt.throttle        = 0.0f;
	tgt.target_roll     = 0.0f;
	tgt.target_pitch    = 0.0f;
	tgt.target_yaw_rate = 0.0f;
	pid_state_reset();
	actual_throttle = 0.0f;
	armed = 0;
	emergency = 0;
	state = FLY_IDLE;
	last_cmd_ms = 0;
	hover_enter_ms = 0;
	printf("[CTRL] Init OK\r\n");
}

void Control_SetPID(const pid_params_t *p) { pid = *p; }

/* 注意: 调用方应为任务上下文; 若以后改在中断里喂指令,
 * 应换成队列传递, 不能直接调本函数 */
void Control_SetTarget(const control_target_t *t)
{
	taskENTER_CRITICAL();   /* 4×float 拷贝非原子, 防撕裂 */
	tgt = *t;
	taskEXIT_CRITICAL();
	last_cmd_ms = now_ms;   /* 刷新指令心跳 (失控计时归零) */
}

/* 非控制目标类有效帧 (PING/ARM/EMERGENCY...) 也视为链路存活,
 * 防止 App 只发心跳不发 SETPOINT 时被误判失控 */
void Control_KeepAlive(void)
{
	last_cmd_ms = now_ms;
}

void Control_Armed(int arm)
{
	if (arm) {
		if (emergency) {
			printf("[CTRL] ARM refused: emergency active, disarm first\r\n");
			return;         /* 急停未解除, 拒绝解锁 */
		}
		armed = 1;
		state = FLY_DISARMED;
		pid_state_reset();  /* 干净起飞: 清积分/D历史/去抖计时 */
		printf("[CTRL] ARMED\r\n");
	} else {
		armed = 0;
		emergency = 0;      /* 上锁即解除急停锁存 (此时电机已停, 安全) */
		state = FLY_IDLE;
		actual_throttle = 0.0f;
		pid_state_reset();
		Motor_Stop();
		printf("[CTRL] DISARMED\r\n");
	}
}

void Control_EmergencyStop(void)
{
	emergency = 1;
	state = FLY_IDLE;
	actual_throttle = 0.0f;
	pid_state_reset();
	Motor_Stop();
	printf("[CTRL] EMERGENCY STOP!\r\n");
}

flight_state_t Control_GetState(void) { return state; }

/* ============================================================
 * 状态机 + 油门更新 (t: 本拍目标快照)
 * ============================================================ */
static void UpdateStateMachine(const control_target_t *t)
{
	switch (state) {

	case FLY_IDLE:
		actual_throttle = 0.0f;
		return;

	case FLY_DISARMED:
		/* 油门>5% → 进入跟随(起飞) */
		if (t->throttle > THROTTLE_ARM_MIN) {
			state = FLY_FOLLOW;
			actual_throttle = t->throttle;
		} else {
			actual_throttle = 0.0f;
		}
		return;

	case FLY_FOLLOW:
		if (t->throttle > THROTTLE_ARM_MIN) {
			/* 推油门: 直接跟随 */
			actual_throttle = t->throttle;
		} else {
			/* 松杆: 进入 az 闭环悬停 (保留当前油门作闭环起点, 衔接平滑) */
			state = FLY_HOVER;
			hover_enter_ms = now_ms ? now_ms : 1;   /* 自动降落计时起点 */
		}
		return;

	case FLY_HOVER:
		if (t->throttle > THROTTLE_ARM_MIN) {
			/* 再推油门: 回跟随, 抢回控制权 */
			state = FLY_FOLLOW;
			actual_throttle = t->throttle;
			return;
		}
		/* az 闭环: az>0 上升减油门, az<0 下落加油门, 收敛到 az=0 */
		actual_throttle -= az * AZ_GAIN;
		actual_throttle = clampf(actual_throttle, THROTTLE_MIN, THROTTLE_MAX);
		return;

	case FLY_LANDING:
		/* 匀速收油门缓降 (0.3/s) */
		actual_throttle -= LAND_DESCENT_RATE * CTRL_DT;
		if (actual_throttle <= 0.0f) {
			actual_throttle = 0.0f;
			armed = 0;       /* 落地完成: 自动上锁, 需重新解锁才能再飞 */
			state = FLY_IDLE;
			printf("[CTRL] Landed -> auto disarm\r\n");
		}
		return;

	default:
		state = FLY_IDLE;
		actual_throttle = 0.0f;
		return;
	}
}

/* ============================================================
 * 每 5ms 控制循环
 * ============================================================ */
void Control_Update(const attitude_t *att, const float gyro[3], float az_body)
{
	now_ms = HAL_GetTick();
	az = az_body;

	/* ---------- 0. D 项预处理: 测量值(角速度)微分 ----------
	 * 放在最前且每拍刷新历史: 即使下面安全链早退,
	 * 也不会留下"历史空洞", 避免停转很久后第一次更新出现 D 尖峰。
	 * 负号: D 作用于误差 = 目标-测量, 目标平滑时 d(err)/dt ≈ -d(gyro)/dt
	 */
	float der[3];
	if (!d_ready) {
		prev_gyro[0] = gyro[0];
		prev_gyro[1] = gyro[1];
		prev_gyro[2] = gyro[2];
		d_ready = 1;
	}
	der[0] = -(gyro[0] - prev_gyro[0]) / CTRL_DT;
	der[1] = -(gyro[1] - prev_gyro[1]) / CTRL_DT;
	der[2] = -(gyro[2] - prev_gyro[2]) / CTRL_DT;
	prev_gyro[0] = gyro[0];
	prev_gyro[1] = gyro[1];
	prev_gyro[2] = gyro[2];

	/* ---------- 安全链 (优先级从高到低) ---------- */

	/* 1. 急停 (锁存, 直到上锁清除) */
	if (emergency) {
		Motor_Stop();
		return;
	}

	/* 2. 未解锁 → 输出 0 */
	if (!armed) {
		Motor_Stop();
		return;
	}

	/* 2.5 传感器数据有效性: NaN/Inf 直接急停
	 * (NaN 与任何比较都是假, 若不拦截会绕过后面所有保护,
	 *  NaN 传到占空比转换属于未定义行为) */
	if (!isfinite(att->roll)  || !isfinite(att->pitch) ||
	    !isfinite(gyro[0])    || !isfinite(gyro[1])    || !isfinite(gyro[2]) ||
	    !isfinite(az_body)) {
		printf("[CTRL] Sensor data NaN/Inf -> EMERGENCY\r\n");
		Control_EmergencyStop();
		return;
	}

	/* 3. 姿态超限 → 停机 (持续 200ms 才动作, 单帧噪声不误杀;
	 *    200ms 窗口内控制律仍在尝试挽救) */
	if (!(fabsf(att->roll)  <= ANGLE_LIMIT_RAD &&
	      fabsf(att->pitch) <= ANGLE_LIMIT_RAD)) {
		if (att_over_ms == 0) {
			att_over_ms = now_ms ? now_ms : 1;
		} else if (now_ms - att_over_ms > ATT_LIMIT_HOLD_MS) {
			printf("[CTRL] ATT LIMIT R=%.1f P=%.1f\r\n",
			       att->roll * 57.2958f, att->pitch * 57.2958f);
			Control_EmergencyStop();
			return;
		}
	} else {
		att_over_ms = 0;
	}

	/* 4. 失控超时 (1s 无指令) → 进降落
	 *    仅从飞行态触发 (DISARMED 停在地面无指令属正常) */
	if (now_ms - last_cmd_ms > CMD_TIMEOUT_MS) {
		if (state == FLY_FOLLOW || state == FLY_HOVER) {
			printf("[CTRL] LOST CONTROL -> LANDING\r\n");
			state = FLY_LANDING;
		}
	}

	/* 5. HOVER 持续超时 → 自动缓降落地
	 *    (独立计时: HOVER 里油门必然是松的, 3s 到点自动降落;
	 *     不再与失控判据共用 last_cmd_ms —— 那样永远触发不了) */
	if (state == FLY_HOVER && now_ms - hover_enter_ms > HOVER_AUTO_LAND_MS) {
		state = FLY_LANDING;
	}

	/* ---------- 目标快照 (临界区防撕裂) ---------- */
	control_target_t t;
	taskENTER_CRITICAL();
	t = tgt;
	taskEXIT_CRITICAL();
	t.throttle = clampf(t.throttle, 0.0f, 1.0f);

	/* ---------- 状态机 + 油门 ---------- */
	UpdateStateMachine(&t);

	/* 油门为 0 (起飞前/落地后): 不跑 PID, 清积分
	 * (防止地面机身倾斜时角度误差把积分器慢慢充满) */
	if (actual_throttle <= 0.0f) {
		int_roll = int_pitch = int_yaw = 0.0f;
		Motor_Stop();
		return;
	}

	/* ---------- 串级 PID ---------- */

	/* 目标角限幅 (BLE 侧已校验, 这里兜底) */
	float tgt_r = clampf(t.target_roll,  -CTRL_ANGLE_MAX_RAD, CTRL_ANGLE_MAX_RAD);
	float tgt_p = clampf(t.target_pitch, -CTRL_ANGLE_MAX_RAD, CTRL_ANGLE_MAX_RAD);

	/* 角度环 (P): 姿态角误差 → 期望角速度 (带限幅) */
	float rate_r = clampf(pid.kp_angle * (tgt_r - att->roll), -RATE_MAX_RAD, RATE_MAX_RAD);
	float rate_p = clampf(pid.kp_angle * (tgt_p - att->pitch), -RATE_MAX_RAD, RATE_MAX_RAD);
	/* yaw 无角度外环 (Mahony 无磁力计, yaw 会漂移), 目标角速度直通 */
	float rate_y = clampf(t.target_yaw_rate, -RATE_MAX_RAD, RATE_MAX_RAD);

	/* 角速度误差 (rad/s) */
	float er[3];
	er[0] = rate_r - gyro[0];
	er[1] = rate_p - gyro[1];
	er[2] = rate_y - gyro[2];

	/* 积分: ×dt (SI 量纲), 输出限幅抗饱和 */
	float i_r = integral_out(&int_roll,  er[0], pid.ki_rate);
	float i_p = integral_out(&int_pitch, er[1], pid.ki_rate);
	float i_y = integral_out(&int_yaw,   er[2], pid.ki_rate);

	/* D 项: 开头已按测量值微分算好, 这里只乘增益 */
	/* 角速度环 PID → 归一化力矩 */
	float roll_corr  = pid.kp_rate * er[0] + i_r + pid.kd_rate * der[0];
	float pitch_corr = pid.kp_rate * er[1] + i_p + pid.kd_rate * der[1];
	float yaw_corr   = pid.kp_rate * er[2] + i_y + pid.kd_rate * der[2];

	/* ---------- X型混控 ---------- */
	Mixer_Apply(actual_throttle, roll_corr, pitch_corr, yaw_corr);
}
