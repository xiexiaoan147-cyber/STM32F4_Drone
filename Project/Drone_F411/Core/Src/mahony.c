/*
 *  mahony.c — Mahony 互补滤波姿态解算
 *
 *  为什么需要姿态解算?
 *    陀螺仪积分 → 高频好, 低频漂移 (像用"步数"推算位置, 越走越偏)
 *    加速度计     → 低频好, 高频噪声大 (像 GPS, 静止很准, 一动就抖)
 *    互补滤波     → 取各自优点: 陀螺仪负责快速变化, 加速度计负责长期修正
 *
 *  核心思想 (Mahony 的优雅之处):
 *    1. 用陀螺仪角速度乘以 dt, 更新四元数 (核心积分)
 *    2. 用加速度计算出"重力应该在哪个方向"
 *    3. 拿"实际重力方向" 和 "估计的重力方向" 做叉积 → 得到误差
 *    4. 把误差通过 PI 控制器反馈到陀螺仪角速度上 → 既修正了漂移, 又保持了动态响应
 *
 *  为什么用四元数而不是欧拉角?
 *    欧拉角有"万向节死锁"问题 (pitch=90°时 roll 和 yaw 重叠),
 *    四元数没有这个问题, 而且计算量更小 (只做乘法和加法)
 *
 *  参考文献:
 *    Mahony, R., Hamel, T., & Pflimlin, J. M. (2008).
 *    "Nonlinear Complementary Filters on the Special Orthogonal Group."
 *    IEEE Transactions on Automatic Control, 53(5), 1203-1218.
 *
 *  FPU 注意:
 *    所有运算都用 float + 硬件 FPU (STM32F411 的 Cortex-M4F),
 *    单精度对姿态解算足够 (精度 ~7 位有效数字)
 */

#include "mahony.h"
#include <math.h>

// ============================
// 滤波器参数
// ============================

static float twoKp;  // 2 × 比例增益 (越大收敛越快, 噪声也越大, 典型 0.5~2.0)
static float twoKi;  // 2 × 积分增益 (消除陀螺零偏, 通常设 0)

// PI 控制器的积分累积项 (消除陀螺仪的长期漂移)
static float integralFBx = 0.0f;
static float integralFBy = 0.0f;
static float integralFBz = 0.0f;

// 姿态四元数 (初始为水平放置, 机头朝北)
// q0 是实部, q1/q2/q3 是虚部 (代表绕 X/Y/Z 轴的旋转)
static float q0 = 1.0f, q1 = 0.0f, q2 = 0.0f, q3 = 0.0f;

// ============================
// 初始化
// ============================
// sample_freq_hz: 采样频率 (比如 200Hz)
// kp:             比例增益 (0.5 是保守值, 适合有噪声的数据)
// ki:             积分增益 (0.0 表示不用积分修正, 简单场景够用)

void Mahony_Init(float sample_freq_hz, float kp, float ki)
{
    (void)sample_freq_hz;   // 预留给以后用 (自动调整 dt)
    twoKp = 2.0f * kp;
    twoKi = 2.0f * ki;

    // 重置所有状态
    integralFBx = integralFBy = integralFBz = 0.0f;
    q0 = 1.0f; q1 = q2 = q3 = 0.0f;
}

// ============================
// 更新滤波器 (每次 IMU 数据到来时调用)
// ============================
// gx/gy/gz: 陀螺仪角速度 (rad/s)
// ax/ay/az: 加速度计测量值 (m/s²)
// dt:       距离上次更新的时间间隔 (秒)
// out:      输出欧拉角 roll/pitch/yaw (rad)

void Mahony_Update(float gx, float gy, float gz,
                   float ax, float ay, float az,
                   float dt,
                   attitude_t *out)
{
    float recipNorm;
    float halfvx, halfvy, halfvz;
    float halfex, halfey, halfez;

    // ———— 1、加速度计修正 (仅在数据有效时进行) ————
    // 加速度计模长应该在 1g (9.81 m/s²) 附近,
    // 如果偏离太多 (>20 或 <0.5), 说明飞机在剧烈加速/自由落体,
    // 这时候加速度计数据不可信, 跳过修正
    float acc_norm = sqrtf(ax * ax + ay * ay + az * az);
    if(acc_norm > 0.5f && acc_norm < 20.0f)
    {

        // 归一化 → 变成单位向量 (方向对了就行, 长度不重要)
        recipNorm = 1.0f / acc_norm;
        ax *= recipNorm;
        ay *= recipNorm;
        az *= recipNorm;

        // 根据当前四元数, 推算"重力应该在哪"
        // 这是四元数空间旋转公式的简化版:
        //   把 [0,0,1] (世界坐标系的"下") 旋转到机体坐标系
        halfvx = q1 * q3 - q0 * q2;
        halfvy = q0 * q1 + q2 * q3;
        halfvz = q0 * q0 - 0.5f + q3 * q3;

        // 叉积 → 得到"实际重力"和"估计重力"之间的误差
        // 这个误差告诉滤波器: 你的姿态估计偏了多少
        halfex = (ay * halfvz - az * halfvy);
        halfey = (az * halfvx - ax * halfvz);
        halfez = (ax * halfvy - ay * halfvx);

        // ———— PI 控制器 ————
        // 积分项: 累积长期误差, 消除陀螺零偏
        if(twoKi > 0.0f)
        {
            integralFBx += twoKi * halfex * dt;
            integralFBy += twoKi * halfey * dt;
            integralFBz += twoKi * halfez * dt;
            gx += integralFBx;
            gy += integralFBy;
            gz += integralFBz;
        }
        else
        {
            integralFBx = integralFBy = integralFBz = 0.0f;
        }

        // 比例项: 立即响应误差, 把姿态"拽回来"
        gx += twoKp * halfex;
        gy += twoKp * halfey;
        gz += twoKp * halfez;
    }

    // ———— 2、四元数积分 (一阶龙格库塔) ————
    // 把角速度乘以 dt, 用来更新四元数:
    //   q' = q + 0.5 * dt * ω ⊗ q
    // 其中 ⊗ 是四元数乘法 (不交换, 顺序很重要!)
    gx *= (0.5f * dt);
    gy *= (0.5f * dt);
    gz *= (0.5f * dt);

    float qa = q0, qb = q1, qc = q2, qd = q3;
    q0 += (-qb * gx - qc * gy - qd * gz);
    q1 += ( qa * gx + qd * gy - qc * gz);
    q2 += ( qd * gx - qa * gy + qb * gz);
    q3 += (-qc * gx + qb * gy + qa * gz);

    // ———— 3、归一化 ————
    // 数值误差会让四元数长度逐渐偏离 1, 必须定期归一化
    float q_norm = sqrtf(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    if(q_norm > 1e-10f)
    {
        recipNorm = 1.0f / q_norm;
        q0 *= recipNorm;
        q1 *= recipNorm;
        q2 *= recipNorm;
        q3 *= recipNorm;
    }

    // ———— 4、四元数 → 欧拉角 ————
    Mahony_QuatToEuler(q0, q1, q2, q3, out);
}

// ============================
// 四元数 → 欧拉角
// ============================
// 标准转换公式 (Z-Y-X 旋转顺序, 即偏航→俯仰→横滚):
//
//   roll  = atan2( 2(q0·q1 + q2·q3), 1 - 2(q1² + q2²) )
//   pitch = asin(  2(q0·q2 - q3·q1) )
//   yaw   = atan2( 2(q0·q3 + q1·q2), 1 - 2(q2² + q3²) )
//
// 注意 asin 的参数范围: 如果 |x| > 1, 夹到 ±π/2 (防止浮点溢出)

void Mahony_QuatToEuler(float q0, float q1, float q2, float q3, attitude_t *out)
{
    // Roll (横滚) — 机身绕 X 轴旋转, 左低右高为正
    float sinr_cosp = 2.0f * (q0 * q1 + q2 * q3);
    float cosr_cosp = 1.0f - 2.0f * (q1 * q1 + q2 * q2);
    out->roll = atan2f(sinr_cosp, cosr_cosp);

    // Pitch (俯仰) — 机身绕 Y 轴旋转, 抬头为正
    float sinp = 2.0f * (q0 * q2 - q3 * q1);
    if(fabsf(sinp) >= 1.0f)
    {
        // 万向节死锁边界: pitch = ±90°, 卡住
        out->pitch = copysignf(3.14159265f / 2.0f, sinp);
    } else {
        out->pitch = asinf(sinp);
    }

    // Yaw (偏航) — 机头绕 Z 轴旋转, 右偏为正
    float siny_cosp = 2.0f * (q0 * q3 + q1 * q2);
    float cosy_cosp = 1.0f - 2.0f * (q2 * q2 + q3 * q3);
    out->yaw = atan2f(siny_cosp, cosy_cosp);
}

/* ============================================================
 * 获取当前四元数
 * ============================================================ */
void Mahony_GetQuat(float *q0o, float *q1o, float *q2o, float *q3o)
{
    *q0o = q0;
    *q1o = q1;
    *q2o = q2;
    *q3o = q3;
}
