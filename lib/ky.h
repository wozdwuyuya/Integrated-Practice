#ifndef _KY_H_
#define _KY_H_

#include "pinctrl.h"
#include "soc_osal.h"
#include "adc.h"
#include "adc_porting.h"
#include "osal_debug.h"
#include "cmsis_os2.h"
#include "app_init.h"

/**
 * @brief KY-039心跳传感器ADC通道定义（H3863开发板）
 * 硬件连接：KY-039模拟输出 → 主控ADC通道2
 */
#define KY                      2

/**
 * @brief 心跳ADC值区间阈值（单位：ADC原始值，需根据硬件校准）
 */
#define KY_HEART_NORMAL_LOW     200     // 下调正常下限
#define KY_HEART_NORMAL_HIGH    1200    // 上调正常上限
#define KY_HEART_WARN_LOW       100     // 下调提醒下限
#define KY_HEART_WARN_HIGH      1500   // 上调提醒上限
/**
 * @brief 初始化KY-039心跳传感器（ADC底层配置）
 * @return true-初始化成功，false-初始化失败
 */
bool KY_init(void);

/**
 * @brief 读取KY-039心跳传感器的ADC原始值
 * @return 非0-有效ADC值，0-采样失败/超时
 */
uint32_t KY_read_heart_adc(void);

/**
 * @brief ADC自动扫描回调函数（底层触发，处理采样数据）
 * @param ch ADC通道
 * @param buffer 采样数据缓冲区
 * @param length 缓冲区长度
 * @param next 是否继续扫描（未使用）
 */
void kycallback(uint8_t ch, uint32_t *buffer, uint32_t length, bool *next);

#endif // _KY_H_