/**
 * motor_pwm.c — 电机 PWM 驱动实现
 *
 * TIM3: APB1=50MHz → TIM CLK=100MHz
 * PWM 频率 = 100MHz / (ARR+1) = 4kHz  (ARR=24999, 0.01% 分辨率)
 */
#include "motor_pwm.h"
#include "stm32f4xx_hal.h"

static TIM_HandleTypeDef  htim3;

/* ============================================================
 * TIM3 PWM 初始化
 * ============================================================ */
void Motor_Init(void)
{
	__HAL_RCC_TIM3_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/* PA6/PA7 (CH1/CH2) */
	GPIO_InitTypeDef gpio = {0};
	gpio.Pin       = GPIO_PIN_6 | GPIO_PIN_7;
	gpio.Mode      = GPIO_MODE_AF_PP;
	gpio.Pull      = GPIO_NOPULL;
	gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
	gpio.Alternate = GPIO_AF2_TIM3;
	HAL_GPIO_Init(GPIOA, &gpio);

	/* PB0/PB1 (CH3/CH4) */
	gpio.Pin       = GPIO_PIN_0 | GPIO_PIN_1;
	HAL_GPIO_Init(GPIOB, &gpio);

	/* 4kHz PWM, 0.01% 分辨率 */
	htim3.Instance               = TIM3;
	htim3.Init.Prescaler         = 0;        /* 100MHz */
	htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
	htim3.Init.Period            = 24999;    /* 100M/25000 = 4kHz */
	htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
	htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
	HAL_TIM_PWM_Init(&htim3);

	/* 4 通道, 初始占空比 0 */
	TIM_OC_InitTypeDef oc = {0};
	oc.OCMode     = TIM_OCMODE_PWM1;
	oc.Pulse      = 0;
	oc.OCPolarity = TIM_OCPOLARITY_HIGH;
	oc.OCFastMode = TIM_OCFAST_DISABLE;
	HAL_TIM_PWM_ConfigChannel(&htim3, &oc, TIM_CHANNEL_1);
	HAL_TIM_PWM_ConfigChannel(&htim3, &oc, TIM_CHANNEL_2);
	HAL_TIM_PWM_ConfigChannel(&htim3, &oc, TIM_CHANNEL_3);
	HAL_TIM_PWM_ConfigChannel(&htim3, &oc, TIM_CHANNEL_4);

	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);
}

/* ============================================================
 * 占空比 clamp 到 [0, 1]
 * ============================================================ */
static inline uint32_t duty_to_ccr(float duty)
{
	if (duty < 0.0f) duty = 0.0f;
	if (duty > 1.0f) duty = 1.0f;
	return (uint32_t)(duty * 25000.0f);
}

/* ============================================================
 * 设置 4 路电机
 * ============================================================ */
void Motor_Set(float m1, float m2, float m3, float m4)
{
	__HAL_TIM_SET_COMPARE(&htim3, LOGICAL_M1_CHANNEL, duty_to_ccr(m1));
	__HAL_TIM_SET_COMPARE(&htim3, LOGICAL_M2_CHANNEL, duty_to_ccr(m2));
	__HAL_TIM_SET_COMPARE(&htim3, LOGICAL_M3_CHANNEL, duty_to_ccr(m3));
	__HAL_TIM_SET_COMPARE(&htim3, LOGICAL_M4_CHANNEL, duty_to_ccr(m4));
}

void Motor_Stop(void)
{
	Motor_Set(0, 0, 0, 0);
}
