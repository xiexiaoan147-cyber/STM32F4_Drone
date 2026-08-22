/**
 * safety.c — 整机安全保护实现
 */
#include "safety.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

Safety_t g_safety;

void Safety_Init(void) {
    memset(&g_safety, 0, sizeof(g_safety));
    g_safety.bat_threshold  = 6.4f;
    g_safety.angle_limit_deg = 60.0f;
    g_safety.cmd_timeout_ms  = 1000;
    g_safety.motor_threshold = 0.9f;
    g_safety.iwdg_reload_ms  = 1000;
    g_safety.armed = 0;
    g_safety.status = SAFE_OK;
    printf("[SAFETY] Init OK: %ums cmd timeout, %.1fV low-bat, %.0fdeg limit\r\n",
           g_safety.cmd_timeout_ms, g_safety.bat_threshold, g_safety.angle_limit_deg);
}

void Safety_SetArmed(int arm) {
    g_safety.armed = arm;
    printf("[SAFETY] %s\r\n", arm ? "ARMED" : "DISARMED");
}

void Safety_NotifyCmd(void) {
    g_safety.last_cmd_ms = HAL_GetTick();
}

void Safety_Update(float roll, float pitch, float yaw,
                   float m1, float m2, float m3, float m4,
                   float bat_voltage, uint32_t now_ms) {
    float roll_deg  = roll  * 57.2958f;
    float pitch_deg = pitch * 57.2958f;
    float yaw_deg   = yaw   * 57.2958f;

    /* 1、上电锁 — 不允许未解锁就输出高油门 */
    float max_motor = m1;
    if (m2 > max_motor) max_motor = m2;
    if (m3 > max_motor) max_motor = m3;
    if (m4 > max_motor) max_motor = m4;

    if (!g_safety.armed && max_motor > 0.1f) {
        g_safety.status = SAFE_ARM_FAIL;
        printf("[SAFETY] ARM FAIL: motor=%.0f%% without arming\r\n", max_motor * 100);
        return;
    }

    /* 2、姿态超限 */
    if (fabsf(roll_deg) > g_safety.angle_limit_deg ||
        fabsf(pitch_deg) > g_safety.angle_limit_deg) {
        if (g_safety.over_angle_start_ms == 0) {
            g_safety.over_angle_start_ms = now_ms;
        } else if (now_ms - g_safety.over_angle_start_ms > 200) {
            g_safety.status = SAFE_ATTITUDE_LIMIT;
            printf("[SAFETY] ATT LIMIT: R=%.1f P=%.1f\r\n", roll_deg, pitch_deg);
            return;
        }
    } else {
        g_safety.over_angle_start_ms = 0;
    }

    /* 3、失控保护 */
    if (g_safety.armed && now_ms - g_safety.last_cmd_ms > g_safety.cmd_timeout_ms) {
        g_safety.status = SAFE_LOST_CONTROL;
        printf("[SAFETY] LOST CONTROL: %lums no cmd\r\n",
               now_ms - g_safety.last_cmd_ms);
        return;
    }

    /* 4、电机堵转 */
    if (max_motor > g_safety.motor_threshold) {
        if (g_safety.stall_start_ms == 0) {
            g_safety.stall_start_ms = now_ms;
            g_safety.last_roll  = roll;
            g_safety.last_pitch = pitch;
            g_safety.last_yaw   = yaw;
        } else if (now_ms - g_safety.stall_start_ms > 3000) {
            float dr = fabsf(roll  - g_safety.last_roll);
            float dp = fabsf(pitch - g_safety.last_pitch);
            if (dr < 0.0175f && dp < 0.0175f) { /* <1°变化 */
                g_safety.status = SAFE_MOTOR_STALL;
                printf("[SAFETY] MOTOR STALL: 3s full throttle no movement\r\n");
                return;
            }
            g_safety.stall_start_ms = now_ms;
            g_safety.last_roll  = roll;
            g_safety.last_pitch = pitch;
        }
    } else {
        g_safety.stall_start_ms = 0;
    }

    /* 5、低电压 */
    if (bat_voltage > 0.1f && bat_voltage < g_safety.bat_threshold) {
        if (g_safety.bat_low_start_ms == 0) {
            g_safety.bat_low_start_ms = now_ms;
        } else if (now_ms - g_safety.bat_low_start_ms > 500) {
            g_safety.status = SAFE_LOW_BATTERY;
            printf("[SAFETY] LOW BAT: %.2fV\r\n", bat_voltage);
            return;
        }
    } else {
        g_safety.bat_low_start_ms = 0;
    }

    g_safety.status = SAFE_OK;
}

/* IWDG 独立看门狗 */
void IWDG_Init(uint32_t reload_ms) {
    /* LSI ~32KHz, 分频 32 → 1KHz tick */
    IWDG->KR  = 0x5555;           /* 解锁 PR */
    IWDG->PR  = 0x02;            /* /32 → ~1ms/tick */
    IWDG->RLR = reload_ms;       /* 重载值 */
    IWDG->KR  = 0xCCCC;          /* 启动 */
    IWDG->KR  = 0xAAAA;          /* 首次喂狗 */
    printf("[SAFETY] IWDG started: %lums timeout\r\n", reload_ms);
}

void IWDG_Feed(void) {
    IWDG->KR = 0xAAAA;
}
