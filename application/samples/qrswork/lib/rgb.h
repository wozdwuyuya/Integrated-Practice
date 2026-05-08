#ifndef _RGB_H_
#define _RGB_H_

#include "pinctrl.h"
#include "soc_osal.h"
#include "gpio.h"
#include "osal_debug.h"
#include "cmsis_os2.h"
#include "app_init.h"
#include "ky.h"

#define RGB_R   GPIO_06
#define RGB_G   GPIO_07
#define RGB_B   GPIO_08
#define RGB_ON  GPIO_LEVEL_HIGH
#define RGB_OFF GPIO_LEVEL_LOW

bool RGB_init(void);
void RGB_run(bool r,bool g,bool b);
void RGB_R_crtl(bool r);
void RGB_G_crtl(bool g);
void RGB_B_crtl(bool b);

void heart_status_set_rgb(uint32_t adc_val);

#endif