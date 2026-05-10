/**
 * @file sw420.c
 * @brief SW-420震动传感器驱动实现
 * @note 使用GPIO中断检测震动事件
 */

#include "sw420.h"
#include "gpio.h"
#include "pinctrl.h"
#include "errcode.h"
#include "osal_debug.h"
#include "system/system_utils.h"

// 震动回调函数
static sw420_callback_t g_vibration_callback = NULL;

// 震动计数
static volatile uint32_t g_vibration_count = 0;

// 消抖：上次触发时间戳
static volatile uint32_t g_last_irq_time = 0;
#define SW420_DEBOUNCE_MS  50  // 50ms 消抖窗口

// 中断处理函数（带消抖）
static void sw420_irq_handler(uint8_t pin, uint32_t data){
    uint32_t now = get_time_ms();
    if(now - g_last_irq_time < SW420_DEBOUNCE_MS) {
        return;  // 消抖期内，忽略本次中断
    }
    g_last_irq_time = now;
    g_vibration_count++;

    // 调用用户注册的回调函数
    if(g_vibration_callback != NULL) {
        g_vibration_callback();
    }
}

// SW-420初始化
bool sw420_init(void){
    errcode_t ret;

    // 配置GPIO为输入模式
    ret = uapi_pin_set_mode(SW420_GPIO_PIN, 0);  // 模式0为GPIO
    if(ret != ERRCODE_SUCC) {
        osal_printk("SW420 pin mode set failed\r\n");
        return false;
    }

    // 配置GPIO为输入方向
    ret = uapi_gpio_set_dir(SW420_GPIO_PIN, GPIO_DIRECTION_INPUT);
    if(ret != ERRCODE_SUCC) {
        osal_printk("SW420 gpio dir set failed\r\n");
        return false;
    }

    // 配置GPIO中断（下降沿触发，检测到震动时输出低电平）
    ret = uapi_pin_set_irq(SW420_GPIO_PIN, GPIO_IRQ_TYPE_EDGE_FALLING, sw420_irq_handler, 0);
    if(ret != ERRCODE_SUCC) {
        osal_printk("SW420 irq set failed\r\n");
        return false;
    }

    g_vibration_count = 0;

    osal_printk("SW420 init success\r\n");
    return true;
}

// 读取当前震动状态
bool sw420_read(void){
    uint8_t val = 0;
    uapi_gpio_get_val(SW420_GPIO_PIN, &val);
    return (val == 0);  // 低电平表示检测到震动
}

// 注册震动中断回调函数
void sw420_register_callback(sw420_callback_t callback){
    g_vibration_callback = callback;
}

// 使能/禁用震动检测中断
void sw420_enable_interrupt(bool enable){
    if(enable) {
        uapi_pin_set_irq(SW420_GPIO_PIN, GPIO_IRQ_TYPE_EDGE_FALLING, sw420_irq_handler, 0);
    } else {
        uapi_pin_set_irq(SW420_GPIO_PIN, GPIO_IRQ_TYPE_EDGE_NONE, NULL, 0);
    }
}

// 获取震动计数
uint32_t sw420_get_count(void){
    return g_vibration_count;
}

// 重置震动计数
void sw420_reset_count(void){
    g_vibration_count = 0;
}
