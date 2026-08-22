/**
 * control.c — 飞控控制实现 (状态机 + 串级PID + X型混控 + 安全链)
 *
 * 每 5ms (200Hz):
 *   安全链 → 状态机/油门更新 → 串级PID → 混控 → 电机
 * 悬停: az 闭环 (松杆后微调油门直到垂直加速度=0)
 */
#include "control.h"
#include "mixer.h"
#include "motor_pwm.h"
#include <stdio.h>

/* ============================================================
 * 参数
 * ============================================================ */
static pid_params_t pid = 
{
	.kp_angle  = 6.0f,
	.kp_rate   = 0.15f,
	.ki_rate   = 0.01f,
	.kd_rate   = 0.005f,
};

/* 角速度环积分状态 */
static float int_roll  = 0.0f;
static float int_pitch = 0.0f;
static float int_yaw   = 0.0f;
static float prev_er[3] = {0, 0, 0};

/* 控制目标与状态 */
static control_target_t tgt;
static flight_state_t state = FLY_IDLE;
static uint8_t  armed = 0;
static uint8_t  emergency = 0;
static uint32_t last_cmd_ms = 0;
static uint32_t now_ms = 0;

/* 油门状态 */
static float actual_throttle = 0.0f;
static float az = 0.0f;

/* 限幅常量 */
#define RATE_MAX_DEG      200.0f
#define INT_LIMIT         0.15f
#define ANGLE_LIMIT_RAD   0.7854f   /* 45° */
#define CMD_TIMEOUT_MS    1000
#define THROTTLE_MIN      0.30f
#define THROTTLE_MAX      0.85f
#define AZ_GAIN           0.0002f     /* az → 油门 闭环增益 */
#define THROTTLE_ARM_MIN  0.05f       /* 油门低于此值判"松杆/锁定" */

/* ============================================================
 * 初始化
 * ============================================================ */
void Control_Init(void)
{
	tgt.throttle        = 0.0f;
	tgt.target_roll     = 0.0f;
	tgt.target_pitch    = 0.0f;
	tgt.target_yaw_rate = 0.0f;
	int_roll = int_pitch = int_yaw = 0;
	prev_er[0] = prev_er[1] = prev_er[2] = 0;
	actual_throttle = 0.0f;
	armed = 0;
	emergency = 0;
	state = FLY_IDLE;
	printf("[CTRL] Init OK\r\n");
}

void Control_SetPID(const pid_params_t *p) { pid = *p; }

void Control_SetTarget(const control_target_t *t)
{
	tgt = *t;
	last_cmd_ms = now_ms;
}

void Control_Armed(int arm)
{
	armed = arm;
	state = arm ? FLY_DISARMED : FLY_IDLE;
	printf("[CTRL] %s\r\n", arm ? "ARMED" : "DISARMED");
}

void Control_EmergencyStop(void)
{
	emergency = 1;
	state = FLY_IDLE;
	Motor_Stop();
	printf("[CTRL] EMERGENCY STOP!\r\n");
}

flight_state_t Control_GetState(void) { return state; }

/* ============================================================
 * 状态机 + 油门更新
 * ============================================================ */
static void UpdateStateMachine(void)
{
	switch (state) {

	case FLY_IDLE:
		actual_throttle = 0.0f;
		return;

	case FLY_DISARMED:
		/* 油门>5% → 进入跟随(起飞) */
		if (tgt.throttle > THROTTLE_ARM_MIN) {
			state = FLY_FOLLOW;
			actual_throttle = tgt.throttle;
		} else {
			actual_throttle = 0.0f;
		}
		return;

	case FLY_FOLLOW:
		if (tgt.throttle > THROTTLE_ARM_MIN) 
		{
			/* 推油门: 直接跟随 */
			actual_throttle = tgt.throttle;
		} else 
		{
			/* 松杆: 进入 az 闭环悬停 */
			state = FLY_HOVER;
		}
		return;

	case FLY_HOVER:
		if (tgt.throttle > THROTTLE_ARM_MIN) 
		{
			/* 再推油门: 回跟随 */
			state = FLY_FOLLOW;
			actual_throttle = tgt.throttle;
			return;
		}
		if (tgt.throttle < 0.01f && tgt.throttle > 0.0f) 
		{
			/* 极低油门持续 → 降落 (由主循环计时) */
		}
		/* az 闭环: az>0 减油门, az<0 加油门, 收敛到 az=0 */
		actual_throttle -= az * AZ_GAIN;
		if (actual_throttle < THROTTLE_MIN) actual_throttle = THROTTLE_MIN;
		if (actual_throttle > THROTTLE_MAX) actual_throttle = THROTTLE_MAX;
		return;

	case FLY_LANDING:
		/* 缓降 */
		actual_throttle -= 0.3f * 0.005f;
		if (actual_throttle < 0.0f) actual_throttle = 0.0f;
		return;

	default:
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

	/* ---------- 安全链 (优先级从高到低) ---------- */

	/* 1. 急停 */
	if (emergency) {
		Motor_Stop();
		return;
	}

	/* 2. 未解锁 → 输出 0 */
	if (!armed) {
		Motor_Stop();
		return;
	}

	/* 3. 姿态超限 → 停机 */
	if (att->roll  >  ANGLE_LIMIT_RAD ||
	    att->roll  < -ANGLE_LIMIT_RAD ||
	    att->pitch >  ANGLE_LIMIT_RAD ||
	    att->pitch < -ANGLE_LIMIT_RAD) {
		printf("[CTRL] ATT LIMIT R=%.1f P=%.1f\r\n",
		       att->roll * 57.2958f, att->pitch * 57.2958f);
		Control_EmergencyStop();
		return;
	}

	/* 4. 失控超时 (1s 无指令) → 进降落 */
	if (now_ms - last_cmd_ms > CMD_TIMEOUT_MS) {
		if (state != FLY_LANDING && state != FLY_IDLE) {
			printf("[CTRL] LOST CONTROL -> LANDING\r\n");
			state = FLY_LANDING;
		}
	}

	/* 5. HOVER 中油门持续 0 超过 3s → 降落 */
	if (state == FLY_HOVER && now_ms - last_cmd_ms > 3000) {
		state = FLY_LANDING;
	}

	/* ---------- 状态机 + 油门 ---------- */
	UpdateStateMachine();

	/* ---------- 串级PID ---------- */

	/* 角度环 (P): 姿态角误差 → 期望角速度 */
	float rate_r = pid.kp_angle * (tgt.target_roll  - att->roll);
	float rate_p = pid.kp_angle * (tgt.target_pitch - att->pitch);
	float rate_y = tgt.target_yaw_rate;

	/* 角速度误差 */
	float er[3];
	er[0] = rate_r - gyro[0];
	er[1] = rate_p - gyro[1];
	er[2] = rate_y - gyro[2];

	/* 积分 (抗饱和) */
	int_roll  += er[0];  int_pitch += er[1];  int_yaw += er[2];
	if (int_roll  >  INT_LIMIT) int_roll  =  INT_LIMIT;
	if (int_roll  < -INT_LIMIT) int_roll  = -INT_LIMIT;
	if (int_pitch >  INT_LIMIT) int_pitch =  INT_LIMIT;
	if (int_pitch < -INT_LIMIT) int_pitch = -INT_LIMIT;
	if (int_yaw   >  INT_LIMIT) int_yaw   =  INT_LIMIT;
	if (int_yaw   < -INT_LIMIT) int_yaw   = -INT_LIMIT;

	/* D 项 (测量值微分) */
	float der[3];
	der[0] = (er[0] - prev_er[0]) / 0.005f;
	der[1] = (er[1] - prev_er[1]) / 0.005f;
	der[2] = (er[2] - prev_er[2]) / 0.005f;
	prev_er[0] = er[0];  prev_er[1] = er[1];  prev_er[2] = er[2];

	/* 角速度环 PID → 归一化力矩 */
	float roll_corr  = pid.kp_rate * er[0] + pid.ki_rate * int_roll  + pid.kd_rate * der[0];
	float pitch_corr = pid.kp_rate * er[1] + pid.ki_rate * int_pitch + pid.kd_rate * der[1];
	float yaw_corr   = pid.kp_rate * er[2] + pid.ki_rate * int_yaw   + pid.kd_rate * der[2];

	/* ---------- X型混控 ---------- */
	Mixer_Apply(actual_throttle, roll_corr, pitch_corr, yaw_corr);
}
