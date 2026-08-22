/**
 * mixer.c — X 型混控实现
 *
 * 混控矩阵 (M1前右/M2前左/M3后左/M4后右):
 *   M1 = T + P - R - Y
 *   M2 = T + P + R + Y
 *   M3 = T - P + R - Y
 *   M4 = T - P - R + Y
 *
 * 输出限幅 [0,1], 通过 motor_pwm.h 宏映射到物理通道
 */
#include "mixer.h"
#include "motor_pwm.h"

static inline float clamp01(float x)
{
	if (x < 0.0f) return 0.0f;
	if (x > 1.0f) return 1.0f;
	return x;
}

void Mixer_Apply(float throttle, float roll_corr, float pitch_corr, float yaw_corr)
{
	/* 混控矩阵 */
	float m1 = throttle + pitch_corr - roll_corr - yaw_corr;
	float m2 = throttle + pitch_corr + roll_corr + yaw_corr;
	float m3 = throttle - pitch_corr + roll_corr - yaw_corr;
	float m4 = throttle - pitch_corr - roll_corr + yaw_corr;

	/* 限幅 0~1 */
	m1 = clamp01(m1);
	m2 = clamp01(m2);
	m3 = clamp01(m3);
	m4 = clamp01(m4);

	/* 输出 (宏映射到物理 PWM 通道) */
	Motor_Set(m1, m2, m3, m4);
}
