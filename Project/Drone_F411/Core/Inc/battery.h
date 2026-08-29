/**
 * battery.h — 电池电压检测模块 (自持 ADC1)
 *
 * 硬件: PA1 / ADC1_IN1, 分压 R1=150K + R2=100K (2.5:1)
 *       V_bat = V_adc × 2.5, 满电 4.2V → 1.68V < 3.3V
 */
#ifndef BATTERY_H
#define BATTERY_H

#include <stdint.h>

/** @brief ADC1 初始化 (PA1 模拟输入 + 单通道轮询) */
void Battery_Init(void);

/**
 * @brief 读取电池电压 (V)
 * @return 电压值, 失败返回 0
 */
float Battery_GetVoltage(void);

#endif
