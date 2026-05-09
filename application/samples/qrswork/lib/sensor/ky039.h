// 协作提示：此文件已由 lib/ 迁移，硬件引脚严格保持小熊派原始定义。
/**
 * @file ky039.h
 * @brief KY-039心率传感器驱动头文件
 * @note 模拟输出，需要ADC采集心率波形
 * @note 硬件连接：KY-039模拟输出 → H3863 ADC通道2
 */

#ifndef KY039_H
#define KY039_H

#include <stdint.h>
#include <stdbool.h>

// 数据结构定义
typedef struct {
    uint32_t heart_rate;       // 心率值 (BPM)
    uint16_t adc_value;        // 当前ADC值
    bool valid;                // 数据有效标志
} ky039_data_t;

/**
 * @brief KY-039初始化
 * @return true成功，false失败
 */
bool ky039_init(void);

/**
 * @brief 读取ADC原始值
 * @return ADC值 (0-4095)
 */
uint16_t ky039_read_adc(void);

/**
 * @brief 读取心率数据
 * @param data 输出数据结构
 * @return true有有效心率数据，false无数据
 */
bool ky039_read_heart_rate(ky039_data_t *data);

/**
 * @brief 获取心率值
 * @return 心率 (BPM)，0表示无有效数据
 */
uint32_t ky039_get_heart_rate(void);

/**
 * @brief 重置心率检测
 */
void ky039_reset(void);

#endif // KY039_H
