// 协作提示：此文件已由 lib/ 迁移，硬件引脚严格保持小熊派原始定义。
/**
 * @file i2c_master.h
 * @brief I2C主设备初始化头文件
 * @note 包含I2C总线初始化、SSD1306、MPU6050设备初始化
 */

#ifndef I2C_MASTER_H
#define I2C_MASTER_H
#include <stdbool.h>
#include "pinctrl.h"
#include "soc_osal.h"
#include "i2c.h"
#include "osal_debug.h"
#include "cmsis_os2.h"
#include "app_init.h"
#include "errcode.h"

#define CONFIG_I2C_MASTER_BUS_ID 1  // I2C总线ID

void all_i2c_init(void);

/**
 * @brief 获取I2C总线互斥锁
 * @return 互斥锁句柄，未初始化返回NULL
 */
osMutexId_t i2c_master_get_mutex(void);

/**
 * @brief 获取I2C总线锁
 * @return true成功，false失败（互斥锁未初始化）
 */
bool i2c_master_lock(void);

/**
 * @brief 释放I2C总线锁
 */
void i2c_master_unlock(void);

#endif
