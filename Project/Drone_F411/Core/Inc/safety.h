/**
 * safety.h — 整机安全保护
 *
 * 保护机制：
 *   1. 低电压保护 — 电池 <6.4V 自动降落
 *   2. 姿态超限   — Roll/Pitch >60° 关电机
 *   3. 失控保护   — 指令超时 1s 锁高下降
 *   4. 电机堵转   — 高油门 3s 无姿态变化 → 停机
 *   5. 上电锁     — 油门非零时拒绝启动
 *   6. 看门狗     — IWDG 任务卡死复位
 */
#ifndef SAFETY_H
#define SAFETY_H

#include <stdint.h>

/* 安全状态枚举 */
typedef enum {
    SAFE_OK = 0,           /* 正常 */
    SAFE_LOW_BATTERY,      /* 低电压 */
    SAFE_ATTITUDE_LIMIT,   /* 姿态超限 */
    SAFE_LOST_CONTROL,     /* 失控 */
    SAFE_MOTOR_STALL,      /* 堵转 */
    SAFE_ARM_FAIL,         /* 上电锁拒绝 */
    SAFE_WATCHDOG          /* 看门狗复位 */
} SafetyStatus_t;

/* 安全监控上下文 */
typedef struct {
    /* 低电压 */
    float    bat_voltage;        /* 当前电池电压 (V) */
    float    bat_threshold;      /* 低电压阈值 (V), 默认 6.4V */
    uint32_t bat_low_start_ms;   /* 低电压开始时间 */

    /* 姿态超限 */
    float    angle_limit_deg;    /* 角度限制 (°), 默认 60° */
    uint32_t over_angle_start_ms;

    /* 失控保护 */
    uint32_t last_cmd_ms;        /* 最后一次收到指令时间 */
    uint32_t cmd_timeout_ms;     /* 指令超时 (ms), 默认 1000ms */

    /* 堵转检测 */
    float    motor_threshold;    /* 油门阈值, 默认 0.9 */
    uint32_t stall_start_ms;
    float    last_roll, last_pitch, last_yaw;

    /* 上电锁 */
    int      armed;              /* 解锁状态 */

    /* 看门狗 */
    uint32_t iwdg_reload_ms;     /* 喂狗间隔 (ms) */

    /* 当前状态 */
    SafetyStatus_t status;
} Safety_t;

/* 全局安全上下文 */
extern Safety_t g_safety;

void Safety_Init(void);
void Safety_SetArmed(int arm);
void Safety_NotifyCmd(void);     /* 蓝牙收到指令时调用 */
void Safety_Update(float roll, float pitch, float yaw,
                   float motor_m1, float motor_m2,
                   float motor_m3, float motor_m4,
                   float bat_voltage, uint32_t now_ms);

void IWDG_Init(uint32_t reload_ms);
void IWDG_Feed(void);

#endif
