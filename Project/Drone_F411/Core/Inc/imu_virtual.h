/**
 * imu_virtual.h — 虚拟 IMU 驱动 (模拟 MPU6050)
 * 用于路线B先软件后硬件的仿真阶段，无需真实传感器
 */
#ifndef IMU_VIRTUAL_H
#define IMU_VIRTUAL_H

#include <stdint.h>

/* IMU 原始数据结构体 */
typedef struct {
    float acc_x, acc_y, acc_z;   /* 加速度 (m/s²) */
    float gyr_x, gyr_y, gyr_z;   /* 角速度  (rad/s) */
    float temp;                   /* 温度     (°C)   */
    uint32_t timestamp_ms;        /* 时间戳   (ms)   */
} IMU_RawData_t;

/**
 * @brief 初始化虚拟 IMU（设置初始姿态和噪声参数）
 * @param noise_std 传感器噪声标准差 (典型 0.01)
 */
void IMU_Virtual_Init(float noise_std);

/**
 * @brief 读取虚拟 IMU 数据（生成模拟的加速度+角速度）
 * @param data 输出数据结构体指针
 * @note  基于正弦波模拟小幅度晃动，叠加高斯噪声
 */
void IMU_Virtual_Read(IMU_RawData_t *data);

#endif /* IMU_VIRTUAL_H */
