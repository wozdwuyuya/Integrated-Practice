#include "led.h"
#include <common_def.h>

bool LED_init(void){
    // LED核心初始化配置（这部分是有效的，保留不变）
    uapi_pin_set_mode(LED, PIN_MODE_0);   // 配置引脚模式
    uapi_gpio_set_dir(LED, GPIO_DIRECTION_OUTPUT); // 配置为输出
    uapi_gpio_set_val(LED, GPIO_LEVEL_LOW); // 初始低电平

/*     uapi_pin_set_mode(GREEN,PIN_MODE_0);
    uapi_gpio_set_dir(GREEN,GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(GREEN,GPIO_LEVEL_HIGH);

    uapi_pin_set_mode(YELLOW,PIN_MODE_0);
    uapi_gpio_set_dir(YELLOW,GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(YELLOW,GPIO_LEVEL_HIGH);
    uapi_pin_set_mode(RED,PIN_MODE_0);
    uapi_gpio_set_dir(RED,GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(RED,GPIO_LEVEL_HIGH); */
    for(int i=0;i<6;i++){
        osal_msleep(500); // 延迟配置的时长
        uapi_gpio_toggle(LED);
/*         uapi_gpio_toggle(GREEN);
        uapi_gpio_toggle(YELLOW);
        uapi_gpio_toggle(RED); */
    }
    return 1; // 正常返回初始化成功状态
}

/*// 额外：抽离LED闪烁功能到独立函数，不污染初始化函数
void LED_Blink(void)
{
    while(1){
        osal_msleep(500); // 延迟配置的时长
        uapi_gpio_toggle(LED);    // 翻转引脚电平，实现闪烁
    }
}*/