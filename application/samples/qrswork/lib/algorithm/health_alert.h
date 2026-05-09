// 协作提示：此文件已由 lib/ 迁移，硬件引脚严格保持小熊派原始定义。
/**
 * @file health_alert.h
 * @brief 心率异常智能提醒系统头文件
 * @note [四柱-提醒] 分级报警系统
 * @note 整合RGB灯、蜂鸣器、震动马达、OLED进行多模式提醒
 */

#ifndef HEALTH_ALERT_H
#define HEALTH_ALERT_H

#include <stdint.h>
#include <stdbool.h>

// 健康状态定义
typedef enum {
    HEALTH_NORMAL = 0,         // 正常状态
    HEALTH_HIGH_HR,            // 心率过高
    HEALTH_LOW_HR,             // 心率过低
    HEALTH_LOW_SPO2,           // 血氧过低
    HEALTH_FALL_DETECTED,      // 检测到跌倒
    HEALTH_FEVER               // 发烧
} health_status_t;

// [四柱-提醒] 报警分级：INFO(注意) / WARNING(警告) / DANGER(危险)
typedef enum {
    ALERT_LEVEL_NONE = 0,      // 无报警
    ALERT_LEVEL_INFO,          // 注意：仅视觉提示（OLED+LED变色）
    ALERT_LEVEL_WARNING,       // 警告：视觉+触觉（LED闪烁+震动）
    ALERT_LEVEL_DANGER         // 危险：全模式（LED+蜂鸣器+震动）
} alert_level_t;

// 提醒模式定义
typedef enum {
    ALERT_MODE_NONE = 0,       // 无提醒
    ALERT_MODE_VISUAL,         // 视觉提醒（RGB灯）
    ALERT_MODE_BUZZER,         // 蜂鸣器提醒
    ALERT_MODE_VIBRATION,      // 振动提醒
    ALERT_MODE_ALL             // 全模式提醒
} alert_mode_t;

// 心率阈值配置
typedef struct {
    uint32_t hr_high_threshold;     // 心率上限 (BPM)
    uint32_t hr_low_threshold;      // 心率下限 (BPM)
    uint32_t spo2_low_threshold;    // 血氧下限 (%)
    float temp_high_threshold;      // 温度上限 (℃)
} health_threshold_t;

// [四柱-提醒] 滞后阈值：避免边界值频繁触发/解除
#define ALERT_HYSTERESIS_HR     5       // 心率滞后 ±5 BPM
#define ALERT_HYSTERESIS_SPO2   2       // 血氧滞后 ±2%
#define ALERT_MUTE_TIMEOUT_MS   30000   // 静音自动恢复时间 30秒

/**
 * @brief 健康提醒系统初始化
 * @return true成功，false失败
 */
bool health_alert_init(void);

/**
 * @brief 设置健康阈值
 * @param threshold 阈值配置结构体
 */
void health_alert_set_threshold(health_threshold_t threshold);

/**
 * @brief 更新健康数据并触发提醒
 * @param heart_rate 心率 (BPM)
 * @param spo2 血氧 (%)
 * @param temperature 温度 (℃)
 */
void health_alert_update(uint32_t heart_rate, uint32_t spo2, float temperature);

/**
 * @brief 更新跌倒检测状态
 * @param fall_detected 是否检测到跌倒
 */
void health_alert_update_fall(bool fall_detected);

/**
 * @brief 获取当前健康状态
 * @return 健康状态枚举值
 */
health_status_t health_alert_get_status(void);

/**
 * @brief 获取当前提醒模式
 * @return 提醒模式枚举值
 */
alert_mode_t health_alert_get_mode(void);

/**
 * @brief 确认提醒（用户已知晓）
 */
void health_alert_confirm(void);

/**
 * @brief 静音提醒
 */
void health_alert_mute(void);

/**
 * @brief 更新提醒动画（需要在定时器中调用）
 */
void health_alert_update_animation(void);

/**
 * @brief 在OLED上显示健康状态
 * @param y 起始Y坐标
 */
void health_alert_display(uint8_t y);

/**
 * @brief 获取健康状态提示文字
 * @return 状态提示文字
 */
const char* health_alert_get_text(void);

/**
 * @brief [四柱-提醒] 获取当前报警等级
 * @return 报警等级枚举值
 */
alert_level_t health_alert_get_level(void);

#endif // HEALTH_ALERT_H
