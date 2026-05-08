#include "ky.h"
#include "errcode.h"
#include "adc.h"
#include <string.h>
#include "cmsis_os2.h" // 增加RTOS同步头文件

// 全局变量：增加互斥锁保护
static int32_t adc_val = 0;
static adc_scan_config_t config = {0};
static bool g_ky_callback_done = false;
static osMutexId_t ky_mutex; // ADC数据读写互斥锁
uint32_t num=0;
uint32_t lengthn=0;

void kycallback(uint8_t ch, uint32_t *buffer, uint32_t length, bool *next) 
{ 
     UNUSED(next);
    num=0;
    lengthn=0;
        for (uint32_t i = 0; i < length; i++) {
        printf("channel: %d, voltage: %dmv\r\n", ch, buffer[i]);
    } 
    for (uint32_t i = 0; i < length; i++) {
        if(buffer[i]!=0){
            num+=buffer[i];
            lengthn++;
        }   
    }
    if(lengthn!=0){
        adc_val=num/lengthn;
    }
    else adc_val=0; 
}

bool KY_init(void)
{
    osal_printk("start adc test");
    uapi_adc_init(ADC_CLOCK_500KHZ);
    uapi_adc_power_en(AFE_SCAN_MODE_MAX_NUM, true);
    
    // 新增：初始化互斥锁
    ky_mutex = osMutexNew(NULL);
    if (ky_mutex == NULL) {
        osal_printk("KY mutex create failed!\r\n");
        return false;
    }

    adc_scan_config_t config = {
        .type = 0,
        .freq = 1,
    };    
    return 1;
}

/**
 * @brief 读取KY-039心跳传感器的ADC原始值（优化超时逻辑）
 * @return 非0-有效ADC值，0-采样失败/超时
 */
uint32_t KY_read_heart_adc(void) {
    // 临界区：重置标志位和旧值
    osMutexAcquire(ky_mutex, osWaitForever);
    g_ky_callback_done = false;
    adc_val = 0;
    osMutexRelease(ky_mutex);

    uapi_adc_auto_scan_ch_enable(ADC_CHANNEL_2, config, kycallback);
    osal_msleep(1000);

    uapi_adc_auto_scan_ch_disable(ADC_CHANNEL_2);
 
    // 临界区：读取最终ADC值（即使超时，也返回已采集的有效值）
    osMutexAcquire(ky_mutex, osWaitForever);
    uint32_t result = (adc_val > 0) ? (uint32_t)adc_val : 0;
    osMutexRelease(ky_mutex);

    return result;
}