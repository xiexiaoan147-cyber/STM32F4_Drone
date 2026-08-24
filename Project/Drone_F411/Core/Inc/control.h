/**
 * control.h — 飞控控制模块 (串级PID + 飞行状态机)
 *
 * 角度环(外环,P) → 期望角速度 → 角速度环(内环,PID) → 力矩 → X型混控
 * 飞行状态: IDLE/DISARMED/FOLLOW/HOVER/LANDING (见 flight_state_machine.md)
 */
#ifndef CONTROL_H
#define CONTROL_H

#include "data_process.h"
#include <stdint.h>

/* 飞行状态机 */
typedef enum {
	FLY_IDLE,       /* 上电 */
	FLY_DISARMED,   /* 已解锁待飞, 油门<5% 锁定 */
	FLY_FOLLOW,     /* 推油门: 直接跟随 */
	FLY_HOVER,      /* 松杆: az 闭环悬停 */
	FLY_LANDING     /* 收油门: 缓降 */
} flight_state_t;

/* 控制目标 (来自蓝牙指令解析) */
typedef struct {
	float throttle;        /* 油门 0.0~1.0 */
	float target_roll;     /* 目标横滚角 (rad, ±0.52) */
	float target_pitch;    /* 目标俯仰角 (rad, ±0.52) */
	float target_yaw_rate; /* 目标偏航角速度 (rad/s, ±3.14) */
} control_target_t;

/* PID 参数 */
typedef struct {
    float kp_angle;   /* 角度环 P */
    float kp_rate;    /* 角速度环 P */
    float ki_rate;    /* 角速度环 I, 单位 1/s (配合 ∫err·dt) */
    float kd_rate;    /* 角速度环 D (测量值微分) */
} pid_params_t;

/** @brief 初始化控制模块 */
void Control_Init(void);

/** @brief 设置 PID 参数 */
void Control_SetPID(const pid_params_t *p);

/** @brief 更新控制目标 (蓝牙指令每帧调用) */
void Control_SetTarget(const control_target_t *t);

/**
 * @brief 每 5ms 调用一次: 状态机 + 串级PID + 混控 + 电机输出
 * @param att     姿态角 (rad)
 * @param gyro    角速度 (rad/s, 滤波后已去零偏)
 * @param az_body 垂直加速度 (m/s², 去重力; >0 上升)
 */
void Control_Update(const attitude_t *att, const float gyro[3], float az_body);

/** @brief 解锁/上锁 (上锁会清除急停锁存并复位 PID 动态状态;
 *         急停激活期间拒绝解锁) */
void Control_Armed(int arm);

/** @brief 急停 */
void Control_EmergencyStop(void);

/** @brief 获取当前飞行状态 (调试/显示用) */
flight_state_t Control_GetState(void);

#endif
