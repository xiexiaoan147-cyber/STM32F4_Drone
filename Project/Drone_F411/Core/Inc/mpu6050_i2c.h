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

/* 注意括号: 避免宏展开优先级陷阱 (原来是 #define MPU_ADDR 0x68 << 1,
 * 若写成 MPU_ADDR | x 会错) */
#define MPU_ADDR ((uint8_t)(0x68 << 1))

/* IMU 数据结构体 */
typedef struct 
	{
	float ax, ay, az;       /* 加速度 (m/s^2) */
	float gx, gy, gz;       /* 角速度 (rad/s, 驱动已换算, 全链路 SI 单位) */
	float temp;             /* 温度 (°C) */
} imu_data_t;

int  MPU6050_Init(void);
int  MPU6050_Read(imu_data_t *out);

#endif
