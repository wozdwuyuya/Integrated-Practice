/**
 * @file attitude_estimation.c
 * @brief 姿态解算模块实现（互补滤波）
 * @note 算法原理：
 *       加速度计：低频准确（静态时能算出重力方向），高频噪声大
 *       陀螺仪：  高频准确（瞬时角速度积分得角度），低频会漂移
 *       互补滤波 = α × (陀螺仪积分角度) + (1-α) × (加速度计角度)
 *       两者互补，兼顾响应速度和稳定性
 */

#include "attitude_estimation.h"
#include <math.h>

// 静态全局状态
static attitude_state_t g_attitude;

// 辅助函数：限制角度范围到 [-180, 180]
static float normalize_angle(float angle){
    while(angle > 180.0f) angle -= 360.0f;
    while(angle < -180.0f) angle += 360.0f;
    return angle;
}

void attitude_init(float alpha){
    g_attitude.pitch = 0.0f;
    g_attitude.roll = 0.0f;
    g_attitude.accel_pitch = 0.0f;
    g_attitude.accel_roll = 0.0f;
    g_attitude.alpha = alpha;
    g_attitude.initialized = false;
}

void attitude_update(const float accel_g[3], const float gyro_dps[3], float dt){
    if(accel_g == NULL || gyro_dps == NULL || dt <= 0.0f) return;

    float ax = accel_g[0];
    float ay = accel_g[1];
    float az = accel_g[2];

    // 第一步：用加速度计计算 Pitch 和 Roll（重力方向）
    // Pitch = atan2(-ax, sqrt(ay² + az²))，绕X轴
    // Roll  = atan2(ay, az)，绕Y轴
    float accel_pitch_rad = atan2f(-ax, sqrtf(ay * ay + az * az));
    float accel_roll_rad  = atan2f(ay, az);

    g_attitude.accel_pitch = accel_pitch_rad * 180.0f / M_PI;
    g_attitude.accel_roll  = accel_roll_rad  * 180.0f / M_PI;

    // 第二步：如果未初始化，直接用加速度计角度作为初始值
    if(!g_attitude.initialized) {
        g_attitude.pitch = g_attitude.accel_pitch;
        g_attitude.roll  = g_attitude.accel_roll;
        g_attitude.initialized = true;
        return;
    }

    // 第三步：互补滤波融合
    // 陀螺仪积分角度 + 加速度计角度，按 alpha 加权
    float gyro_pitch_delta = gyro_dps[0] * dt;  // 陀螺仪X轴角速度积分
    float gyro_roll_delta  = gyro_dps[1] * dt;  // 陀螺仪Y轴角速度积分

    // 互补滤波核心公式：
    // 新角度 = α × (旧角度 + 陀螺仪增量) + (1-α) × 加速度计角度
    g_attitude.pitch = g_attitude.alpha * (g_attitude.pitch + gyro_pitch_delta)
                     + (1.0f - g_attitude.alpha) * g_attitude.accel_pitch;

    g_attitude.roll  = g_attitude.alpha * (g_attitude.roll + gyro_roll_delta)
                     + (1.0f - g_attitude.alpha) * g_attitude.accel_roll;

    // 限制角度范围，防止数值溢出
    g_attitude.pitch = normalize_angle(g_attitude.pitch);
    g_attitude.roll  = normalize_angle(g_attitude.roll);
}

float attitude_get_pitch(void){
    return g_attitude.pitch;
}

float attitude_get_roll(void){
    return g_attitude.roll;
}

const attitude_state_t* attitude_get_state(void){
    return &g_attitude;
}
