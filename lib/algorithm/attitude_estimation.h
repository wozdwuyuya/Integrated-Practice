/**
 * @file attitude_estimation.h
 * @brief 姿态解算模块（互补滤波）
 * @note 融合加速度计和陀螺仪数据，输出 Pitch/Roll 欧拉角
 * @note 适用于跌倒检测场景：快速响应姿态突变，不追求长期零偏稳定
 */

#ifndef ATTITUDE_ESTIMATION_H
#define ATTITUDE_ESTIMATION_H

#include <stdint.h>
#include <stdbool.h>

// 互补滤波器结构体
typedef struct {
    float pitch;            // 俯仰角（°），绕X轴旋转
    float roll;             // 横滚角（°），绕Y轴旋转
    float alpha;            // 互补滤波系数（0~1，越大越信任陀螺仪）
    float accel_pitch;      // 加速度计计算的 Pitch（内部使用）
    float accel_roll;       // 加速度计计算的 Roll（内部使用）
    bool initialized;       // 是否已初始化
} attitude_state_t;

/**
 * @brief 初始化姿态解算模块
 * @param alpha 互补滤波系数，推荐 0.96~0.98
 *              - 越大越信任陀螺仪（响应快，但长期漂移）
 *              - 越小越信任加速度计（稳定，但对噪声敏感）
 */
void attitude_init(float alpha);

/**
 * @brief 更新姿态解算（每次采样调用一次）
 * @param accel_g  加速度数据 [x,y,z]，单位 g
 * @param gyro_dps 陀螺仪数据 [x,y,z]，单位 °/s
 * @param dt       距上次调用的时间间隔，单位秒
 */
void attitude_update(const float accel_g[3], const float gyro_dps[3], float dt);

/**
 * @brief 获取当前 Pitch 角
 * @return Pitch 角度（°），范围 -180~180
 */
float attitude_get_pitch(void);

/**
 * @brief 获取当前 Roll 角
 * @return Roll 角度（°），范围 -90~90
 */
float attitude_get_roll(void);

/**
 * @brief 获取姿态解算状态（用于调试）
 * @return 当前状态结构体指针
 */
const attitude_state_t* attitude_get_state(void);

#endif /* ATTITUDE_ESTIMATION_H */
