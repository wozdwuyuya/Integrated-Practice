/**
 * @file rgb_led.c
 * @brief RGB三色灯驱动实现
 * @note [四柱-提醒] 软件PWM灰度 + 呼吸灯
 * @note 使用GPIO控制RGB灯，支持颜色切换、闪烁、亮度控制
 */

#include "rgb_led.h"
#include "system/system_utils.h"
#include "gpio.h"
#include "pinctrl.h"
#include "timer.h"
#include "errcode.h"
#include "osal_debug.h"

// [四柱-提醒] PWM定时器
static timer_handle_t g_pwm_timer = NULL;
static uint32_t g_pwm_timer_tick = 0;
static bool g_timer_initialized = false;  // 定时器子系统是否已初始化

// 10ms定时器回调
static void rgb_pwm_timer_cb(uintptr_t data){
    g_pwm_timer_tick++;
    rgb_led_pwm_update();
    if(g_pwm_timer != NULL) {
        uapi_timer_start(g_pwm_timer, RGB_PWM_STEP_MS * 1000, rgb_pwm_timer_cb, 0);
    }
}

// 闪烁状态
static rgb_blink_mode_t g_blink_mode = RGB_BLINK_OFF;
static rgb_color_t g_blink_color = RGB_COLOR_OFF;
static uint32_t g_blink_last_time = 0;
static uint8_t g_blink_state = 0;

// [四柱-提醒] 软件PWM状态
static uint8_t g_pwm_duty_r = 0;   // 红色占空比 (0-100)
static uint8_t g_pwm_duty_g = 0;   // 绿色占空比 (0-100)
static uint8_t g_pwm_duty_b = 0;   // 蓝色占空比 (0-100)
static uint8_t g_pwm_tick = 0;     // 当前PWM周期内的tick (0-9)
static bool g_pwm_active = false;  // PWM模式是否激活


// RGB灯初始化
bool rgb_led_init(void){
    errcode_t ret;

    // 配置RGB引脚为输出模式
    ret = uapi_pin_set_mode(RGB_LED_RED_PIN, 0);  // 模式0为GPIO
    if(ret != ERRCODE_SUCC) {
        osal_printk("RGB LED red pin mode set failed\r\n");
        return false;
    }

    ret = uapi_pin_set_mode(RGB_LED_GREEN_PIN, 0);
    if(ret != ERRCODE_SUCC) {
        osal_printk("RGB LED green pin mode set failed\r\n");
        return false;
    }

    ret = uapi_pin_set_mode(RGB_LED_BLUE_PIN, 0);
    if(ret != ERRCODE_SUCC) {
        osal_printk("RGB LED blue pin mode set failed\r\n");
        return false;
    }

    // 配置GPIO为输出方向
    ret = uapi_gpio_set_dir(RGB_LED_RED_PIN, GPIO_DIRECTION_OUTPUT);
    if(ret != ERRCODE_SUCC) {
        osal_printk("RGB LED red gpio dir set failed\r\n");
        return false;
    }

    ret = uapi_gpio_set_dir(RGB_LED_GREEN_PIN, GPIO_DIRECTION_OUTPUT);
    if(ret != ERRCODE_SUCC) {
        osal_printk("RGB LED green gpio dir set failed\r\n");
        return false;
    }

    ret = uapi_gpio_set_dir(RGB_LED_BLUE_PIN, GPIO_DIRECTION_OUTPUT);
    if(ret != ERRCODE_SUCC) {
        osal_printk("RGB LED blue gpio dir set failed\r\n");
        return false;
    }

    // 默认关闭
    rgb_led_off();

    osal_printk("RGB LED init success\r\n");
    return true;
}

// 设置RGB灯颜色（共阳：低电平亮，高电平灭）
void rgb_led_set_color(rgb_color_t color){
    // 关闭闪烁和PWM
    g_blink_mode = RGB_BLINK_OFF;
    g_pwm_active = false;

    switch(color) {
        case RGB_COLOR_OFF:
            uapi_gpio_set_val(RGB_LED_RED_PIN, GPIO_LEVEL_HIGH);
            uapi_gpio_set_val(RGB_LED_GREEN_PIN, GPIO_LEVEL_HIGH);
            uapi_gpio_set_val(RGB_LED_BLUE_PIN, GPIO_LEVEL_HIGH);
            break;
        case RGB_COLOR_RED:
            uapi_gpio_set_val(RGB_LED_RED_PIN, GPIO_LEVEL_LOW);
            uapi_gpio_set_val(RGB_LED_GREEN_PIN, GPIO_LEVEL_HIGH);
            uapi_gpio_set_val(RGB_LED_BLUE_PIN, GPIO_LEVEL_HIGH);
            break;
        case RGB_COLOR_GREEN:
            uapi_gpio_set_val(RGB_LED_RED_PIN, GPIO_LEVEL_HIGH);
            uapi_gpio_set_val(RGB_LED_GREEN_PIN, GPIO_LEVEL_LOW);
            uapi_gpio_set_val(RGB_LED_BLUE_PIN, GPIO_LEVEL_HIGH);
            break;
        case RGB_COLOR_BLUE:
            uapi_gpio_set_val(RGB_LED_RED_PIN, GPIO_LEVEL_HIGH);
            uapi_gpio_set_val(RGB_LED_GREEN_PIN, GPIO_LEVEL_HIGH);
            uapi_gpio_set_val(RGB_LED_BLUE_PIN, GPIO_LEVEL_LOW);
            break;
        case RGB_COLOR_YELLOW:
            uapi_gpio_set_val(RGB_LED_RED_PIN, GPIO_LEVEL_LOW);
            uapi_gpio_set_val(RGB_LED_GREEN_PIN, GPIO_LEVEL_LOW);
            uapi_gpio_set_val(RGB_LED_BLUE_PIN, GPIO_LEVEL_HIGH);
            break;
        case RGB_COLOR_CYAN:
            uapi_gpio_set_val(RGB_LED_RED_PIN, GPIO_LEVEL_HIGH);
            uapi_gpio_set_val(RGB_LED_GREEN_PIN, GPIO_LEVEL_LOW);
            uapi_gpio_set_val(RGB_LED_BLUE_PIN, GPIO_LEVEL_LOW);
            break;
        case RGB_COLOR_MAGENTA:
            uapi_gpio_set_val(RGB_LED_RED_PIN, GPIO_LEVEL_LOW);
            uapi_gpio_set_val(RGB_LED_GREEN_PIN, GPIO_LEVEL_HIGH);
            uapi_gpio_set_val(RGB_LED_BLUE_PIN, GPIO_LEVEL_LOW);
            break;
        case RGB_COLOR_WHITE:
            uapi_gpio_set_val(RGB_LED_RED_PIN, GPIO_LEVEL_LOW);
            uapi_gpio_set_val(RGB_LED_GREEN_PIN, GPIO_LEVEL_LOW);
            uapi_gpio_set_val(RGB_LED_BLUE_PIN, GPIO_LEVEL_LOW);
            break;
        default:
            break;
    }
}

// 设置RGB灯自定义颜色
void rgb_led_set_rgb(uint8_t r, uint8_t g, uint8_t b){
    // 关闭闪烁和PWM
    g_blink_mode = RGB_BLINK_OFF;
    g_pwm_active = false;

    // 共阳灯：低电平亮
    uapi_gpio_set_val(RGB_LED_RED_PIN, (r > 127) ? GPIO_LEVEL_LOW : GPIO_LEVEL_HIGH);
    uapi_gpio_set_val(RGB_LED_GREEN_PIN, (g > 127) ? GPIO_LEVEL_LOW : GPIO_LEVEL_HIGH);
    uapi_gpio_set_val(RGB_LED_BLUE_PIN, (b > 127) ? GPIO_LEVEL_LOW : GPIO_LEVEL_HIGH);
}

// 关闭RGB灯
void rgb_led_off(void){
    g_blink_mode = RGB_BLINK_OFF;
    g_pwm_active = false;
    if(g_pwm_timer != NULL) {
        uapi_timer_stop(g_pwm_timer);
        uapi_timer_delete(g_pwm_timer);
        g_pwm_timer = NULL;
    }
    // 共阳灯：HIGH = 灭
    uapi_gpio_set_val(RGB_LED_RED_PIN, GPIO_LEVEL_HIGH);
    uapi_gpio_set_val(RGB_LED_GREEN_PIN, GPIO_LEVEL_HIGH);
    uapi_gpio_set_val(RGB_LED_BLUE_PIN, GPIO_LEVEL_HIGH);
}

// [四柱-提醒] 设置RGB灯亮度（软件PWM，0-100%占空比）
void rgb_led_set_bright(uint8_t r, uint8_t g, uint8_t b){
    g_blink_mode = RGB_BLINK_OFF;
    g_pwm_duty_r = (r > 100) ? 100 : r;
    g_pwm_duty_g = (g > 100) ? 100 : g;
    g_pwm_duty_b = (b > 100) ? 100 : b;
    g_pwm_tick = 0;
    g_pwm_active = (r > 0 || g > 0 || b > 0);

    // 全灭时立即关闭GPIO并停止定时器（共阳：HIGH=灭）
    if(!g_pwm_active) {
        uapi_gpio_set_val(RGB_LED_RED_PIN, GPIO_LEVEL_HIGH);
        uapi_gpio_set_val(RGB_LED_GREEN_PIN, GPIO_LEVEL_HIGH);
        uapi_gpio_set_val(RGB_LED_BLUE_PIN, GPIO_LEVEL_HIGH);
        if(g_pwm_timer != NULL) {
            uapi_timer_stop(g_pwm_timer);
            uapi_timer_delete(g_pwm_timer);
            g_pwm_timer = NULL;
        }
        return;
    }

    // 启动PWM定时器（如果还没有的话）
    if(g_pwm_timer == NULL) {
        // SDK要求：首次使用定时器必须先init（参考timer_demo.c）
        if(!g_timer_initialized) {
            uapi_timer_init();
            g_timer_initialized = true;
        }
        errcode_t ret = uapi_timer_adapter(TIMER_INDEX_1, 0, 1);
        if(ret == ERRCODE_SUCC) {
            ret = uapi_timer_create(TIMER_INDEX_1, &g_pwm_timer);
            if(ret == ERRCODE_SUCC) {
                g_pwm_timer_tick = 0;  // 重置tick，从新周期开始
                uapi_timer_start(g_pwm_timer, RGB_PWM_STEP_MS * 1000, rgb_pwm_timer_cb, 0);
            }
        }
    }

    // 立即设置第一个tick的状态（共阳：LOW=亮）
    if(g_pwm_active) {
        uapi_gpio_set_val(RGB_LED_RED_PIN,   (g_pwm_duty_r > 0) ? GPIO_LEVEL_LOW : GPIO_LEVEL_HIGH);
        uapi_gpio_set_val(RGB_LED_GREEN_PIN, (g_pwm_duty_g > 0) ? GPIO_LEVEL_LOW : GPIO_LEVEL_HIGH);
        uapi_gpio_set_val(RGB_LED_BLUE_PIN,  (g_pwm_duty_b > 0) ? GPIO_LEVEL_LOW : GPIO_LEVEL_HIGH);
    }
}

// [四柱-提醒] 更新PWM状态（每10ms调用一次，100ms周期内10步）
void rgb_led_pwm_update(void){
    if(!g_pwm_active) return;

    g_pwm_tick++;
    if(g_pwm_tick >= 10) g_pwm_tick = 0;

    // 共阳灯：LOW = 亮
    uapi_gpio_set_val(RGB_LED_RED_PIN,
        (g_pwm_tick < (g_pwm_duty_r / 10)) ? GPIO_LEVEL_LOW : GPIO_LEVEL_HIGH);
    uapi_gpio_set_val(RGB_LED_GREEN_PIN,
        (g_pwm_tick < (g_pwm_duty_g / 10)) ? GPIO_LEVEL_LOW : GPIO_LEVEL_HIGH);
    uapi_gpio_set_val(RGB_LED_BLUE_PIN,
        (g_pwm_tick < (g_pwm_duty_b / 10)) ? GPIO_LEVEL_LOW : GPIO_LEVEL_HIGH);
}

// 设置闪烁模式
void rgb_led_set_blink(rgb_blink_mode_t mode, rgb_color_t color){
    g_blink_mode = mode;
    g_blink_color = color;
    g_blink_state = 0;
    g_blink_last_time = get_time_ms();
}

// 更新闪烁状态
void rgb_led_update_blink(void){
    uint32_t current_time;
    uint32_t interval;

    if(g_blink_mode == RGB_BLINK_OFF) {
        return;
    }

    current_time = get_time_ms();

    // 根据闪烁模式设置间隔
    switch(g_blink_mode) {
        case RGB_BLINK_SLOW:
            interval = 500;  // 500ms (1Hz)
            break;
        case RGB_BLINK_FAST:
            interval = 250;  // 250ms (2Hz)
            break;
        default:
            return;
    }

    // 检查是否到切换时间
    if(current_time - g_blink_last_time >= interval) {
        g_blink_last_time = current_time;
        g_blink_state = !g_blink_state;

        if(g_blink_state) {
            rgb_led_set_color(g_blink_color);
        } else {
            rgb_led_set_color(RGB_COLOR_OFF);
        }
    }
}

// 预定义状态指示函数
void rgb_led_status_normal(void){
    rgb_led_set_color(RGB_COLOR_GREEN);
}

void rgb_led_status_warning(void){
    rgb_led_set_blink(RGB_BLINK_SLOW, RGB_COLOR_YELLOW);
}

void rgb_led_status_error(void){
    rgb_led_set_blink(RGB_BLINK_FAST, RGB_COLOR_RED);
}

void rgb_led_status_bluetooth(void){
    rgb_led_set_blink(RGB_BLINK_SLOW, RGB_COLOR_BLUE);
}
