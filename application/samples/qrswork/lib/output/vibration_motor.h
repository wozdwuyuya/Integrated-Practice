// 协作提示：此文件已由 lib/ 迁移，硬件引脚严格保持小熊派原始定义。
/**
 * @file vibration_motor.h
 * @brief 震动马达模块驱动头文件
 * @note GPIO控制，高电平开启震动
 */

#ifndef VIBRATION_MOTOR_H
#define VIBRATION_MOTOR_H

#include <stdint.h>
#include <stdbool.h>

/**
 * 震动马达引脚配置：
 *   当前配置：GPIO3，数字输出，高电平=开启震动
 *   ⚠️ 5V供电，需三极管/MOSFET驱动电路 + 续流二极管(1N4148)保护
 */
#define VIBRATION_MOTOR_PIN 3  // GPIO3

/**
 * @brief 震动马达初始化
 * @return true成功，false失败
 */
bool vibration_motor_init(void);

/**
 * @brief 开启震动
 */
void vibration_motor_on(void);

/**
 * @brief 关闭震动
 */
void vibration_motor_off(void);

/**
 * @brief 定时震动
 * @param duration_ms 震动持续时间 (毫秒)
 */
void vibration_motor_pulse(uint32_t duration_ms);

/**
 * @brief 触觉反馈模式（连续短震动）
 * @param count 震动次数
 * @param interval_ms 每次震动间隔 (毫秒)
 */
void vibration_motor_pattern(uint8_t count, uint32_t interval_ms);

/**
 * @brief 更新震动状态（需要在定时器中调用，用于脉冲震动）
 */
void vibration_motor_update(void);

#endif // VIBRATION_MOTOR_H
