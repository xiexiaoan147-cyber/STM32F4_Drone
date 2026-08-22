/**
 * data_process.c — IMU 数据处理实现
 *
 * 1. 零偏校准: 静止采样 N 帧求陀螺仪均值
 * 2. 二阶低通滤波: 级联两级一阶 IIR (alpha=0.3)
 * 3. 姿态解算: Mahony 互补滤波 (200Hz, 四元数内部表示)
 * 4. 垂直加速度: 大地系 z 轴去重力 (az 闭环用)
 */
#include "data_process.h"
#include "mahony.h"
#include <stdio.h>

/* 二阶低通: 每级 alpha=0.3 (等效截止 ~45Hz @500Hz 采样) */
#define LPF_ALPHA  0.3f
#define GRAVITY    9.81f

/* 内部状态 */
static float gyr_bias[3];                /* 陀螺仪零偏 */
static float acc_lpf1[3], acc_lpf2[3];   /* 加速度 二阶滤波缓存 (级1/级2) */
static float gyr_lpf1[3], gyr_lpf2[3];   /* 角速度 二阶滤波缓存 (级1/级2) */
static float vert_accel = 0.0f;          /* 垂直加速度 (大地系z, 去重力) */
static uint8_t calib_done = 0;

/* ============================================================
 * 零偏校准: 静止放置, 采样 samples 帧求平均
 * ============================================================ */
void DataProcess_Init(uint32_t calib_samples)
{
	imu_data_t raw;
	float sum_gx = 0, sum_gy = 0, sum_gz = 0;
	uint32_t i;

	printf("[CALIB] Hold still... (%lu samples)\r\n", calib_samples);

	for (i = 0; i < calib_samples; i++) {
		if (MPU6050_Read(&raw) == 0) {
			sum_gx += raw.gx;
			sum_gy += raw.gy;
			sum_gz += raw.gz;
		}
		HAL_Delay(2);   /* 2ms = 500Hz 同步 */
	}

	gyr_bias[0] = sum_gx / calib_samples;
	gyr_bias[1] = sum_gy / calib_samples;
	gyr_bias[2] = sum_gz / calib_samples;

	/* 初始化滤波缓存和 Mahony */
	for (int i = 0; i < 3; i++) {
		acc_lpf1[i] = acc_lpf2[i] = 0;
		gyr_lpf1[i] = gyr_lpf2[i] = 0;
	}
	Mahony_Init(200.0f, 0.5f, 0.05f);

	calib_done = 1;
	printf("[CALIB] Done. Gyro bias: %.2f %.2f %.2f deg/s\r\n",
	       gyr_bias[0], gyr_bias[1], gyr_bias[2]);
}

/* ============================================================
 * 一帧处理: 减零偏 → 二阶低通 → Mahony → 垂直加速度
 * ============================================================ */
void DataProcess_Update(imu_data_t *raw, attitude_t *att)
{
	if (!calib_done) {
		DataProcess_Init(200);
		return;
	}

	/* 1. 减零偏 */
	float gx = raw->gx - gyr_bias[0];
	float gy = raw->gy - gyr_bias[1];
	float gz = raw->gz - gyr_bias[2];

	/* 2. 二阶低通滤波 (级联两级一阶 IIR) */
	/* 第 1 级 */
	acc_lpf1[0] = LPF_ALPHA * raw->ax + (1.0f - LPF_ALPHA) * acc_lpf1[0];
	acc_lpf1[1] = LPF_ALPHA * raw->ay + (1.0f - LPF_ALPHA) * acc_lpf1[1];
	acc_lpf1[2] = LPF_ALPHA * raw->az + (1.0f - LPF_ALPHA) * acc_lpf1[2];

	gyr_lpf1[0] = LPF_ALPHA * gx + (1.0f - LPF_ALPHA) * gyr_lpf1[0];
	gyr_lpf1[1] = LPF_ALPHA * gy + (1.0f - LPF_ALPHA) * gyr_lpf1[1];
	gyr_lpf1[2] = LPF_ALPHA * gz + (1.0f - LPF_ALPHA) * gyr_lpf1[2];

	/* 第 2 级 */
	acc_lpf2[0] = LPF_ALPHA * acc_lpf1[0] + (1.0f - LPF_ALPHA) * acc_lpf2[0];
	acc_lpf2[1] = LPF_ALPHA * acc_lpf1[1] + (1.0f - LPF_ALPHA) * acc_lpf2[1];
	acc_lpf2[2] = LPF_ALPHA * acc_lpf1[2] + (1.0f - LPF_ALPHA) * acc_lpf2[2];

	gyr_lpf2[0] = LPF_ALPHA * gyr_lpf1[0] + (1.0f - LPF_ALPHA) * gyr_lpf2[0];
	gyr_lpf2[1] = LPF_ALPHA * gyr_lpf1[1] + (1.0f - LPF_ALPHA) * gyr_lpf2[1];
	gyr_lpf2[2] = LPF_ALPHA * gyr_lpf1[2] + (1.0f - LPF_ALPHA) * gyr_lpf2[2];

	/* 3. Mahony 姿态解算 (固定 dt=5ms @200Hz) */
	Mahony_Update(gyr_lpf2[0], gyr_lpf2[1], gyr_lpf2[2],
		      acc_lpf2[0],   acc_lpf2[1],   acc_lpf2[2],
		      0.005f, att);

	/* 4. 垂直加速度 (大地系 z, 去重力)
	 *    用四元数把机体系加速度转到大地系, 取 z 分量减 g */
	float q0, q1, q2, q3;
	Mahony_GetQuat(&q0, &q1, &q2, &q3);

	/* 旋转矩阵第三行: 大地系 z 分量 */
	float a_wz = 2.0f * (q0 * q2 + q1 * q3) * acc_lpf2[0]
		   + 2.0f * (q2 * q3 - q0 * q1) * acc_lpf2[1]
		   + (q0 * q0 - q1 * q1 - q2 * q2 + q3 * q3) * acc_lpf2[2];

	vert_accel = a_wz - GRAVITY;   /* >0 上升, <0 下落 */
}

/* ============================================================
 * 获取垂直加速度
 * ============================================================ */
float DataProcess_GetVerticalAccel(void)
{
	return vert_accel;
}

/* ============================================================
 * 获取滤波后角速度 (rad/s, 已减零偏)
 * ============================================================ */
void DataProcess_GetGyro(float gyr[3])
{
	gyr[0] = gyr_lpf2[0];
	gyr[1] = gyr_lpf2[1];
	gyr[2] = gyr_lpf2[2];
}

/* ============================================================
 * 获取零偏 (调试)
 * ============================================================ */
void DataProcess_GetOffset(float *gx, float *gy, float *gz)
{
	*gx = gyr_bias[0];
	*gy = gyr_bias[1];
	*gz = gyr_bias[2];
}
