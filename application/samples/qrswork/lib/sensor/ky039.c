/**
 * @file ky039.c
 * @brief KY-039心率传感器驱动实现
 * @note 使用ADC通道2采集模拟信号，通过峰值检测算法计算心率
 * @note 参考模板：qrswork/lib/ky.c + ky_test/ky.c
 */

#include "ky039.h"
#include "system/system_utils.h"
#include "pinctrl.h"
#include "soc_osal.h"
#include "adc.h"
#include "errcode.h"
#include "osal_debug.h"
#include "cmsis_os2.h"

/**
 * KY-039 ADC通道配置：
 *   当前配置：ADC_CHANNEL_2
 *   依据：旧项目 qrswork/lib/ky.c + ky_test/ky.c 均使用通道2
 *         注释原文："3863芯片KY-039硬件对应通道2"
 *   ⚠️ 请确认KY-039实际接的GPIO引脚，如不同需修改此处
 *   注意：uapi_adc_init() 的参数是时钟频率（ADC_CLOCK_500KHZ），不是通道号！
 */
#define KY039_ADC_CHANNEL  ADC_CHANNEL_2

// 心率检测参数
#define KY039_MIN_HEART_RATE 40   // 最小心率 (BPM)
#define KY039_MAX_HEART_RATE 200  // 最大心率 (BPM)
#define KY039_MIN_INTERVAL 300    // 最小峰值间隔 (ms)
#define KY039_MAX_INTERVAL 1500   // 最大峰值间隔 (ms)

// 心率检测状态
static uint32_t g_heart_rate = 0;
static uint32_t g_last_peak_time = 0;
static uint32_t g_last_peak_value = 0;
static uint8_t g_peak_count = 0;
static uint32_t g_peak_intervals[5] = {0};
static uint8_t g_interval_index = 0;

// ADC扫描配置
static adc_scan_config_t g_adc_scan_config = {
    .type = 0,
    .freq = 1,
};

// KY-039初始化
bool ky039_init(void){
    errcode_t ret;

    osal_printk("[KY039] ADC init (channel=%d)...\r\n", KY039_ADC_CHANNEL);

    // 1. 初始化ADC（参数为时钟频率，不是通道号）
    ret = uapi_adc_init(ADC_CLOCK_500KHZ);
    if(ret != ERRCODE_SUCC) {
        osal_printk("[KY039] ADC init failed, ret=0x%x\r\n", ret);
        return false;
    }

    // 2. 打开ADC通道
    ret = uapi_adc_open_channel(KY039_ADC_CHANNEL);
    if(ret != ERRCODE_SUCC) {
        osal_printk("[KY039] ADC open channel failed, ret=0x%x\r\n", ret);
        return false;
    }

    // 3. 使能ADC电源
    uapi_adc_power_en(AFE_GADC_MODE, true);

    osal_printk("[KY039] Init success\r\n");
    return true;
}

// 读取ADC原始值（手动采样）
uint16_t ky039_read_adc(void){
    int32_t raw = uapi_adc_manual_sample(KY039_ADC_CHANNEL);
    if(raw < 0) {
        return 0;
    }
    return (uint16_t)raw;
}

// 读取心率数据
bool ky039_read_heart_rate(ky039_data_t *data){
    uint32_t current_time;
    uint16_t adc_value;
    uint32_t avg_interval = 0;
    int valid_interval = 0;

    if(data == NULL) return false;

    // 读取ADC值
    adc_value = ky039_read_adc();
    data->adc_value = adc_value;
    data->valid = false;

    current_time = get_time_ms();

    // 峰值检测算法（带冷却期保护）
    // 冷却期内不更新峰值，防止同一心跳周期的噪声被误判为新峰值
    if(adc_value > g_last_peak_value && (current_time - g_last_peak_time) > KY039_MIN_INTERVAL) {
        g_last_peak_value = adc_value;
    }
    else if(g_last_peak_value > 0 && adc_value < g_last_peak_value * 0.8) {
        // 检测到峰值下降，认为是一个心跳周期
        uint32_t current_interval = current_time - g_last_peak_time;

        if(current_interval > KY039_MIN_INTERVAL && current_interval < KY039_MAX_INTERVAL) {
            g_peak_intervals[g_interval_index] = current_interval;
            g_interval_index = (g_interval_index + 1) % 5;
            g_peak_count++;

            if(g_peak_count >= 2) {
                // 计算平均间隔
                for(int i = 0; i < 5; i++) {
                    if(g_peak_intervals[i] > 0) {
                        avg_interval += g_peak_intervals[i];
                        valid_interval++;
                    }
                }

                if(valid_interval > 0) {
                    avg_interval = avg_interval / valid_interval;
                    g_heart_rate = 60000 / avg_interval;  // 转换为BPM

                    // 验证心率范围
                    if(g_heart_rate >= KY039_MIN_HEART_RATE && g_heart_rate <= KY039_MAX_HEART_RATE) {
                        data->heart_rate = g_heart_rate;
                        data->valid = true;
                    }
                }
            }
        }

        g_last_peak_value = 0;
        g_last_peak_time = current_time;
    }

    return data->valid;
}

// 获取心率值
uint32_t ky039_get_heart_rate(void){
    return g_heart_rate;
}

// 重置心率检测
void ky039_reset(void){
    g_heart_rate = 0;
    g_last_peak_time = 0;
    g_last_peak_value = 0;
    g_peak_count = 0;
    g_interval_index = 0;
    for(int i = 0; i < 5; i++) {
        g_peak_intervals[i] = 0;
    }
}
