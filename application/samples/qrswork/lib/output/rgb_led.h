// 协作提示：此文件已由 lib/ 迁移，硬件引脚严格保持小熊派原始定义。
/**
 * @file rgb_led.h
 * @brief RGB三色灯驱动头文件
 * @note [四柱-提醒] 软件PWM灰度
 * @note 支持共阴/共阳RGB灯，提供颜色控制、闪烁、呼吸灯功能
 */

#ifndef RGB_LED_H
#define RGB_LED_H

#include <stdint.h>
#include <stdbool.h>

/**
 * RGB灯引脚配置（共阳灯珠，低电平点亮）：
 *   Red   = GPIO6
 *   Green = GPIO7
 *   Blue  = GPIO8
 *   共阳接法：GPIO LOW = 点亮，GPIO HIGH = 熄灭
 *   ⚠️ 如果改接共阴灯珠，需反转 rgb_led.c 中所有 GPIO_LEVEL（HIGH↔LOW）
 */
#define RGB_LED_RED_PIN   6   // GPIO6
#define RGB_LED_GREEN_PIN 7   // GPIO7
#define RGB_LED_BLUE_PIN  8   // GPIO8

// [四柱-提醒] PWM参数
#define RGB_PWM_PERIOD_MS   100     // PWM周期 (ms)
#define RGB_PWM_STEP_MS     10      // PWM步进 (ms) — 每10ms更新一次

// 颜色定义
typedef enum {
    RGB_COLOR_OFF = 0,      // 灭
    RGB_COLOR_RED,          // 红色
    RGB_COLOR_GREEN,        // 绿色
    RGB_COLOR_BLUE,         // 蓝色
    RGB_COLOR_YELLOW,       // 黄色 (红+绿)
    RGB_COLOR_CYAN,         // 青色 (绿+蓝)
    RGB_COLOR_MAGENTA,      // 品红 (红+蓝)
    RGB_COLOR_WHITE         // 白色 (红+绿+蓝)
} rgb_color_t;

// 闪烁模式定义
typedef enum {
    RGB_BLINK_OFF = 0,      // 不闪烁
    RGB_BLINK_SLOW,         // 慢闪 (1Hz)
    RGB_BLINK_FAST          // 快闪 (2Hz)
} rgb_blink_mode_t;

/**
 * @brief RGB灯初始化
 * @return true成功，false失败
 */
bool rgb_led_init(void);

/**
 * @brief 设置RGB灯颜色
 * @param color 颜色枚举值
 */
void rgb_led_set_color(rgb_color_t color);

/**
 * @brief 设置RGB灯自定义颜色（PWM控制）
 * @param r 红色分量 (0-255)
 * @param g 绿色分量 (0-255)
 * @param b 蓝色分量 (0-255)
 */
void rgb_led_set_rgb(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief 关闭RGB灯
 */
void rgb_led_off(void);

/**
 * @brief [四柱-提醒] 设置RGB灯亮度（软件PWM）
 * @param r 红色亮度 (0-100)
 * @param g 绿色亮度 (0-100)
 * @param b 蓝色亮度 (0-100)
 */
void rgb_led_set_bright(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief [四柱-提醒] 更新PWM状态（需要每10ms调用一次）
 */
void rgb_led_pwm_update(void);

/**
 * @brief 设置闪烁模式
 * @param mode 闪烁模式
 * @param color 闪烁颜色
 */
void rgb_led_set_blink(rgb_blink_mode_t mode, rgb_color_t color);

/**
 * @brief 更新闪烁状态（需要在定时器中调用）
 */
void rgb_led_update_blink(void);

// 预定义状态指示函数
void rgb_led_status_normal(void);    // 正常状态（绿色常亮）
void rgb_led_status_warning(void);   // 警告状态（黄色闪烁）
void rgb_led_status_error(void);     // 错误状态（红色闪烁）
void rgb_led_status_bluetooth(void); // 蓝牙状态（蓝色闪烁）

#endif // RGB_LED_H
