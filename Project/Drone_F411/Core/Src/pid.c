/**
 * pid.c — PID 控制器实现
 */
#include "pid.h"
#include <math.h>

void PID_Init(PID_t *pid, float kp, float ki, float kd,
              float i_limit, float o_limit) 
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
    pid->integral_limit = i_limit;
    pid->output_limit = o_limit;
}

float PID_Update(PID_t *pid, float setpoint, float measured, float dt) 
{
    float error = setpoint - measured;

    /* 比例项 */
    float p_out = pid->kp * error;

    /* 积分项 (带抗饱和限幅) */
    pid->integral += error * dt;
    if (pid->integral_limit > 0.0f) 
	{
        if (pid->integral >  pid->integral_limit) pid->integral =  pid->integral_limit;
        if (pid->integral < -pid->integral_limit) pid->integral = -pid->integral_limit;
    }
    float i_out = pid->ki * pid->integral;

    /* 微分项 — 用误差变化率 (error - prev_error)，D 对抗变化起到阻尼作用 */
    if (dt <= 0.0f) 
	{
        return pid->prev_output;  /* dt 无效则不更新 */
    }
    float derivative = (error - pid->prev_error) / dt;
    pid->prev_error = error;
    float d_out = pid->kd * derivative;

    /* 合成输出 */
    float output = p_out + i_out + d_out;

    /* 输出限幅 */
    if (pid->output_limit > 0.0f) 
	{
        if (output >  pid->output_limit) { output =  pid->output_limit; }
        if (output < -pid->output_limit) { output = -pid->output_limit; }
    }

    pid->prev_output = output;
    return output;
}

void PID_Reset(PID_t *pid)
{
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
}
