/**
 * @file vibration_motor.c
 * @brief 震动马达模块驱动实现
 * @note 使用GPIO控制震动马达，支持脉冲和模式震动
 */

#include "vibration_motor.h"
#include "system/system_utils.h"
#include "gpio.h"
#include "pinctrl.h"
#include "errcode.h"
#include "osal_debug.h"

// 震动状态
static uint8_t g_vibration_active = 0;
static uint32_t g_vibration_end_time = 0;
static uint8_t g_pattern_count = 0;
static uint8_t g_pattern_index = 0;
static uint32_t g_pattern_interval = 0;
static uint32_t g_pattern_start_time = 0;


// 震动马达初始化
bool vibration_motor_init(void){
    errcode_t ret;

    // 配置GPIO为输出模式
    ret = uapi_pin_set_mode(VIBRATION_MOTOR_PIN, 0);  // 模式0为GPIO
    if(ret != ERRCODE_SUCC) {
        osal_printk("Vibration motor pin mode set failed\r\n");
        return false;
    }

    // 配置GPIO为输出方向
    ret = uapi_gpio_set_dir(VIBRATION_MOTOR_PIN, GPIO_DIRECTION_OUTPUT);
    if(ret != ERRCODE_SUCC) {
        osal_printk("Vibration motor gpio dir set failed\r\n");
        return false;
    }

    // 默认关闭
    vibration_motor_off();

    osal_printk("Vibration motor init success\r\n");
    return true;
}

// 开启震动
void vibration_motor_on(void){
    uapi_gpio_set_val(VIBRATION_MOTOR_PIN, GPIO_LEVEL_HIGH);
    g_vibration_active = 1;
}

// 关闭震动
void vibration_motor_off(void){
    uapi_gpio_set_val(VIBRATION_MOTOR_PIN, GPIO_LEVEL_LOW);
    g_vibration_active = 0;
    g_vibration_end_time = 0;
}

// 定时震动
void vibration_motor_pulse(uint32_t duration_ms){
    vibration_motor_on();
    g_vibration_end_time = get_time_ms() + duration_ms;
}

// 触觉反馈模式
void vibration_motor_pattern(uint8_t count, uint32_t interval_ms){
    g_pattern_count = count;
    g_pattern_index = 0;
    g_pattern_interval = interval_ms;
    g_pattern_start_time = get_time_ms();
    vibration_motor_on();
}

// 更新震动状态
void vibration_motor_update(void){
    uint32_t current_time = get_time_ms();

    // 检查定时震动是否结束
    if(g_vibration_end_time > 0 && current_time >= g_vibration_end_time) {
        vibration_motor_off();
    }

    // 检查模式震动
    if(g_pattern_count > 0) {
        uint32_t elapsed = current_time - g_pattern_start_time;
        uint32_t expected_time = g_pattern_index * g_pattern_interval;

        if(elapsed >= expected_time) {
            if(g_pattern_index < g_pattern_count * 2) {
                // 奇数次震动，偶数次停止
                if(g_pattern_index % 2 == 0) {
                    vibration_motor_on();
                } else {
                    vibration_motor_off();
                }
                g_pattern_index++;
            } else {
                // 模式完成
                g_pattern_count = 0;
                vibration_motor_off();
            }
        }
    }
}
