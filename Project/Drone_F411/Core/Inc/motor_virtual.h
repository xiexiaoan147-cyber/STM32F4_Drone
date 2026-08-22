/**
 * motor_virtual.h — 虚拟无刷电机驱动 (模拟 BLDC + 电调)
 * 路线B仿真阶段替代真实电调，接收油门值并模拟电机响应
 */
#ifndef MOTOR_VIRTUAL_H
#define MOTOR_VIRTUAL_H

#include <stdint.h>

/* 电机油门结构体 (4 个电机) */
typedef struct {
    float m1;  /* 电机1 油门 0.0 ~ 1.0 */
    float m2;  /* 电机2 油门 0.0 ~ 1.0 */
    float m3;  /* 电机3 油门 0.0 ~ 1.0 */
    float m4;  /* 电机4 油门 0.0 ~ 1.0 */
} MotorOutput_t;

/**
 * @brief 初始化虚拟电机驱动
 * @param max_thrust 单电机最大推力 (N), 典型 1.5N
 * @param arm_length 电机到中心距离 (m), 典型 0.065m
 * @param torque_const 扭矩常数 (Nm/N), 典型 0.01
 */
void Motor_Virtual_Init(float max_thrust, float arm_length, float torque_const);

/**
 * @brief 设置电机油门并模拟物理响应
 * @param throttle 4 通道油门 0~1
 * @param dt       时间步长 (s)
 * @param out_force 输出合力 [fx,fy,fz] (N, 机体坐标系)
 * @param out_torque 输出合力矩 [tx,ty,tz] (Nm)
 */
void Motor_Virtual_Set(const MotorOutput_t *throttle, float dt,
                       float out_force[3], float out_torque[3]);

#endif /* MOTOR_VIRTUAL_H */
