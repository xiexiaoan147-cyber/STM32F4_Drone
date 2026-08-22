/**
 * mixer.h — X 型四轴混控
 *
 * 布局: M1前右 / M2前左 / M3后左 / M4后右 (见 control_logic.md)
 * 物理通道映射由 motor_pwm.h 的 LOGICAL_Mx_CHANNEL 宏完成
 */
#ifndef MIXER_H
#define MIXER_H

/**
 * @brief 混控: 油门 + 三轴力矩 → 4 路电机占空比并输出
 * @param throttle   基础油门 [0,1]
 * @param roll_corr  roll 力矩 [-1,1]
 * @param pitch_corr pitch 力矩 [-1,1]
 * @param yaw_corr   yaw 力矩 [-1,1]
 */
void Mixer_Apply(float throttle, float roll_corr, float pitch_corr, float yaw_corr);

#endif
