/**
 * mpu6050_i2c.c — MPU6050 I2C 驱动实现
 *
 * I2C1: PB8(SCL,AF4) / PB9(SDA,AF4) @ 400kHz
 * 设备地址: 0x68 << 1 = 0xD0
 */
#include "mpu6050_i2c.h"

I2C_HandleTypeDef hi2c1;

/* ---- I2C1 初始化 ---- */
void I2C1_Init(void)
{
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_I2C1_CLK_ENABLE();

	GPIO_InitTypeDef gpio = {0};
	gpio.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
	gpio.Mode      = GPIO_MODE_AF_OD;
	gpio.Pull      = GPIO_PULLUP;
	gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
	gpio.Alternate = GPIO_AF4_I2C1;
	HAL_GPIO_Init(GPIOB, &gpio);

	hi2c1.Instance             = I2C1;
	hi2c1.Init.ClockSpeed      = 400000;
	hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_2;
	hi2c1.Init.OwnAddress1     = 0;
	hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
	hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
	HAL_I2C_Init(&hi2c1);
}

/* ---- 写单个寄存器 ---- */
static int MPU6050_WriteReg(uint8_t reg, uint8_t val)
{
	return HAL_I2C_Mem_Write(&hi2c1, MPU_ADDR, reg,
				 I2C_MEMADD_SIZE_8BIT, &val, 1, 10);
}

/* ---- 读多个寄存器 ---- */
static int MPU6050_ReadRegs(uint8_t reg, uint8_t *buf, uint8_t len)
{
	return HAL_I2C_Mem_Read(&hi2c1, MPU_ADDR, reg,
				I2C_MEMADD_SIZE_8BIT, buf, len, 50);
}

/* ---- MPU6050 初始化 ---- */
int MPU6050_Init(void)
{
	uint8_t id;

	/* 检查 WHO_AM_I (0x75) 是否返回 0x68 */
	if (MPU6050_ReadRegs(0x75, &id, 1) != HAL_OK || id != 0x68)
		return -1;

	/* 复位设备 */
	MPU6050_WriteReg(0x6B, 0x80);  /* PWR_MGMT_1: DEVICE_RESET */
	HAL_Delay(100);
	MPU6050_WriteReg(0x6B, 0x00);  /* PWR_MGMT_1: 唤醒, 时钟=内部8MHz */

	/* 陀螺仪 ±250°/s */
	MPU6050_WriteReg(0x1B, 0x00);  /* GYRO_CONFIG: FS_SEL=0 */

	/* 加速度计 ±2g */
	MPU6050_WriteReg(0x1C, 0x00);  /* ACCEL_CONFIG: AFS_SEL=0 */

	/* 低通滤波 44Hz (DLPF=3) */
	MPU6050_WriteReg(0x1A, 0x03);  /* CONFIG: DLPF_CFG=3 */

	/* 采样率 1kHz (SMPLRT_DIV=0) */
	MPU6050_WriteReg(0x19, 0x00);

	return 0;
}

/* ---- 读取加速度 + 角速度 + 温度 ---- */
int MPU6050_Read(imu_data_t *out)
{
	uint8_t buf[14];
	if (MPU6050_ReadRegs(0x3B, buf, 14) != HAL_OK)
		return -1;

	int16_t ax_raw = (buf[0]  << 8) | buf[1];
	int16_t ay_raw = (buf[2]  << 8) | buf[3];
	int16_t az_raw = (buf[4]  << 8) | buf[5];
	int16_t t_raw  = (buf[6]  << 8) | buf[7];
	int16_t gx_raw = (buf[8]  << 8) | buf[9];
	int16_t gy_raw = (buf[10] << 8) | buf[11];
	int16_t gz_raw = (buf[12] << 8) | buf[13];

	out->ax   = ax_raw / 16384.0f * 9.81f;   /* ±2g → 16384 LSB/g */
	out->ay   = ay_raw / 16384.0f * 9.81f;	 //*9.81进行单位转换9.81m/s2=1g
	out->az   = az_raw / 16384.0f * 9.81f;
	
	/* 陀螺仪输出统一为 rad/s (全链路 SI 单位) */
	out->gx   = gx_raw / 131.0f * 0.01745329f;   /* ±250°/s → 131 LSB/° → rad/s */
	out->gy   = gy_raw / 131.0f * 0.01745329f;
	out->gz   = gz_raw / 131.0f * 0.01745329f;
	out->temp = t_raw / 340.0f + 36.53f;     /* 温度公式 */

	return 0;
}
