/**
 * motor_virtual.c — 虚拟电机驱动
 * 模拟 X 型四轴布局，接收油门值计算推力/扭矩
 *
 * 电机布局 (X 型，从顶部俯视):
 *    M1 (前右, CW)     M2 (前左, CCW)
 *        \               /
 *          \     X     /
 *            \       /
 *             +-----+
 *            /       \
 *          /     X     \
 *        /               \
 *    M4 (后右, CCW)    M3 (后左, CW)
 */
#include "motor_virtual.h"

static float max_thrust   = 1.5f;   /* N */
static float torque_k     = 0.01f;  /* Nm/N */
static float arm_sin45    = 0.04596f; /* 0.065m × sin45° */
static float prev_m1 = 0, prev_m2 = 0, prev_m3 = 0, prev_m4 = 0;
static float motor_tau = 0.02f; /* 电机一阶响应时间常数 (s) */

void Motor_Virtual_Init(float mt, float al, float tk) {
    max_thrust = mt;
    torque_k   = tk;
    arm_sin45  = al * 0.707107f;
    prev_m1 = prev_m2 = prev_m3 = prev_m4 = 0.0f;
}

void Motor_Virtual_Set(const MotorOutput_t *thr, float dt,
                       float out_force[3], float out_torque[3]) {
    /* 一阶低通模拟电机响应 (实际电调有 ~20ms 延迟) */
    float alpha = dt / (motor_tau + dt);
    float m1 = prev_m1 + alpha * (thr->m1 - prev_m1);
    float m2 = prev_m2 + alpha * (thr->m2 - prev_m2);
    float m3 = prev_m3 + alpha * (thr->m3 - prev_m3);
    float m4 = prev_m4 + alpha * (thr->m4 - prev_m4);
    prev_m1 = m1; prev_m2 = m2; prev_m3 = m3; prev_m4 = m4;

    /* 推力 = 油门 * 最大推力 */
    float T1 = m1 * max_thrust;
    float T2 = m2 * max_thrust;
    float T3 = m3 * max_thrust;
    float T4 = m4 * max_thrust;

    /* 总推力 Z */
    out_force[0] = 0.0f;
    out_force[1] = 0.0f;
    out_force[2] = -(T1 + T2 + T3 + T4); /* 负 Z = 向上 */

    /* 力矩 (X 型布局, 机体坐标系 NED: x前 y右 z下, 见文件头布局图)
     *
     * 修复记录: 原 roll 公式 (T1-T2+T3-T4) 是"前右+后左-前左-后右"的
     * 对角差, 与自身布局图矛盾 (M3 是后左不是右侧) — 正确应为左右差;
     * yaw 的 M3/M4 符号也与声明的桨转向不符, 一并修正 */
    /* Roll (绕X): 左侧(M2前左+M3后左) - 右侧(M1前右+M4后右)
     * 左侧推力大 → 机体右滚 (+roll), 与 mixer 的 +R→M2/M3 加速一致 */
    float roll_t  = (T2 + T3 - T1 - T4) * arm_sin45;
    /* Pitch (绕Y): 前(M1+M2) - 后(M3+M4), 前桨加速 → 抬头 (+pitch) */
    float pitch_t = (T1 + T2 - T3 - T4) * arm_sin45;
    /* Yaw (绕Z): 逆桨 M2,M4 产生机头右偏(+yaw), 顺桨 M1,M3 相反,
     * 与 mixer 的 +Y→M2/M4 加速一致 */
    float yaw_t   = (T2 + T4 - T1 - T3) * torque_k;

    out_torque[0] = roll_t;
    out_torque[1] = pitch_t;
    out_torque[2] = yaw_t;
}
