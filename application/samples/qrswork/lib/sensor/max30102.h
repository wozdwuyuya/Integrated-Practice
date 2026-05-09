// 协作提示：此文件已由 lib/ 迁移，硬件引脚严格保持小熊派原始定义。
/**
 * @file max30102.h
 * @brief MAX30102心率血氧传感器驱动头文件
 * @note 包含传感器初始化、数据读取、心率血氧算法声明
 */

#ifndef MAX30102_H
#define MAX30102_H

#include <stdint.h>
#include <stdbool.h>

extern uint32_t return_ac[2];
bool max30102_init(void);
/**
 * @brief  自动计算处理max30102的数据,无返回值
 * @param  全局变量@ return_ac[0]为心率 return_ac[1]为血氧
 * @param  请注意 此代码仅录入一对原始数据进入缓冲区 可调用max30102.c 内函数 is_time() 判断是否可以录入信息
 * @param  请以最低10ms的间隔运行程序 当vital_signs.heart_rate_enable && vital_signs.spo2_enable == 1 时便可输出心率血氧
*/
bool main_max30102_data(void);
bool is_time(void);

#endif