/**
 * battery.c — 电池电压检测实现 (自持 ADC1)
 *
 * PA1 = ADC1_IN1, 分压 2.5:1 (150K/100K)
 * V_bat = raw/4096 × VREF(3.3) × 2.5
 */
#include "battery.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>

static ADC_HandleTypeDef hadc1;

/* 分压比: (150K+100K)/100K = 2.5 */
#define DIVIDER_RATIO  2.5f
#define VREF           3.3f

void Battery_Init(void)
{
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_ADC1_CLK_ENABLE();

	/* PA1: 模拟输入 */
	GPIO_InitTypeDef gpio = {0};
	gpio.Pin  = GPIO_PIN_1;
	gpio.Mode = GPIO_MODE_ANALOG;
	gpio.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOA, &gpio);

	hadc1.Instance                   = ADC1;
	hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;
	hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
	hadc1.Init.ScanConvMode          = DISABLE;
	hadc1.Init.ContinuousConvMode    = DISABLE;
	hadc1.Init.DiscontinuousConvMode = DISABLE;
	hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
	hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
	hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
	hadc1.Init.NbrOfConversion       = 1;
	hadc1.Init.DMAContinuousRequests = DISABLE;
	hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
	if (HAL_ADC_Init(&hadc1) != HAL_OK) {
		printf("[BAT] ADC1 init FAILED\r\n");
		return;
	}

	ADC_ChannelConfTypeDef ch = {0};
	ch.Channel      = ADC_CHANNEL_1;   /* PA1 */
	ch.Rank         = 1;
	ch.SamplingTime = ADC_SAMPLETIME_84CYCLES;
	if (HAL_ADC_ConfigChannel(&hadc1, &ch) != HAL_OK) {
		printf("[BAT] ADC channel config FAILED\r\n");
	}
}

float Battery_GetVoltage(void)
{
	uint32_t raw = 0;
	uint8_t i;

	/* 多次采样取平均, 抑制噪声 */
	for (i = 0; i < 8; i++) {
		if (HAL_ADC_Start(&hadc1) != HAL_OK) return 0.0f;
		if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
			raw += HAL_ADC_GetValue(&hadc1);
		}
		HAL_ADC_Stop(&hadc1);
	}

	return (float)(raw / 8) / 4096.0f * VREF * DIVIDER_RATIO;
}

/* ADC1 时钟使能 (HAL_ADC_Init 回调) */
void HAL_ADC_MspInit(ADC_HandleTypeDef *h)
{
	if (h->Instance == ADC1) {
		__HAL_RCC_ADC1_CLK_ENABLE();
		__HAL_RCC_GPIOA_CLK_ENABLE();
	}
}
