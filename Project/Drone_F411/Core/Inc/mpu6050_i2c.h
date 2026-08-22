/**
 * mpu6050_i2c.h — MPU6050 I2C 驱动
 *
 * 引脚: PB8(I2C1_SCL, AF4) / PB9(I2C1_SDA, AF4)
 * 速度: 400kHz Fast Mode
 * 地址: 0x68 (AD0=GND)
 */
#ifndef MPU6050_I2C_H
#define MPU6050_I2C_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

#define MPU_ADDR 0x68 << 1
/* IMU 数据结构体 */
typedef struct 
	{
	float ax, ay, az;       /* 加速度 (m/s^2) */
	float gx, gy, gz;       /* 角速度 (deg/s) */
	float temp;             /* 温度 (°C) */
} imu_data_t;

extern I2C_HandleTypeDef hi2c1;

void I2C1_Init(void);
int  MPU6050_Init(void);
int  MPU6050_Read(imu_data_t *out);

#endif
