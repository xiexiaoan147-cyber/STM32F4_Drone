/* 主机端 control.c 逻辑回归测试 — 桩掉 HAL/FreeRTOS/混控, 纯逻辑验证
 *
 * 构建&运行:  cd Tools/host_test && ./run.sh
 * 不需要 arm 工具链, 用本机 gcc 即可
 */
#include <stdio.h>
#include <math.h>
#include "control.h"
#include "mixer.h"
#include "motor_pwm.h"

uint32_t g_ms = 0;
uint32_t HAL_GetTick(void) { return g_ms; }

/* 记录混控/电机输出 */
static float mT = -1, mR = 0, mP = 0, mY = 0;
static int   motors_stopped = 0;
void Mixer_Apply(float t, float r, float p, float y)
{ mT = t; mR = r; mP = p; mY = y; motors_stopped = 0; }
void Motor_Stop(void) { motors_stopped = 1; mT = 0; }

static attitude_t att = {0, 0, 0};
static float gyro[3] = {0, 0, 0};
static float azv = 0;
static int fails = 0;

#define CHECK(cond, msg) do { \
    if (cond) printf("  PASS  %s\n", msg); \
    else { printf("  FAIL  %s\n", msg); fails++; } } while (0)

static void frame(void) { g_ms += 5; Control_Update(&att, gyro, azv); }

static void send(float th, float r, float p, float y)
{
    control_target_t t = { .throttle = th, .target_roll = r,
                           .target_pitch = p, .target_yaw_rate = y };
    Control_SetTarget(&t);
}

int main(void)
{
    printf("== T1 上电/未解锁 ==\n");
    Control_Init();
    frame(); CHECK(motors_stopped, "未解锁: 电机停");

    printf("== T2 解锁后不推油门 ==\n");
    Control_Armed(1);
    CHECK(Control_GetState() == FLY_DISARMED, "进入 DISARMED");
    for (int i = 0; i < 100; i++) { send(0, 0, 0, 0); frame(); }
    CHECK(motors_stopped, "油门0: 不转 (积分清零路径)");

    printf("== T3 推油门起飞 → FOLLOW ==\n");
    for (int i = 0; i < 50; i++) { send(0.6f, 0, 0, 0); frame(); }
    CHECK(Control_GetState() == FLY_FOLLOW, "进入 FOLLOW");
    CHECK(fabsf(mT - 0.6f) < 0.01f, "油门直接跟随 0.6");

    printf("== T4 松杆 → HOVER + az 闭环 ==\n");
    for (int i = 0; i < 10; i++) { send(0, 0, 0, 0); frame(); }
    CHECK(Control_GetState() == FLY_HOVER, "进入 HOVER");
    float t0 = mT;
    azv = 5.0f;                        /* 持续上升 → 应减油门 */
    for (int i = 0; i < 100; i++) { send(0, 0, 0, 0); frame(); }
    CHECK(mT < t0 - 0.05f, "az>0: 油门下调");
    azv = 50.0f;                       /* 猛烈上升 → 压到下限 */
    for (int i = 0; i < 100; i++) { send(0, 0, 0, 0); frame(); }
    CHECK(fabsf(mT - 0.30f) < 0.001f, "HOVER 油门钳位 0.30 下限");
    azv = 0;

    printf("== T5 HOVER 3s → 自动 LANDING → 落地自动上锁 ==\n");
    int got_landing = 0; float tl = 0;
    for (int i = 0; i < 900 && !got_landing; i++) {
        send(0, 0, 0, 0); frame();
        if (Control_GetState() == FLY_LANDING) { got_landing = 1; tl = mT; }
    }
    CHECK(got_landing, "3s 后进 LANDING");
    for (int i = 0; i < 100; i++) { send(0, 0, 0, 0); frame(); }  /* 0.5s */
    CHECK(mT < tl, "降落中油门匀速下降");
    for (int i = 0; i < 400; i++) { send(0, 0, 0, 0); frame(); }
    CHECK(Control_GetState() == FLY_IDLE, "落地后自动上锁回 IDLE");
    CHECK(motors_stopped, "落地后电机停");

    printf("== T6 姿态超限去抖 + 急停恢复 ==\n");
    Control_Armed(1);
    for (int i = 0; i < 10; i++) { send(0.6f, 0, 0, 0); frame(); }
    att.roll = 0.9f;                   /* 51° 超限 */
    for (int i = 0; i < 30; i++) { send(0.6f, 0, 0, 0); frame(); }   /* 150ms */
    CHECK(!motors_stopped, "150ms 内不去抖触发 (控制律仍在挽救)");
    for (int i = 0; i < 20; i++) { send(0.6f, 0, 0, 0); frame(); }   /* +100ms */
    CHECK(motors_stopped, ">200ms 持续超限 → 急停");
    att.roll = 0;
    Control_Armed(1);
    for (int i = 0; i < 5; i++) { send(0.6f, 0, 0, 0); frame(); }
    CHECK(motors_stopped, "急停未解除: 拒绝解锁, 电机仍停");
    Control_Armed(0);                  /* 上锁清除急停 */
    Control_Armed(1);
    for (int i = 0; i < 5; i++) { send(0.6f, 0, 0, 0); frame(); }
    CHECK(!motors_stopped, "上锁→再解锁: 恢复正常起飞");

    printf("== T7 失控 1s → 缓降 ==\n");
    for (int i = 0; i < 250; i++) frame();  /* 1.25s 不发指令 */
    CHECK(Control_GetState() == FLY_LANDING, "1s 无指令 → LANDING");
    Control_Armed(0);

    printf("== T8 NaN 传感器数据 → 立即急停 ==\n");
    Control_Armed(1);
    for (int i = 0; i < 10; i++) { send(0.6f, 0, 0, 0); frame(); }
    att.roll = NAN;
    frame();
    CHECK(motors_stopped, "NaN 第一拍即急停 (原实现会绕过所有保护)");
    att.roll = 0;
    Control_Armed(0);

    printf("== T9 D 项方向 (测量微分, 无 derivative kick) ==\n");
    Control_Armed(1);
    for (int i = 0; i < 20; i++) { send(0.5f, 0, 0, 0); frame(); }
    send(0.5f, 0.1f, 0, 0); frame();   /* 突然给目标角 */
    float corr_step = mR;
    CHECK(fabsf(corr_step - 6.0f * 0.1f * 0.15f) < 0.02f,
          "目标角阶跃: 输出≈纯P (无D踢腿)");
    gyro[0] = 0.5f; frame();           /* 突然出现角速度 */
    float corr_gyro = mR;
    CHECK(corr_gyro < corr_step - 0.4f,
          "角速度阶跃: D 立即反向阻尼 (测量微分生效)");
    gyro[0] = 0;

    printf("== T10 I 项: 量纲正确 + 输出限幅 ==\n");
    gyro[0] = 0.05f;                   /* 恒定角速度误差 */
    for (int i = 0; i < 2000; i++) { send(0.5f, 0, 0, 0); frame(); }  /* 10s */
    CHECK(mR < -0.14f, "积分 10s 后 I 项达到 ±0.15 权限 (旧实现仅 0.0015)");
    gyro[0] = 0;
    Control_Armed(0);

    printf("== T11 KeepAlive: 非控制帧也算链路存活 ==\n");
    Control_Armed(1);
    for (int i = 0; i < 50; i++) { send(0.6f, 0, 0, 0); frame(); }
    CHECK(Control_GetState() == FLY_FOLLOW, "重新起飞 FOLLOW");
    for (int i = 0; i < 300; i++) {    /* 1.5s 只发 KeepAlive 不发目标 */
        if (i % 25 == 0) Control_KeepAlive();
        frame();
    }
    CHECK(Control_GetState() == FLY_FOLLOW, "有心跳不发目标: 不误判失控");
    Control_Armed(0);

    printf("\n========== 结果: %s (%d 失败) ==========\n",
           fails ? "有失败项" : "全部通过", fails);
    return fails ? 1 : 0;
}
