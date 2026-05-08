/**
 * @file fall_detection.c
 * @brief 老人跌倒检测功能实现（优化版）
 * @note 融合MPU6050加速度/陀螺仪和SW-420震动数据进行跌倒检测
 * @note 优化点：1.加权置信度计算 2.运动状态检测 3.误报过滤
 */

#include "fall_detection.h"
#include "system/system_utils.h"
#include "osal_debug.h"
#include <math.h>

// 跌倒检测状态
static fall_state_t g_fall_state = FALL_STATE_NORMAL;
static fall_detection_callback_t g_fall_callback = NULL;

// 检测数据
static float g_accel[3] = {0};
static float g_gyro[3] = {0};
static bool g_vibration_detected = false;
static float g_pitch = 0;    // 互补滤波输出的俯仰角
static float g_roll = 0;     // 互补滤波输出的横滚角

// 计算数据
static float g_accel_magnitude = 0;
static float g_gyro_magnitude = 0;
static float g_tilt_angle = 0;
static uint8_t g_confidence = 0;

// 时间记录
static uint32_t g_abnormal_start_time = 0;
static bool g_abnormal_detected = false;
static uint32_t g_last_update_time = 0;

// [四柱-监测] 跌倒后处理：冷却期和ALERT状态超时
static uint32_t g_cooldown_start_time = 0;  // 冷却期开始时间
static bool g_in_cooldown = false;          // 是否在冷却期中

// 运动状态检测
static float g_accel_history[5] = {0};  // 加速度历史
static uint8_t g_accel_history_index = 0;
static bool g_is_moving = false;        // 是否在运动


// 计算向量大小
static float calculate_magnitude(float vec[3]){
    return sqrtf(vec[0] * vec[0] + vec[1] * vec[1] + vec[2] * vec[2]);
}

// 计算倾斜角度（使用互补滤波输出的 Pitch/Roll）
// 比纯加速度计算更准确：融合了陀螺仪的瞬时响应，不受振动噪声影响
static float calculate_tilt_angle(float pitch, float roll){
    // 取 Pitch 和 Roll 中绝对值较大的作为倾斜角
    // 人倒下时，Pitch 或 Roll 会从 ~0° 跳到 ~90°
    float abs_pitch = fabsf(pitch);
    float abs_roll = fabsf(roll);
    return (abs_pitch > abs_roll) ? abs_pitch : abs_roll;
}

// 检测是否在运动
static void update_motion_state(float accel_magnitude){
    g_accel_history[g_accel_history_index] = accel_magnitude;
    g_accel_history_index = (g_accel_history_index + 1) % 5;

    // 计算加速度方差
    float sum = 0;
    for(int i = 0; i < 5; i++) {
        sum += g_accel_history[i];
    }
    float mean = sum / 5;

    float variance = 0;
    for(int i = 0; i < 5; i++) {
        float diff = g_accel_history[i] - mean;
        variance += diff * diff;
    }
    variance /= 5;

    // 方差大于阈值认为在运动
    g_is_moving = (variance > 0.1f);
}

// 检查是否为跌倒特征（非运动状态下的异常）
static bool is_fall_characteristic(float accel_mag, float gyro_mag, float tilt_angle){
    // 跌倒特征：高加速度 + 高倾斜角度 + 低角速度（非旋转）
    bool high_accel = (accel_mag > FALL_ACCEL_THRESHOLD);
    bool high_tilt = (tilt_angle > FALL_ANGLE_THRESHOLD);
    bool low_gyro = (gyro_mag < FALL_GYRO_THRESHOLD * 0.5);  // 旋转不剧烈

    // 特征1：高加速度 + 高倾斜（跌倒冲击）
    if(high_accel && high_tilt) {
        return true;
    }

    // 特征2：高加速度 + 低角速度（直接摔倒）
    if(high_accel && low_gyro) {
        return true;
    }

    return false;
}

// 跌倒检测初始化
bool fall_detection_init(void){
    g_fall_state = FALL_STATE_NORMAL;
    g_abnormal_detected = false;
    g_abnormal_start_time = 0;
    g_confidence = 0;
    g_is_moving = false;

    // 初始化历史数据
    for(int i = 0; i < 5; i++) {
        g_accel_history[i] = 1.0f;  // 默认重力加速度
    }

    osal_printk("Fall detection init success\r\n");
    return true;
}

// 更新跌倒检测数据
void fall_detection_update(float accel[3], float gyro[3], bool vibration_detected,
                           float pitch, float roll){
    // 保存原始数据
    for(int i = 0; i < 3; i++) {
        g_accel[i] = accel[i];
        g_gyro[i] = gyro[i];
    }
    g_vibration_detected = vibration_detected;
    g_pitch = pitch;
    g_roll = roll;

    // 计算特征值
    g_accel_magnitude = calculate_magnitude(accel);
    g_gyro_magnitude = calculate_magnitude(gyro);
    g_tilt_angle = calculate_tilt_angle(pitch, roll);

    // 更新运动状态
    update_motion_state(g_accel_magnitude);

    // 跌倒检测算法（优化版）
    uint32_t current_time = get_time_ms();
    bool abnormal_now = false;
    uint8_t current_confidence = 0;

    // 只有在非正常运动状态下才进行跌倒检测
    // 如果用户在正常运动（如跑步），不应该误判为跌倒

    // 条件1：跌倒特征检测（加权更高）
    if(is_fall_characteristic(g_accel_magnitude, g_gyro_magnitude, g_tilt_angle)) {
        abnormal_now = true;
        current_confidence += 50;  // 跌倒特征权重50%
    }

    // 条件2：加速度异常（冲击检测）
    if(g_accel_magnitude > FALL_ACCEL_THRESHOLD) {
        abnormal_now = true;
        current_confidence += 25;  // 加速度异常权重25%
    }

    // 条件3：倾斜角度异常
    if(g_tilt_angle > FALL_ANGLE_THRESHOLD) {
        abnormal_now = true;
        current_confidence += 15;  // 倾斜角度权重15%
    }

    // 条件4：震动传感器辅助
    if(vibration_detected && g_accel_magnitude > 2.0f) {
        current_confidence += 10;  // 震动检测权重10%
    }

    // 限制置信度范围
    if(current_confidence > 100) current_confidence = 100;

    // 运动状态下提高门槛：避免跑步/大幅动作导致误报
    if(g_is_moving && current_confidence < 80) {
        abnormal_now = false;
    }

    g_confidence = current_confidence;

    // [四柱-监测] 冷却期检查：跌倒确认后10秒内不重新触发
    if(g_in_cooldown) {
        if(current_time - g_cooldown_start_time >= FALL_COOLDOWN_MS) {
            g_in_cooldown = false;
            osal_printk("[FALL] Cooldown expired\r\n");
        }
    }

    // 状态机更新
    switch(g_fall_state) {
        case FALL_STATE_NORMAL:
            // 冷却期内不检测
            if(g_in_cooldown) break;

            if(abnormal_now && g_confidence >= 60) {
                // 只有置信度达到60%以上才开始计时
                if(!g_abnormal_detected) {
                    g_abnormal_detected = true;
                    g_abnormal_start_time = current_time;
                } else if(current_time - g_abnormal_start_time >= FALL_DURATION_MS) {
                    // 持续异常超过阈值，判定为跌倒
                    g_fall_state = FALL_STATE_FALLEN;
                    g_confidence = 100;

                    if(g_fall_callback != NULL) {
                        g_fall_callback(g_fall_state);
                    }
                }
            } else if(!abnormal_now || g_confidence < 40) {
                // 异常消失或置信度过低，重置
                g_abnormal_detected = false;
            }
            break;

        case FALL_STATE_FALLEN:
            // 等待用户确认或自动超时（30秒后自动取消）
            if(current_time - g_abnormal_start_time > FALL_ALERT_TIMEOUT_MS) {
                osal_printk("[FALL] Alert timeout, auto-cancel\r\n");
                fall_detection_cancel_alert();
            }
            break;

        case FALL_STATE_ALERT:
            // [四柱-监测] 报警已确认状态：10秒后自动回到NORMAL
            if(current_time - g_abnormal_start_time > FALL_ALERT_TIMEOUT_MS) {
                osal_printk("[FALL] ALERT timeout, back to NORMAL\r\n");
                fall_detection_cancel_alert();
            }
            break;

        default:
            break;
    }

    g_last_update_time = current_time;
}

// 获取当前跌倒状态
fall_state_t fall_detection_get_state(void){
    return g_fall_state;
}

// 注册跌倒检测回调函数
void fall_detection_register_callback(fall_detection_callback_t callback){
    g_fall_callback = callback;
}

// 确认跌倒报警
void fall_detection_confirm(void){
    if(g_fall_state == FALL_STATE_FALLEN) {
        g_fall_state = FALL_STATE_ALERT;
        // [四柱-监测] 确认后启动冷却期，防止确认后立刻重新触发
        g_cooldown_start_time = get_time_ms();
        g_in_cooldown = true;
    }
}

// 取消跌倒报警
void fall_detection_cancel_alert(void){
    g_fall_state = FALL_STATE_NORMAL;
    g_abnormal_detected = false;
    g_confidence = 0;
    g_in_cooldown = false;  // 取消报警时同时清除冷却期
}

// 获取跌倒检测置信度
uint8_t fall_detection_get_confidence(void){
    return g_confidence;
}

// 获取当前倾斜角度
float fall_detection_get_angle(void){
    return g_tilt_angle;
}

// 检查是否在运动
bool fall_detection_is_moving(void){
    return g_is_moving;
}
