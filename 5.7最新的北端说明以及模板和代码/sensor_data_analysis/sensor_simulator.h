/**
 * @file sensor_simulator.h
 * @brief 传感器数据模拟器头文件
 * @note 生成模拟传感器数据，格式与 health_monitor_main.c 的 send_data_to_serial() 一致
 * @note JSON格式: {"hr":72,"spo2":98,"temp":36.5,"accel":[x,y,z],"gyro":[x,y,z],
 *               "pitch":1.2,"roll":0.8,"fall_conf":0,"fall_alert":false,
 *               "status":"Status: Normal","hr_source":"max30102",
 *               "valid":{"hr":true,...}}
 */

#ifndef SENSOR_SIMULATOR_H
#define SENSOR_SIMULATOR_H

#include <stdint.h>
#include <stdbool.h>

// 模拟数据结构（匹配 lib 实际输出）
typedef struct {
    // MAX30102 数据
    uint32_t heart_rate;        // 心率 (BPM)
    uint32_t spo2;              // 血氧 (%)

    // MPU6050 数据
    float accel[3];             // 加速度 [x,y,z] (g)
    float gyro[3];              // 陀螺仪 [x,y,z] (°/s)
    float temperature;          // 温度 (℃)

    // 姿态解算（互补滤波输出）
    float pitch;                // 俯仰角 (°)
    float roll;                 // 横滚角 (°)

    // 跌倒检测
    uint8_t fall_confidence;    // 跌倒置信度 (0-100)
    bool fall_alert;            // 跌倒报警状态

    // 系统状态文字
    const char *status;         // "Status: Normal" / "[!] High HR" / "[!!!] FALL!" 等

    // 数据有效性
    bool hr_valid;
    bool spo2_valid;
    bool temp_valid;
    bool imu_valid;
} sensor_data_t;

/**
 * @brief 初始化模拟器
 */
void sensor_sim_init(void);

/**
 * @brief 生成正常范围内的随机数据
 * @param data 输出数据结构
 */
void sensor_sim_generate(sensor_data_t *data);

/**
 * @brief 模拟正常静息状态
 * @param hr 心率值 (BPM)
 * @param data 输出数据结构
 */
void sensor_sim_normal(uint32_t hr, sensor_data_t *data);

/**
 * @brief 模拟运动状态（加速度增大）
 * @param accel_magnitude 加速度大小 (g)
 * @param data 输出数据结构
 */
void sensor_sim_motion(float accel_magnitude, sensor_data_t *data);

/**
 * @brief 模拟跌倒事件
 * @param data 输出数据结构
 */
void sensor_sim_fall(sensor_data_t *data);

/**
 * @brief 模拟心率异常（过高/过低）
 * @param high true=过高, false=过低
 * @param data 输出数据结构
 */
void sensor_sim_hr_abnormal(bool high, sensor_data_t *data);

/**
 * @brief 模拟血氧过低
 * @param data 输出数据结构
 */
void sensor_sim_low_spo2(sensor_data_t *data);

/**
 * @brief 模拟传感器离线（数据无效）
 * @param data 输出数据结构
 */
void sensor_sim_offline(sensor_data_t *data);

/**
 * @brief 以 lib JSON 格式输出数据
 * @param data 输入数据结构
 * @param buffer 输出缓冲区
 * @param size 缓冲区大小
 */
void sensor_sim_to_json(const sensor_data_t *data, char *buffer, size_t size);

/**
 * @brief 以 CSV 格式输出数据
 * @param data 输入数据结构
 * @param buffer 输出缓冲区
 * @param size 缓冲区大小
 */
void sensor_sim_to_csv(const sensor_data_t *data, char *buffer, size_t size);

#endif // SENSOR_SIMULATOR_H
