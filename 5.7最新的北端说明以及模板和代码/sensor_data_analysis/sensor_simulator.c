/**
 * @file sensor_simulator.c
 * @brief 传感器数据模拟器实现
 * @note 输出格式与 health_monitor_main.c 的 send_data_to_serial() 完全一致
 * @note 用于上位机联调、协议验证、演示
 */

#include "sensor_simulator.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// 模拟器内部状态
static uint32_t g_sim_time = 0;
static uint32_t g_sim_heart_rate = 72;
static uint32_t g_sim_spo2 = 98;
static float g_sim_accel_magnitude = 1.0f;

void sensor_sim_init(void){
    g_sim_time = 0;
    g_sim_heart_rate = 72;
    g_sim_spo2 = 98;
    g_sim_accel_magnitude = 1.0f;
}

// 生成心率脉冲波形（用于模拟 PPG 信号）
static uint32_t generate_heart_rate_signal(uint32_t heart_rate, uint32_t time_ms){
    uint32_t period = 60000 / heart_rate;
    uint32_t phase = time_ms % period;
    float normalized = (float)phase / period;

    if(normalized < 0.1f) {
        return 1000 + (uint32_t)(2000 * normalized / 0.1f);
    } else if(normalized < 0.2f) {
        return 3000 - (uint32_t)(2000 * (normalized - 0.1f) / 0.1f);
    } else {
        return 1000;
    }
}

// 根据心率和血氧状态确定 status 文字
static const char* determine_status(uint32_t hr, uint32_t spo2, float temp,
                                     uint8_t fall_conf, bool hr_valid){
    if(!hr_valid) return "Status: Normal";
    if(fall_conf >= 60) return "[!!!] FALL!";
    if(spo2 > 0 && spo2 < 90) return "[!!] Low SpO2!";
    if(hr > 100) return "[!] High HR";
    if(hr > 0 && hr < 60) return "[!] Low HR";
    if(temp > 37.5f) return "[i] Fever";
    return "Status: Normal";
}

void sensor_sim_generate(sensor_data_t *data){
    if(data == NULL) return;

    g_sim_time += 10;

    // MAX30102
    data->heart_rate = g_sim_heart_rate;
    data->spo2 = g_sim_spo2;
    data->hr_valid = (g_sim_heart_rate > 0);
    data->spo2_valid = (g_sim_spo2 > 0);

    // MPU6050 — 静止时轻微噪声
    float noise = 0.05f * sinf(g_sim_time * 0.001f);
    data->accel[0] = noise;
    data->accel[1] = noise;
    data->accel[2] = g_sim_accel_magnitude + noise;
    data->gyro[0] = 0.1f * sinf(g_sim_time * 0.002f);
    data->gyro[1] = 0.1f * cosf(g_sim_time * 0.0015f);
    data->gyro[2] = 0.05f * sinf(g_sim_time * 0.001f);
    data->temperature = 36.5f + 0.1f * sinf(g_sim_time * 0.0001f);
    data->imu_valid = true;
    data->temp_valid = true;

    // 姿态解算：站立时 pitch/roll 接近 0
    data->pitch = 0.0f;
    data->roll = 0.0f;

    // 跌倒
    data->fall_confidence = 0;
    data->fall_alert = false;

    // 状态
    data->status = determine_status(data->heart_rate, data->spo2,
                                     data->temperature, data->fall_confidence,
                                     data->hr_valid);
}

void sensor_sim_normal(uint32_t hr, sensor_data_t *data){
    g_sim_heart_rate = hr;
    g_sim_spo2 = 98;
    g_sim_accel_magnitude = 1.0f;
    sensor_sim_generate(data);
}

void sensor_sim_motion(float accel_magnitude, sensor_data_t *data){
    g_sim_accel_magnitude = accel_magnitude;
    sensor_sim_generate(data);
}

void sensor_sim_fall(sensor_data_t *data){
    if(data == NULL) return;

    // 模拟跌倒冲击：高加速度 + 高角速度 + 震动
    data->accel[0] = 2.5f;
    data->accel[1] = 1.8f;
    data->accel[2] = 3.2f;
    data->gyro[0] = 15.0f;
    data->gyro[1] = 8.0f;
    data->gyro[2] = 12.0f;
    // 跌倒时姿态突变
    data->pitch = 45.2f;
    data->roll = 12.3f;
    data->fall_confidence = 100;
    data->fall_alert = true;
    data->status = "[!!!] FALL!";
    data->imu_valid = true;
}

void sensor_sim_hr_abnormal(bool high, sensor_data_t *data){
    if(data == NULL) return;

    if(high) {
        data->heart_rate = 115;
        data->status = "[!] High HR";
    } else {
        data->heart_rate = 45;
        data->status = "[!] Low HR";
    }
    data->spo2 = 97;
    data->fall_confidence = 0;
    data->fall_alert = false;
    data->pitch = 0.0f;
    data->roll = 0.0f;
    data->hr_valid = true;
    data->spo2_valid = true;
    data->imu_valid = true;
    data->temp_valid = true;
}

void sensor_sim_low_spo2(sensor_data_t *data){
    if(data == NULL) return;

    data->heart_rate = 75;
    data->spo2 = 85;
    data->fall_confidence = 0;
    data->fall_alert = false;
    data->pitch = 0.0f;
    data->roll = 0.0f;
    data->status = "[!!] Low SpO2!";
    data->hr_valid = true;
    data->spo2_valid = true;
    data->imu_valid = true;
    data->temp_valid = true;
}

void sensor_sim_offline(sensor_data_t *data){
    if(data == NULL) return;

    data->heart_rate = 0;
    data->spo2 = 0;
    data->temperature = 0;
    data->accel[0] = 0; data->accel[1] = 0; data->accel[2] = 0;
    data->gyro[0] = 0; data->gyro[1] = 0; data->gyro[2] = 0;
    data->fall_confidence = 0;
    data->fall_alert = false;
    data->pitch = 0.0f;
    data->roll = 0.0f;
    data->hr_valid = false;
    data->spo2_valid = false;
    data->temp_valid = false;
    data->imu_valid = false;
    data->status = "Status: Normal";
}

// ==================== 输出格式 ====================

/**
 * @brief JSON 格式输出（与 lib send_data_to_serial 完全一致）
 *
 * 示例输出：
 * {"hr":72,"spo2":98,"temp":36.5,"accel":[0.10,0.05,1.00],
 *  "gyro":[0.10,0.05,0.02],"pitch":1.2,"roll":0.8,
 *  "fall_conf":0,"fall_alert":false,"status":"Status: Normal",
 *  "hr_source":"max30102",
 *  "valid":{"hr":true,"spo2":true,"temp":true,"imu":true}}
 */
void sensor_sim_to_json(const sensor_data_t *data, char *buffer, size_t size){
    if(data == NULL || buffer == NULL) return;

    snprintf(buffer, size,
        "{\"hr\":%lu,\"spo2\":%lu,\"temp\":%.1f,"
        "\"accel\":[%.2f,%.2f,%.2f],"
        "\"gyro\":[%.2f,%.2f,%.2f],"
        "\"pitch\":%.1f,\"roll\":%.1f,"
        "\"fall_conf\":%d,\"fall_alert\":%s,"
        "\"status\":\"%s\","
        "\"hr_source\":\"max30102\","
        "\"valid\":{\"hr\":%s,\"spo2\":%s,\"temp\":%s,\"imu\":%s}}",
        data->heart_rate, data->spo2, data->temperature,
        data->accel[0], data->accel[1], data->accel[2],
        data->gyro[0], data->gyro[1], data->gyro[2],
        data->pitch, data->roll,
        data->fall_confidence,
        data->fall_alert ? "true" : "false",
        data->status ? data->status : "Unknown",
        data->hr_valid ? "true" : "false",
        data->spo2_valid ? "true" : "false",
        data->temp_valid ? "true" : "false",
        data->imu_valid ? "true" : "false"
    );
}

/**
 * @brief CSV 格式输出
 * 表头: HR,SpO2,Temp,Accel_X,Accel_Y,Accel_Z,Gyro_X,Gyro_Y,Gyro_Z,Pitch,Roll,Fall_Conf,Fall_Alert,Status
 */
void sensor_sim_to_csv(const sensor_data_t *data, char *buffer, size_t size){
    if(data == NULL || buffer == NULL) return;

    snprintf(buffer, size,
        "%lu,%lu,%.1f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.1f,%.1f,%d,%s,%s",
        data->heart_rate, data->spo2, data->temperature,
        data->accel[0], data->accel[1], data->accel[2],
        data->gyro[0], data->gyro[1], data->gyro[2],
        data->pitch, data->roll,
        data->fall_confidence,
        data->fall_alert ? "true" : "false",
        data->status ? data->status : "Unknown"
    );
}
