/**
 * @file beep.c
 * @brief 蜂鸣器驱动实现
 * @note 使用PWM控制蜂鸣器，支持初始化和定时鸣叫
 */

#include "beep.h"
#include "errcode.h"
#include "pinctrl.h"
#include "gpio.h"
#include "pwm.h"
#include "cmsis_os2.h"

/**
 * 蜂鸣器配置：
 *   引脚：GPIO5，无源蜂鸣器，PWM模式
 *   PWM通道：5（H3863 PWM通道与引脚绑定，GPIO5对应PWM通道5）
 *   ⚠️ 如果接线不同，需同时修改 beep.h 中的引脚宏和此处的 PWM 通道号
 *   ⚠️ 无源蜂鸣器需PWM驱动，有源蜂鸣器只需GPIO高低电平
 */
#define beep_channel 5  // PWM通道5（GPIO5 → PWM通道5）

// PWM配置：占空比50%的方波
pwm_config_t cfg_no_repeat = {
    125,     // 低电平时间
    125,     // 高电平时间
    0,
    0,
    true
};
// 蜂鸣器初始化：配置PWM引脚并测试鸣叫
bool beep_init(void){

    errcode_t ret;
    uapi_pin_set_mode(GPIO_05,PIN_MODE_1);
    uapi_pwm_deinit();
    ret = uapi_pwm_init();
    if(ret != ERRCODE_SUCC)return 0;
    uapi_pwm_open(beep_channel,&cfg_no_repeat);
    osDelay(100);
    uapi_pwm_close(beep_channel);
    return 1;
}

// 蜂鸣器定时鸣叫：鸣叫指定时间(ms)
void beep_time(uint32_t time){
    uapi_pwm_open(beep_channel,&cfg_no_repeat);
    osDelay(time);
    uapi_pwm_close(beep_channel);
}