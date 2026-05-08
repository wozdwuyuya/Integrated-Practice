/**
 * @file sw420.h
 * @brief SW-420震动传感器驱动头文件
 * @note 数字输出，检测到震动时触发中断
 */

#ifndef SW420_H
#define SW420_H

#include <stdint.h>
#include <stdbool.h>

/**
 * SW-420震动传感器引脚配置：
 *   当前配置：GPIO4，数字输入，低电平=检测到震动
 *   注意：GPIO4 是启动限制引脚，上电时不可被强拉高
 */
#define SW420_GPIO_PIN 4  // GPIO4

// 回调函数类型定义
typedef void (*sw420_callback_t)(void);

/**
 * @brief SW-420初始化
 * @return true成功，false失败
 */
bool sw420_init(void);

/**
 * @brief 读取当前震动状态
 * @return true检测到震动，false无震动
 */
bool sw420_read(void);

/**
 * @brief 注册震动中断回调函数
 * @param callback 震动触发时的回调函数
 */
void sw420_register_callback(sw420_callback_t callback);

/**
 * @brief 使能/禁用震动检测中断
 * @param enable true使能，false禁用
 */
void sw420_enable_interrupt(bool enable);

/**
 * @brief 获取震动计数
 * @return 震动触发次数
 */
uint32_t sw420_get_count(void);

/**
 * @brief 重置震动计数
 */
void sw420_reset_count(void);

#endif // SW420_H
