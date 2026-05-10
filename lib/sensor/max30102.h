/**
 * @file max30102.h
 * @brief MAX30102心率血氧传感器驱动头文件
 * @note 包含传感器初始化、数据读取、心率血氧算法声明
 */

#ifndef MAX30102_H
#define MAX30102_H

#include <stdint.h>
#include <stdbool.h>

// MAX30102 I2C设备地址
#define MAX30102_ADDRESS       0x57

// I2C总线编号
#define MAX30102_I2C_BUS       1

// 器件ID
#define MAX30102_PART_ID       0x15

// 寄存器地址定义
#define MAX30102_REG_INTR_STATUS1  0x00
#define MAX30102_REG_INTR_STATUS2  0x01
#define MAX30102_REG_FIFO_WR_PTR   0x04
#define MAX30102_REG_FIFO_RD_PTR   0x06
#define MAX30102_REG_FIFO_DATA     0x07
#define MAX30102_REG_MODE_CONFIG   0x09
#define MAX30102_REG_SPO2_CONFIG   0x0A
#define MAX30102_REG_LED1_PA       0x0C
#define MAX30102_REG_LED2_PA       0x0D
#define MAX30102_REG_PART_ID_REG   0xFF

// 模式配置
#define MAX30102_MODE_SPO2         0x03
#define MAX30102_MODE_RESET        0x40

// SpO2配置：100Hz采样率，18位ADC，脉冲宽度411us
#define MAX30102_SPO2_CONFIG_VAL   0x27

// LED电流默认值 (约6.4mA)
#define MAX30102_LED_CURRENT_DEFAULT 0x1F

// 输出数据：return_ac[0]为心率(BPM), return_ac[1]为血氧(%)
extern volatile uint32_t return_ac[2];

/**
 * @brief  MAX30102初始化：验证ID、软件复位、配置寄存器、启动定时器
 * @return true成功，false失败
 */
bool max30102_init(void);

/**
 * @brief  自动计算处理max30102的数据
 * @param  请以最低10ms的间隔运行程序
 * @param  当return_ac[0]和return_ac[1]都有效时返回true
 * @return true心率血氧数据有效，false数据尚未稳定
 */
bool main_max30102_data(void);

/**
 * @brief  检查是否可以采样(10ms间隔)
 * @return true可以采样，false未到采样时间
 */
bool is_time(void);

/**
 * @brief  清理定时器资源
 */
void clean_timer(void);

#endif /* MAX30102_H */
