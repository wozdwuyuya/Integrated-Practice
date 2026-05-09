// 协作提示：此文件已由 lib/ 迁移，硬件引脚严格保持小熊派原始定义。
/**
 * @file beep.h
 * @brief 蜂鸣器驱动头文件
 * @note 包含蜂鸣器初始化和定时鸣叫功能
 */

#ifndef BEEP_H
#define BEEP_H

#include <stdint.h>
#include <stdbool.h>

bool beep_init(void);
void beep_time(uint32_t time);


#endif
