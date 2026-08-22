/*
 *  imu_virtual.c — 虚拟 IMU 驱动 (模拟 MPU6050)
 *
 *  真硬件没到之前, 我们用这个虚拟驱动代替真正的 MPU6050,
 *  在仿真器里就能跑姿态解算算法。
 *
 *  模拟原理:
 *    用正弦波生成模拟的晃动 (roll/pitch ±5°, yaw ±3°),
 *    根据当前姿态计算重力在机体坐标系的分量 (模拟加速度计),
 *    再叠加高斯噪声来模拟真实传感器的"不完美"。
 *
 *  加速度计原理:
 *    静止时, 加速度计测量的是重力加速度 (9.81 m/s²)。
 *    如果飞机倾斜了, 重力在 XYZ 三个轴上的分量就会变化,
 *    通过这个分量就能推算出飞机的姿态角。
 *    所以加速度计能校准"水平" (roll 和 pitch)。
 *
 *  陀螺仪原理:
 *    陀螺仪测量的是角速度 (rad/s), 积分能得到角度。
 *    但积分会让小误差不断累积, 时间长了角度就会漂走。
 *    所以需要加速度计来定期"拉回来" — 这就是互补滤波的核心思想。
 *
 *  噪声模拟:
 *    Box-Muller 算法 — 把均匀分布的随机数变成高斯分布,
 *    让模拟数据更像真实传感器 (真实传感器的噪声就是高斯分布的)
 */

#include "imu_virtual.h"
#include <math.h>
#include <stdlib.h>

// ============================
// 内部状态
// ============================

static float noise_std = 0.01f;     // 噪声标准差 (越大数据越"脏")
static uint8_t init_done = 0;       // 是否已初始化

// ============================
// 高斯噪声生成 (Box-Muller)
// ============================
// 生成均值为 0、标准差为 noise_std 的高斯分布随机数
// 步骤:
//   1. 取两个 [0,1] 均匀随机数 u1, u2
//   2. sqrt(-2*ln(u1)) * cos(2π*u2) → 标准正态分布
//   3. 乘以 noise_std 得到指定标准差

static float gauss_noise(void)
{
    float u1 = (float)rand() / (float)RAND_MAX;
    float u2 = (float)rand() / (float)RAND_MAX;
    if (u1 < 0.0001f) u1 = 0.0001f;          // 防止 ln(0)
    return noise_std * sqrtf(-2.0f * logf(u1))
                     * cosf(2.0f * 3.14159265f * u2);
}

// ============================
// 初始化
// ============================

void IMU_Virtual_Init(float ns)
{
    noise_std = ns;
    srand(12345);   // 固定随机种子 — 这样每次仿真结果都一样, 方便调试
    init_done = 1;
}

// ============================
// 读取虚拟 IMU 数据
// ============================
// 每次调用生成一个 IMU_RawData_t, 模拟真实传感器的行为:
//
//  加速度:
//    重力在机体坐标系的分量 + 噪声
//    gx = g * sin(pitch)
//    gy = g * sin(roll) * cos(pitch)
//    gz = g * cos(roll) * cos(pitch)
//
//  角速度:
//    姿态角的导数 + 噪声
//    (roll/pitch/yaw 用不同频率的正弦波, 模拟真实晃动)
//
//  时间:
//    每次调用推进一步 (1ms), 用于生成平滑的连续数据

void IMU_Virtual_Read(IMU_RawData_t *data)
{
    if (!init_done) IMU_Virtual_Init(0.01f);

    static uint32_t t = 0;
    t++;
    float t_sec = (float)t * 0.001f;           // 时间 (秒)

    // ———— 模拟晃动姿态 (正弦波, 不同频率, ±5°~±3°) ————
    float roll_d  = 0.087f * sinf(2.0f * 3.14159f * 0.5f * t_sec);  // ±5°
    float pitch_d = 0.087f * sinf(2.0f * 3.14159f * 0.7f * t_sec);  // ±5°
    float yaw_d   = 0.052f * sinf(2.0f * 3.14159f * 0.3f * t_sec);  // ±3°

    // ———— 加速度计输出 (重力分量 + 噪声) ————
    float g = 9.81f;
    data->acc_x = g * sinf(pitch_d)                    + gauss_noise();
    data->acc_y = g * sinf(roll_d)  * cosf(pitch_d)    + gauss_noise();
    data->acc_z = g * cosf(roll_d)  * cosf(pitch_d)    + gauss_noise();

    // ———— 陀螺仪输出 (角度导数 + 噪声) ————
    float droll  = 0.087f * 2.0f * 3.14159f * 0.5f
                   * cosf(2.0f * 3.14159f * 0.5f * t_sec);
    float dpitch = 0.087f * 2.0f * 3.14159f * 0.7f
                   * cosf(2.0f * 3.14159f * 0.7f * t_sec);
    float dyaw   = 0.052f * 2.0f * 3.14159f * 0.3f
                   * cosf(2.0f * 3.14159f * 0.3f * t_sec);

    data->gyr_x = droll  + gauss_noise();
    data->gyr_y = dpitch + gauss_noise();
    data->gyr_z = dyaw   + gauss_noise();

    // ———— 温度 (随便模拟) ————
    data->temp = 25.0f + 0.5f * sinf(2.0f * 3.14159f * 0.05f * t_sec);
    data->timestamp_ms = t;
}
