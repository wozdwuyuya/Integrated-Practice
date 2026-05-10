/**
 * @file sensor_data_template.c
 * @brief 传感器数据处理模板（与 lib 同步版）
 * @note 数据结构、JSON 格式、检测逻辑均与 health_monitor_main.c 保持一致
 * @note 可作为上位机协议验证、新功能开发参考
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

// ==================== 数据结构定义 ====================
// 与 health_monitor_main.c 中的全局变量一一对应

typedef struct {
    uint32_t heart_rate;    // 心率 (BPM)
    uint32_t spo2;          // 血氧 (%)
    bool hr_valid;          // 心率数据有效
    bool spo2_valid;        // 血氧数据有效
} max30102_data_t;

typedef struct {
    float accel[3];         // 加速度 [x,y,z] (g)
    float gyro[3];          // 陀螺仪 [x,y,z] (°/s)
    float temperature;      // 温度 (℃)
    bool imu_valid;         // IMU 数据有效
    bool temp_valid;        // 温度数据有效
} mpu6050_data_t;

typedef struct {
    bool detected;          // 检测到震动
    uint32_t count;         // 震动累计次数
} sw420_data_t;

// 跌倒检测状态（与 fall_detection.h 一致）
typedef enum {
    FALL_STATE_NORMAL = 0,
    FALL_STATE_FALLING,
    FALL_STATE_FALLEN,
    FALL_STATE_ALERT
} fall_state_t;

// 系统状态（与 health_monitor_main.h 一致）
typedef enum {
    SYS_STATE_INIT = 0,
    SYS_STATE_NORMAL,
    SYS_STATE_ALERT,
    SYS_STATE_BREATH_GUIDE,
    SYS_STATE_SLE_CONNECTED
} system_state_t;

// 健康状态（与 health_alert.h 一致）
typedef enum {
    HEALTH_NORMAL = 0,
    HEALTH_HIGH_HR,
    HEALTH_LOW_HR,
    HEALTH_LOW_SPO2,
    HEALTH_FALL_DETECTED,
    HEALTH_FEVER
} health_status_t;

// 报警等级（与 health_alert.h 一致）
typedef enum {
    ALERT_LEVEL_NONE = 0,
    ALERT_LEVEL_INFO,
    ALERT_LEVEL_WARNING,
    ALERT_LEVEL_DANGER
} alert_level_t;

// 综合数据
typedef struct {
    max30102_data_t max30102;
    mpu6050_data_t mpu6050;
    sw420_data_t sw420;
    float pitch;                // 俯仰角 (°)，互补滤波输出
    float roll;                 // 横滚角 (°)，互补滤波输出
    uint8_t fall_confidence;    // 跌倒置信度 0-100
    bool fall_alert;            // 跌倒报警状态
    fall_state_t fall_state;
    health_status_t health_status;
    alert_level_t alert_level;
    system_state_t sys_state;
    uint32_t timestamp;
} sensor_all_data_t;

// ==================== 阈值常量 ====================
// 与 lib 中的 #define 保持一致

#define HR_HIGH_THRESHOLD        100     // 心率上限 (BPM)
#define HR_LOW_THRESHOLD         60      // 心率下限 (BPM)
#define SPO2_LOW_THRESHOLD       90      // 血氧下限 (%)
#define TEMP_HIGH_THRESHOLD      37.5f   // 温度上限 (℃)
#define HR_HYSTERESIS            5       // 心率滞后 ±5 BPM
#define SPO2_HYSTERESIS          2       // 血氧滞后 ±2%

#define FALL_ACCEL_THRESHOLD     2.5f    // 跌倒加速度阈值 (g)
#define FALL_ANGLE_THRESHOLD     45.0f   // 跌倒倾斜角度阈值 (°)
#define FALL_DURATION_MS         3000    // 跌倒持续时间阈值 (ms)
#define FALL_COOLDOWN_MS         10000   // 跌倒冷却期 (ms)

// ==================== 数据解析函数 ====================

/**
 * @brief 解析 MAX30102 原始 FIFO 数据
 * @param raw_data 6字节原始数据 [红光3字节 + 红外3字节]
 * @param data 输出
 */
void parse_max30102_data(uint8_t *raw_data, max30102_data_t *data){
    if(raw_data == NULL || data == NULL) return;

    uint32_t red = ((uint32_t)raw_data[0] << 16) |
                   ((uint32_t)raw_data[1] << 8) |
                   raw_data[2];
    red >>= 6;

    uint32_t ir = ((uint32_t)raw_data[3] << 16) |
                  ((uint32_t)raw_data[4] << 8) |
                  raw_data[5];
    ir >>= 6;

    // 心率血氧算法由 max30102.c 的 heart_rate_enable_in() 计算
    // 这里只做原始数据解析示例
    (void)red; (void)ir;
    data->hr_valid = true;
    data->spo2_valid = true;
}

/**
 * @brief 解析 MPU6050 原始数据
 * @param raw_data 14字节原始数据
 * @param data 输出
 * @param accel_range 加速度量程 (0x00=±2g, 0x08=±4g, ...)
 * @param gyro_range 陀螺仪量程
 */
void parse_mpu6050_data(uint8_t *raw_data, mpu6050_data_t *data,
                        uint8_t accel_range, uint8_t gyro_range){
    if(raw_data == NULL || data == NULL) return;

    float accel_scale;
    switch(accel_range) {
        case 0x00: accel_scale = 16384.0f; break;
        case 0x08: accel_scale = 8192.0f;  break;
        case 0x10: accel_scale = 4096.0f;  break;
        case 0x18: accel_scale = 2048.0f;  break;
        default:   accel_scale = 16384.0f; break;
    }

    float gyro_scale;
    switch(gyro_range) {
        case 0x00: gyro_scale = 131.0f;  break;
        case 0x08: gyro_scale = 65.5f;   break;
        case 0x10: gyro_scale = 32.8f;   break;
        case 0x18: gyro_scale = 16.4f;   break;
        default:   gyro_scale = 131.0f;  break;
    }

    int16_t raw;

    raw = (int16_t)((raw_data[0] << 8) | raw_data[1]);
    data->accel[0] = raw / accel_scale;
    raw = (int16_t)((raw_data[2] << 8) | raw_data[3]);
    data->accel[1] = raw / accel_scale;
    raw = (int16_t)((raw_data[4] << 8) | raw_data[5]);
    data->accel[2] = raw / accel_scale;

    raw = (int16_t)((raw_data[6] << 8) | raw_data[7]);
    data->temperature = raw / 340.0f + 36.53f;

    raw = (int16_t)((raw_data[8] << 8) | raw_data[9]);
    data->gyro[0] = raw / gyro_scale;
    raw = (int16_t)((raw_data[10] << 8) | raw_data[11]);
    data->gyro[1] = raw / gyro_scale;
    raw = (int16_t)((raw_data[12] << 8) | raw_data[13]);
    data->gyro[2] = raw / gyro_scale;

    data->imu_valid = true;
    data->temp_valid = true;  // 读取成功即有效（温度公式 raw/340+36.53 始终非零）
}

// ==================== 检测逻辑 ====================
// 与 lib 中 health_alert.c / fall_detection.c 的逻辑对齐

/**
 * @brief 健康异常检测（带滞后防抖）
 * @param data 综合数据
 * @param hr_high_triggered 上次心率高触发状态（输入/输出）
 * @param hr_low_triggered 上次心率低触发状态（输入/输出）
 * @param spo2_triggered 上次血氧触发状态（输入/输出）
 * @return 健康状态
 */
health_status_t check_health_status(sensor_all_data_t *data,
                                     bool *hr_high_triggered,
                                     bool *hr_low_triggered,
                                     bool *spo2_triggered){
    if(!data->max30102.hr_valid) return HEALTH_NORMAL;

    // 心率高（带滞后）
    if(!(*hr_high_triggered)) {
        if(data->max30102.heart_rate > HR_HIGH_THRESHOLD) {
            *hr_high_triggered = true;
            return HEALTH_HIGH_HR;
        }
    } else {
        if(data->max30102.heart_rate < HR_HIGH_THRESHOLD - HR_HYSTERESIS) {
            *hr_high_triggered = false;
        } else {
            return HEALTH_HIGH_HR;
        }
    }

    // 心率低（带滞后）
    if(!(*hr_low_triggered)) {
        if(data->max30102.heart_rate > 0 &&
           data->max30102.heart_rate < HR_LOW_THRESHOLD) {
            *hr_low_triggered = true;
            return HEALTH_LOW_HR;
        }
    } else {
        if(data->max30102.heart_rate > HR_LOW_THRESHOLD + HR_HYSTERESIS) {
            *hr_low_triggered = false;
        } else {
            return HEALTH_LOW_HR;
        }
    }

    // 血氧低（带滞后）
    if(data->max30102.spo2_valid) {
        if(!(*spo2_triggered)) {
            if(data->max30102.spo2 > 0 &&
               data->max30102.spo2 < SPO2_LOW_THRESHOLD) {
                *spo2_triggered = true;
                return HEALTH_LOW_SPO2;
            }
        } else {
            if(data->max30102.spo2 > SPO2_LOW_THRESHOLD + SPO2_HYSTERESIS) {
                *spo2_triggered = false;
            } else {
                return HEALTH_LOW_SPO2;
            }
        }
    }

    // 发烧
    if(data->mpu6050.temp_valid &&
       data->mpu6050.temperature > TEMP_HIGH_THRESHOLD) {
        return HEALTH_FEVER;
    }

    return HEALTH_NORMAL;
}

/**
 * @brief 根据健康状态确定报警等级
 * @note 与 health_alert.c 的 determine_alert_level() 一致
 */
alert_level_t determine_alert_level(health_status_t status){
    switch(status) {
        case HEALTH_FALL_DETECTED:
        case HEALTH_LOW_SPO2:
            return ALERT_LEVEL_DANGER;
        case HEALTH_HIGH_HR:
        case HEALTH_LOW_HR:
            return ALERT_LEVEL_WARNING;
        case HEALTH_FEVER:
            return ALERT_LEVEL_INFO;
        default:
            return ALERT_LEVEL_NONE;
    }
}

/**
 * @brief 多特征融合跌倒检测
 * @note 与 fall_detection.c 的算法逻辑对齐
 * @note 简化版：省略了运动状态检测（方差判断），实际 lib 中运动中置信度<80不触发
 * @note 倾斜角改用 pitch/roll（互补滤波输出），与 lib 实际一致
 * @return 跌倒置信度 0-100
 */
uint8_t check_fall_detection(sensor_all_data_t *data){
    if(!data->mpu6050.imu_valid) return 0;

    float accel_mag = sqrtf(
        data->mpu6050.accel[0] * data->mpu6050.accel[0] +
        data->mpu6050.accel[1] * data->mpu6050.accel[1] +
        data->mpu6050.accel[2] * data->mpu6050.accel[2]
    );
    float gyro_mag = sqrtf(
        data->mpu6050.gyro[0] * data->mpu6050.gyro[0] +
        data->mpu6050.gyro[1] * data->mpu6050.gyro[1] +
        data->mpu6050.gyro[2] * data->mpu6050.gyro[2]
    );

    // 倾斜角：取 pitch/roll 绝对值较大者（互补滤波输出，与 lib 一致）
    float tilt_angle = fabsf(data->pitch) > fabsf(data->roll)
                     ? fabsf(data->pitch) : fabsf(data->roll);

    uint8_t confidence = 0;

    // 特征1: 跌倒特征（高加速度 + 高倾斜 + 低角速度）→ 50%
    bool high_accel = (accel_mag > FALL_ACCEL_THRESHOLD);
    bool high_tilt = (tilt_angle > FALL_ANGLE_THRESHOLD);
    bool low_gyro = (gyro_mag < 25.0f);
    if((high_accel && high_tilt) || (high_accel && low_gyro)) {
        confidence += 50;
    }

    // 特征2: 加速度异常 → 25%
    if(high_accel) {
        confidence += 25;
    }

    // 特征3: 倾斜角度异常 → 15%
    if(tilt_angle > FALL_ANGLE_THRESHOLD) {
        confidence += 15;
    }

    // 特征4: 震动辅助 → 10%
    if(data->sw420.detected && accel_mag > 2.0f) {
        confidence += 10;
    }

    if(confidence > 100) confidence = 100;
    return confidence;
}

// ==================== 格式化输出 ====================

/**
 * @brief JSON 格式输出（与 lib send_data_to_serial 完全一致）
 */
void format_serial_json(const sensor_all_data_t *data, char *buffer, size_t size){
    if(data == NULL || buffer == NULL) return;

    const char *status_str = "Status: Normal";
    if(data->health_status == HEALTH_HIGH_HR) status_str = "[!] High HR";
    else if(data->health_status == HEALTH_LOW_HR) status_str = "[!] Low HR";
    else if(data->health_status == HEALTH_LOW_SPO2) status_str = "[!!] Low SpO2!";
    else if(data->health_status == HEALTH_FALL_DETECTED) status_str = "[!!!] FALL!";
    else if(data->health_status == HEALTH_FEVER) status_str = "[i] Fever";

    snprintf(buffer, size,
        "{\"hr\":%lu,\"spo2\":%lu,\"temp\":%.1f,"
        "\"accel\":[%.2f,%.2f,%.2f],"
        "\"gyro\":[%.2f,%.2f,%.2f],"
        "\"pitch\":%.1f,\"roll\":%.1f,"
        "\"fall_conf\":%d,\"fall_alert\":%s,"
        "\"status\":\"%s\","
        "\"hr_source\":\"max30102\","
        "\"valid\":{\"hr\":%s,\"spo2\":%s,\"temp\":%s,\"imu\":%s}}",
        data->max30102.heart_rate, data->max30102.spo2,
        data->mpu6050.temperature,
        data->mpu6050.accel[0], data->mpu6050.accel[1], data->mpu6050.accel[2],
        data->mpu6050.gyro[0], data->mpu6050.gyro[1], data->mpu6050.gyro[2],
        data->pitch, data->roll,
        data->fall_confidence,
        data->fall_alert ? "true" : "false",
        status_str,
        data->max30102.hr_valid ? "true" : "false",
        data->max30102.spo2_valid ? "true" : "false",
        data->mpu6050.temp_valid ? "true" : "false",
        data->mpu6050.imu_valid ? "true" : "false"
    );
}

// ==================== 主程序示例 ====================

int main(void){
    sensor_all_data_t sensor_data;
    char json_buf[512];

    memset(&sensor_data, 0, sizeof(sensor_data));

    // 滞后状态
    bool hr_high_triggered = false;
    bool hr_low_triggered = false;
    bool spo2_triggered = false;

    printf("=== 传感器数据处理模板 (lib 同步版) ===\r\n");
    printf("JSON 格式与 health_monitor_main.c send_data_to_serial() 一致\r\n\r\n");

    // 模拟场景序列
    struct {
        const char *scene;
        uint32_t hr;
        uint32_t spo2;
        float accel;
        float pitch;
        float roll;
        bool fall_alert;
    } scenes[] = {
        {"正常静息",   72, 98, 1.0f,  1.2f,  0.8f, false},
        {"正常静息",   75, 97, 1.0f,  0.5f,  0.3f, false},
        {"心率偏高",  105, 96, 1.0f,  2.1f,  1.5f, false},
        {"心率偏高",  110, 95, 1.0f,  1.8f,  1.2f, false},
        {"恢复正常",   80, 98, 1.0f,  0.8f,  0.5f, false},
        {"血氧过低",   78, 85, 1.0f,  1.0f,  0.6f, false},
        {"恢复正常",   74, 97, 1.0f,  0.9f,  0.4f, false},
        {"跌倒冲击",   72, 98, 3.5f, 45.2f, 12.3f, true},
    };
    int scene_count = sizeof(scenes) / sizeof(scenes[0]);

    for(int i = 0; i < scene_count; i++) {
        // 填充数据
        sensor_data.max30102.heart_rate = scenes[i].hr;
        sensor_data.max30102.spo2 = scenes[i].spo2;
        sensor_data.max30102.hr_valid = true;
        sensor_data.max30102.spo2_valid = true;

        sensor_data.mpu6050.accel[0] = 0.1f;
        sensor_data.mpu6050.accel[1] = 0.05f;
        sensor_data.mpu6050.accel[2] = scenes[i].accel;
        sensor_data.mpu6050.temperature = 36.5f;
        sensor_data.mpu6050.imu_valid = true;
        sensor_data.mpu6050.temp_valid = true;

        sensor_data.pitch = scenes[i].pitch;
        sensor_data.roll = scenes[i].roll;
        sensor_data.fall_alert = scenes[i].fall_alert;

        // 检测
        sensor_data.health_status = check_health_status(&sensor_data,
            &hr_high_triggered, &hr_low_triggered, &spo2_triggered);
        sensor_data.alert_level = determine_alert_level(sensor_data.health_status);
        sensor_data.fall_confidence = check_fall_detection(&sensor_data);

        // 输出
        format_serial_json(&sensor_data, json_buf, sizeof(json_buf));
        printf("[%s] %s\r\n", scenes[i].scene, json_buf);
    }

    return 0;
}
