/**
 * @file fall_detection.h
 * @brief 老人跌倒检测功能头文件
 * @note 融合MPU6050和SW-420数据进行跌倒检测
 */

#ifndef FALL_DETECTION_H
#define FALL_DETECTION_H

#include <stdint.h>
#include <stdbool.h>

// 跌倒检测状态
typedef enum {
    FALL_STATE_NORMAL = 0,      // 正常状态
    FALL_STATE_FALLING,         // 正在跌倒
    FALL_STATE_FALLEN,          // 已跌倒
    FALL_STATE_ALERT            // 报警中
} fall_state_t;

// 跌倒检测配置
#define FALL_ACCEL_THRESHOLD    2.5f    // 加速度阈值 (g)
#define FALL_GYRO_THRESHOLD     50.0f   // 陀螺仪阈值 (°/s)
#define FALL_ANGLE_THRESHOLD    45.0f   // 倾斜角度阈值 (°)
#define FALL_DURATION_MS        3000    // 持续时间阈值 (ms)
#define FALL_VIBRATION_WEIGHT   0.3f    // 震动传感器权重

// [四柱-监测] 跌倒后处理配置
#define FALL_COOLDOWN_MS        10000   // 跌倒确认后冷却期 (ms)，防止重复触发
#define FALL_ALERT_TIMEOUT_MS   30000   // ALERT状态自动超时 (ms)

// 回调函数类型定义
typedef void (*fall_detection_callback_t)(fall_state_t state);

/**
 * @brief 跌倒检测初始化
 * @return true成功，false失败
 */
bool fall_detection_init(void);

/**
 * @brief 更新跌倒检测数据
 * @param accel 加速度数据 [x,y,z] (g)
 * @param gyro 陀螺仪数据 [x,y,z] (°/s)
 * @param vibration_detected 是否检测到震动
 * @param pitch 互补滤波输出的俯仰角 (°)，用于姿态判定
 * @param roll  互补滤波输出的横滚角 (°)，用于姿态判定
 */
void fall_detection_update(float accel[3], float gyro[3], bool vibration_detected,
                           float pitch, float roll);

/**
 * @brief 获取当前跌倒状态
 * @return 状态枚举值
 */
fall_state_t fall_detection_get_state(void);

/**
 * @brief 注册跌倒检测回调函数
 * @param callback 状态变化时的回调函数
 */
void fall_detection_register_callback(fall_detection_callback_t callback);

/**
 * @brief 确认跌倒报警（用户手动确认）
 */
void fall_detection_confirm(void);

/**
 * @brief 取消跌倒报警
 */
void fall_detection_cancel_alert(void);

/**
 * @brief 获取跌倒检测置信度
 * @return 0-100的置信度值
 */
uint8_t fall_detection_get_confidence(void);

/**
 * @brief 获取当前倾斜角度
 * @return 倾斜角度 (0-180°)
 */
float fall_detection_get_angle(void);

/**
 * @brief 检查是否在运动
 * @return true正在运动，false静止
 */
bool fall_detection_is_moving(void);

#endif // FALL_DETECTION_H
