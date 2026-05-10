#include "rgb.h"

bool RGB_init(void){
    uapi_pin_set_mode(RGB_R,PIN_MODE_0);
    uapi_gpio_set_dir(RGB_R,GPIO_DIRECTION_OUTPUT);
    uapi_pin_set_ds(RGB_R, PIN_DS_MAX);
    uapi_gpio_set_val(RGB_R,GPIO_LEVEL_HIGH);

    uapi_pin_set_mode(RGB_G,PIN_MODE_0);
    uapi_gpio_set_dir(RGB_G,GPIO_DIRECTION_OUTPUT);
    uapi_pin_set_ds(RGB_G, PIN_DS_MAX);
    uapi_gpio_set_val(RGB_G,GPIO_LEVEL_HIGH);

    uapi_pin_set_mode(RGB_B,PIN_MODE_0);
    uapi_gpio_set_dir(RGB_B,GPIO_DIRECTION_OUTPUT);
    uapi_pin_set_ds(RGB_G, PIN_DS_MAX);
    uapi_gpio_set_val(RGB_B,GPIO_LEVEL_HIGH);

    RGB_run(1,0,0);
    osal_msleep(500);
    RGB_run(0,1,0);
    osal_msleep(500);
    RGB_run(0,0,1);
    osal_msleep(500);
    RGB_run(1,1,1);
    osal_msleep(500);
    RGB_run(1,1,1);
    return 1;
}

void RGB_R_crtl(bool r){
    
    if(r){
        uapi_gpio_set_val(RGB_R,RGB_ON);
    }
    else uapi_gpio_set_val(RGB_R,RGB_OFF);
}

void RGB_G_crtl(bool g){
    
    if(g){
        uapi_gpio_set_val(RGB_G,RGB_ON);
    }
    else uapi_gpio_set_val(RGB_G,RGB_OFF);
}

void RGB_B_crtl(bool b){
    
    if(b){
        uapi_gpio_set_val(RGB_B,RGB_ON);
    }
    else uapi_gpio_set_val(RGB_B,RGB_OFF);
}

void RGB_run(bool r,bool g,bool b){
    RGB_R_crtl(r);
    RGB_G_crtl(g);
    RGB_B_crtl(b);
}

/**
 * @brief 根据心跳ADC值控制RGB灯颜色
 * @param adc_val KY-039读取的ADC值
 */
void heart_status_set_rgb(uint32_t adc_val)
{
    // 1. 威胁区间 → 红色 (R=1, G=0, B=0)
    if (adc_val < KY_HEART_WARN_LOW || adc_val > KY_HEART_WARN_HIGH) {
        RGB_run(1, 0, 0);
        osal_printk("Heart Status: THREAT (RED) | ADC: %d\r\n", adc_val);
    }
    // 2. 提醒区间 → 黄色 (R=1, G=1, B=0)
    else if ((adc_val >= KY_HEART_WARN_LOW && adc_val < KY_HEART_NORMAL_LOW) ||
             (adc_val > KY_HEART_NORMAL_HIGH && adc_val <= KY_HEART_WARN_HIGH)) {
        RGB_run(1, 1, 0);
        osal_printk("Heart Status: WARN (YELLOW) | ADC: %d\r\n", adc_val);
    }
    // 3. 正常区间 → 绿色 (R=0, G=1, B=0)
    else {
        RGB_run(0, 1, 0);
        osal_printk("Heart Status: NORMAL (GREEN) | ADC: %d\r\n", adc_val);
    }
}