/**
 * motor_pwm.h — 电机 PWM 驱动 (逻辑电机 ↔ 物理通道 宏映射)
 *
 * 物理 PWM: TIM3 CH1(PA6) CH2(PA7) CH3(PB0) CH4(PB1) @4kHz
 *
 * 逻辑电机编号 (混控矩阵定义, 见 control_logic.md):
 *   M1 = 前右 / M2 = 前左 / M3 = 后左 / M4 = 后右
 *
 * ★ PCB 上 PA6/PA7/PB0/PB1 接到哪个物理位置的电机,
 *   只需改下面 4 行宏, 混控代码不用动.
 */
#ifndef MOTOR_PWM_H
#define MOTOR_PWM_H

#include "stm32f4xx_hal.h"

/* ============================================================
 * 逻辑电机 → 物理 PWM 通道 映射宏
 * 根据 PCB 实际接线修改这里!
 * ============================================================ */
#define LOGICAL_M1_CHANNEL   TIM_CHANNEL_2   /* M1(前右)   ← PA7 (CH2) */
#define LOGICAL_M2_CHANNEL   TIM_CHANNEL_1   /* M2(前左)   ← PA6 (CH1) */
#define LOGICAL_M3_CHANNEL   TIM_CHANNEL_3   /* M3(后左)   ← PB0 (CH3) */
#define LOGICAL_M4_CHANNEL   TIM_CHANNEL_4   /* M4(后右)   ← PB1 (CH4) */

/**
 * @brief 初始化 TIM3 4 通道 PWM (4kHz), 上电输出 0%
 */
void Motor_Init(void);

/**
 * @brief 按逻辑电机编号设置占空比
 * @param m1..m4 对应 前右/前左/后左/后右, 范围 0.0~1.0
 */
void Motor_Set(float m1, float m2, float m3, float m4);

/** @brief 全部电机停止 */
void Motor_Stop(void);

/**
 * @brief 获取最近一拍 4 路电机输出 (Safety_Update 等记录用)
 * @param m 输出 [m1, m2, m3, m4], 限幅后占空比 0.0~1.0
 */
void Motor_GetLast(float m[4]);

#endif
