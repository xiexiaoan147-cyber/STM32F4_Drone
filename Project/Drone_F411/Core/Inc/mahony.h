/**
 * mahony.h — Mahony 互补滤波姿态解算
 * 适用于 IMU (加速度计+陀螺仪) 6轴传感器
 */
#ifndef MAHONY_H
#define MAHONY_H

#include <stdint.h>

/* 姿态角数据结构体 */
typedef struct {
    float roll;    /* 横滚角 (rad), -PI ~ +PI */
    float pitch;   /* 俯仰角 (rad), -PI/2 ~ +PI/2 */
    float yaw;     /* 偏航角 (rad), -PI ~ +PI */
} attitude_t;

/**
 * @brief 初始化 Mahony 滤波器
 * @param sample_freq_hz 采样频率 (典型 200Hz)
 * @param kp             比例增益 (典型 0.5f，越大收敛越快但噪声越大)
 * @param ki             积分增益 (典型 0.0f，用于消除陀螺零偏)
 */
void Mahony_Init(float sample_freq_hz, float kp, float ki);

/**
 * @brief 更新 Mahony 滤波器（每次 IMU 数据到来时调用）
 * @param gx, gy, gz  陀螺仪角速度 (rad/s)
 * @param ax, ay, az  加速度计测量值 (m/s²)
 * @param dt          采样间隔 (s)
 * @param out          输出姿态角 (rad)
 */
void Mahony_Update(float gx, float gy, float gz,
                   float ax, float ay, float az,
                   float dt,
                   attitude_t *out);

/**
 * @brief 将四元数转换为欧拉角
 * @param q0,q1,q2,q3 四元数
 * @param out          输出欧拉角 (rad)
 */
void Mahony_QuatToEuler(float q0, float q1, float q2, float q3, attitude_t *out);

/**
 * @brief 获取当前四元数 (供垂直加速度计算等使用)
 */
void Mahony_GetQuat(float *q0, float *q1, float *q2, float *q3);

#endif /* MAHONY_H */
