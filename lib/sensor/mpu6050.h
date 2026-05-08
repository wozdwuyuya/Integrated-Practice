/**
 * @file mpu6050.h
 * @brief MPU6050六轴IMU传感器驱动头文件
 * @note 提供加速度、角速度、温度数据读取，支持姿态解算
 */

#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>
#include <stdbool.h>

// MPU6050 I2C地址（AD0引脚接地时为0x68，接VCC时为0x69）
#define MPU6050_ADDRESS 0x68

// 寄存器地址定义
#define MPU6050_REG_SMPLRT_DIV   0x19  // 采样率分频
#define MPU6050_REG_CONFIG       0x1A  // 配置寄存器
#define MPU6050_REG_GYRO_CONFIG  0x1B  // 陀螺仪配置
#define MPU6050_REG_ACCEL_CONFIG 0x1C  // 加速度计配置
#define MPU6050_REG_ACCEL_XOUT_H 0x3B  // 加速度X轴高字节
#define MPU6050_REG_TEMP_OUT_H   0x41  // 温度高字节
#define MPU6050_REG_GYRO_XOUT_H  0x43  // 陀螺仪X轴高字节
#define MPU6050_REG_PWR_MGMT_1   0x6B  // 电源管理1
#define MPU6050_REG_WHO_AM_I     0x75  // 设备ID寄存器

// 加速度量程设置
#define MPU6050_ACCEL_RANGE_2G   0x00  // ±2g
#define MPU6050_ACCEL_RANGE_4G   0x08  // ±4g
#define MPU6050_ACCEL_RANGE_8G   0x10  // ±8g
#define MPU6050_ACCEL_RANGE_16G  0x18  // ±16g

// 陀螺仪量程设置
#define MPU6050_GYRO_RANGE_250   0x00  // ±250°/s
#define MPU6050_GYRO_RANGE_500   0x08  // ±500°/s
#define MPU6050_GYRO_RANGE_1000  0x10  // ±1000°/s
#define MPU6050_GYRO_RANGE_2000  0x18  // ±2000°/s

// 数据结构定义
typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} mpu6050_vec3_t;

typedef struct {
    mpu6050_vec3_t accel;      // 加速度数据 (原始值)
    mpu6050_vec3_t gyro;       // 陀螺仪数据 (原始值)
    float temperature;         // 温度 (℃)
    float accel_g[3];          // 加速度 (g值)
    float gyro_dps[3];         // 角速度 (°/s)
} mpu6050_data_t;

// 函数声明

/**
 * @brief MPU6050初始化
 * @param accel_range 加速度量程 (MPU6050_ACCEL_RANGE_xG)
 * @param gyro_range 陀螺仪量程 (MPU6050_GYRO_RANGE_x)
 * @return true成功，false失败
 */
bool mpu6050_init(uint8_t accel_range, uint8_t gyro_range);

/**
 * @brief 读取原始数据
 * @param data 输出数据结构
 * @return true成功，false失败
 */
bool mpu6050_read_raw(mpu6050_data_t *data);

/**
 * @brief 读取处理后的数据（g值和°/s）
 * @param data 输出数据结构
 * @return true成功，false失败
 */
bool mpu6050_read_processed(mpu6050_data_t *data);

/**
 * @brief 检测设备是否在线
 * @return true在线，false离线
 */
bool mpu6050_check_id(void);

/**
 * @brief 软复位MPU6050
 */
void mpu6050_reset(void);

#endif // MPU6050_H
