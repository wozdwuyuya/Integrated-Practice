#ifndef _LED_H_
#define _LED_H_

#include "pinctrl.h"
#include "soc_osal.h"
#include "gpio.h"
#include "osal_debug.h"
#include "cmsis_os2.h"
#include "app_init.h"


bool LED_init(void);
void LED_Blink(void);

#define LED     2
/* #define GREEN   2
#define YELLOW  GPIO_07
#define RED     GPIO_08
#define LED_ON  uapi_gpio_set_val(LED,GPIO_LEVEL_HIGH)
#define LED_OFF uapi_gpio_set_val(LED,GPIO_LEVEL_LOW)
#define GREEN_ON  uapi_gpio_set_val(GREEN,GPIO_LEVEL_HIGH)
#define GREEN_OFF uapi_gpio_set_val(GREEN,GPIO_LEVEL_LOW)
#define YELLOW_ON  uapi_gpio_set_val(YELLOW,GPIO_LEVEL_HIGH)
#define YELLOW_OFF uapi_gpio_set_val(YELLOW,GPIO_LEVEL_LOW)
#define RED_ON  uapi_gpio_set_val(RED,GPIO_LEVEL_HIGH)
#define RED_OFF uapi_gpio_set_val(RED,GPIO_LEVEL_LOW) */
#endif