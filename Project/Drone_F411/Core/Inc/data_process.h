/**
 * data_process.h — IMU 数据处理模块
 *
 * 流程: 原始数据 → 零偏校准 → 二阶低通滤波 → Mahony姿态解算 → 姿态角
 * 运行频率 500Hz (与 freertos.c 的 ImuRead/Attitude 任务同步)
 * 依赖: mpu6050_i2c.h (imu_data_t) / mahony.h (attitude_t)
 */
#ifndef DATA_PROCESS_H
#define DATA_PROCESS_H

#include "mpu6050_i2c.h"
#include "mahony.h"
#include <stdint.h>

/**
 * @brief 初始化数据处理 (需静止放置; 由 ImuRead 任务在调度器启动后调用)
 * @param calib_samples 零偏校准采样帧数 (250 = 0.5s @500Hz)
 */
void DataProcess_Init(uint32_t calib_samples);

/**
 * @brief 处理一帧 IMU 数据
 * @param raw  MPU6050 原始数据
 * @param att  输出姿态角 (rad, attitude_t)
 */
void DataProcess_Update(imu_data_t *raw, attitude_t *att);

/** 获取当前零偏 (调试用) */
void DataProcess_GetOffset(float *gyr_x, float *gyr_y, float *gyr_z);

/**
 * @brief 获取垂直加速度 (大地系 z 轴, 去掉重力, m/s²)
 *        >0 在上升, <0 在下落, =0 悬停
 */
float DataProcess_GetVerticalAccel(void);

/**
 * @brief 获取滤波后的角速度 (rad/s, 已减零偏)
 * @param gyr 输出 [gx, gy, gz]
 */
void DataProcess_GetGyro(float gyr[3]);

#endif
