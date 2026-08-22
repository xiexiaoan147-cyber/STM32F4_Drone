/**
 * pid.h — PID 控制器 (串级: 角度外环 + 角速度内环 + 高度环)
 */
#ifndef PID_H
#define PID_H

typedef struct {
    float kp, ki, kd;         /* PID 增益 */
    float integral;            /* 积分项 */
    float prev_error;          /* 上一次误差 (微分用) */
    float integral_limit;      /* 积分限幅 (抗饱和) */
    float output_limit;        /* 输出限幅 */
    float prev_output;         /* 上一次输出 (dt=0 时回退) */
} PID_t;

/**
 * @brief 初始化 PID 控制器
 * @param pid    PID 结构体指针
 * @param kp, ki, kd  比例/积分/微分增益
 * @param i_limit      积分限幅 (0 = 不限)
 * @param o_limit      输出限幅 (0 = 不限)
 */
void PID_Init(PID_t *pid, float kp, float ki, float kd,
              float i_limit, float o_limit);

/**
 * @brief 单次 PID 计算
 * @param pid     PID 结构体
 * @param setpoint 目标值
 * @param measured 测量值
 * @param dt       时间步长 (s)
 * @return 控制输出
 */
float PID_Update(PID_t *pid, float setpoint, float measured, float dt);

/**
 * @brief 重置 PID 积分项 (模式切换时调用)
 */
void PID_Reset(PID_t *pid);

#endif /* PID_H */
