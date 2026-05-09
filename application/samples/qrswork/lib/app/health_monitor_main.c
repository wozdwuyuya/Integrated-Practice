/**
 * @file health_monitor_main.c
 * @brief 智能健康监测系统主程序实现
 * @note [四柱-监测] 数据质量评估
 * @note [四柱-架构] 系统状态机
 * @note 整合MAX30102、MPU6050、KY-039、SW-420、OLED、RGB、蜂鸣器、震动马达、SLE通信
 */

#include "app/health_monitor_main.h"
#include "system/i2c_master.h"
#include "system/system_utils.h"
#include <string.h>
#include <stdio.h>
#if MOCK_HARDWARE_MODE
#include <math.h>
#endif
#include "sensor/max30102.h"
#include "sensor/mpu6050.h"
#include "sensor/sw420.h"
#include "sensor/ky039.h"
#include "output/rgb_led.h"
#include "output/beep.h"
#include "output/vibration_motor.h"
#include "algorithm/fall_detection.h"
#include "algorithm/breath_guide.h"
#include "algorithm/health_alert.h"
#include "algorithm/attitude_estimation.h"
#include "output/ssd1306.h"
#include "sle_comm.h"

// 系统状态
static system_state_t g_system_state = SYS_STATE_INIT;
static uint32_t g_last_update_time = 0;
static uint32_t g_last_oled_time = 0;
static uint32_t g_last_data_send_time = 0;

// 传感器数据
static float g_accel[3] = {0};
static float g_gyro[3] = {0};
static uint32_t g_heart_rate = 0;
static uint32_t g_spo2 = 0;
static float g_temperature = 0;

// [四柱-监测] 数据有效性标记
static bool g_hr_valid = false;       // 心率数据有效
static bool g_spo2_valid = false;     // 血氧数据有效
static bool g_temp_valid = false;     // 温度数据有效
static bool g_imu_valid = false;      // IMU数据有效
static uint32_t g_last_hr_update = 0; // 心率最后有效更新时间

// [四柱-监测] 心率源：MAX30102为主，KY-039为备
static bool g_hr_source_max30102 = true;  // true=MAX30102, false=KY-039

// [MOCK] 模拟数据生成器：有规律波动的假数据，用于无硬件调试
#if MOCK_HARDWARE_MODE
static uint32_t g_mock_tick = 0;

static void mock_generate_data(void){
    g_mock_tick++;

    // 心率：60~100 BPM 正弦波动，周期约20秒
    g_heart_rate = 72 + (int)(20.0f * sinf((float)g_mock_tick * 0.05f));
    g_spo2 = 97 + (int)(2.0f * sinf((float)g_mock_tick * 0.03f));
    g_temperature = 36.5f + 0.5f * sinf((float)g_mock_tick * 0.01f);

    // 加速度：模拟站立姿态，偶有晃动
    g_accel[0] = 0.1f * sinf((float)g_mock_tick * 0.08f);
    g_accel[1] = 0.05f * cosf((float)g_mock_tick * 0.06f);
    g_accel[2] = 1.0f + 0.02f * sinf((float)g_mock_tick * 0.1f);

    // 陀螺仪：微小角速度
    g_gyro[0] = 2.0f * sinf((float)g_mock_tick * 0.08f);
    g_gyro[1] = 1.5f * cosf((float)g_mock_tick * 0.06f);
    g_gyro[2] = 0.1f * sinf((float)g_mock_tick * 0.04f);

    // 标记所有数据有效
    g_hr_valid = true;
    g_spo2_valid = true;
    g_temp_valid = true;
    g_imu_valid = true;
    g_hr_source_max30102 = true;
}
#endif

// [四柱-架构] 状态名称表（用于日志输出）
static const char* state_names[] = {
    "INIT", "NORMAL", "ALERT", "BREATH_GUIDE", "SLE_CONNECTED"
};

// 前向声明：SLE数据接收回调（定义在文件末尾）
static void sle_data_callback(uint8_t *data, uint16_t len);

// 跌倒状态回调适配（fall_state_t -> bool）
static void fall_state_callback(fall_state_t state){
    health_alert_update_fall(state == FALL_STATE_FALLEN);
}

// [四柱-架构] 状态转换函数（带日志）
static void state_transition(system_state_t new_state, const char *reason){
    if(g_system_state == new_state) return;
    osal_printk("[STATE] %s -> %s (reason: %s)\r\n",
           state_names[g_system_state], state_names[new_state], reason);
    g_system_state = new_state;
}

// 系统初始化
bool health_monitor_init(void){
    bool init_ok = true;
    osal_printk("=== Smart Health Monitor System ===\r\n");
    osal_printk("Mode: %s\r\n", MOCK_HARDWARE_MODE ? "MOCK" : "REAL");

#if MOCK_HARDWARE_MODE
    osal_printk("[MOCK] Skipping hardware init, using simulated data\r\n");
#else
    // 初始化I2C总线及设备（SSD1306、MAX30102、MPU6050）
    all_i2c_init();

    // 初始化其他硬件
    if(!sw420_init()) {
        osal_printk("SW420 init failed!\r\n");
        init_ok = false;
    }

    if(!ky039_init()) {
        osal_printk("KY039 init failed!\r\n");
        init_ok = false;
    }

    if(!rgb_led_init()) {
        osal_printk("RGB LED init failed!\r\n");
        init_ok = false;
    }

    if(!vibration_motor_init()) {
        osal_printk("Vibration motor init failed!\r\n");
        init_ok = false;
    }

    if(!beep_init()) {
        osal_printk("Beep init failed!\r\n");
        init_ok = false;
    }
#endif

    // 算法初始化始终执行（不依赖硬件）
    if(!fall_detection_init()) {
        osal_printk("Fall detection init failed!\r\n");
        init_ok = false;
    }

    // 初始化姿态解算（互补滤波，α=0.96）
    attitude_init(0.96f);

    // 初始化呼吸引导
    breath_guide_init();

    // 初始化健康提醒系统
    health_alert_init();

    // 注册跌倒检测回调
    fall_detection_register_callback(fall_state_callback);

#if !MOCK_HARDWARE_MODE
    // 初始化SLE通信（仅真实模式）
    if(!sle_comm_init()) {
        osal_printk("SLE init failed!\r\n");
    } else {
        // 注册数据接收回调
        sle_comm_register_callback(sle_data_callback);
        // 启动广播
        sle_comm_start_adv();
    }

    // 设置OLED初始显示
    ssd1306_Fill(Black);
    ssd1306_SetCursor(0, 0);
    ssd1306_DrawString("Health Monitor", Font_7x10, White);
    ssd1306_SetCursor(0, 12);
    ssd1306_DrawString("Initializing...", Font_7x10, White);
    ssd1306_UpdateScreen();
#endif

    if(init_ok) {
        state_transition(SYS_STATE_NORMAL, "init complete");
    }
    if(init_ok) {
        osal_printk("System init success!\r\n");
    } else {
        osal_printk("System init finished with errors!\r\n");
    }

    return init_ok;
}

// 更新传感器数据
static void update_sensor_data(void){
    uint32_t now = get_time_ms();

#if MOCK_HARDWARE_MODE
    // Mock模式：生成模拟数据
    mock_generate_data();
#else
    // 真实模式：读取MAX30102心率血氧（由app_main主循环单线程采样，这里只消费结果）
    if(return_ac[0] > 0) {
        g_heart_rate = return_ac[0];
        g_spo2 = return_ac[1];
        g_hr_valid = true;
        g_spo2_valid = (g_spo2 > 0);
        g_hr_source_max30102 = true;
        g_last_hr_update = now;
    }

    // [四柱-监测] 心率过期检查：超过DATA_STALE_TIMEOUT_MS无更新视为无效
    if(g_hr_valid && (now - g_last_hr_update > DATA_STALE_TIMEOUT_MS)) {
        g_hr_valid = false;
        g_spo2_valid = false;
    }

    // [四柱-监测] KY-039备用心率：MAX30102无效时自动切换
    if(!g_hr_valid) {
        ky039_data_t ky_data;
        if(ky039_read_heart_rate(&ky_data) && ky_data.valid) {
            g_heart_rate = ky_data.heart_rate;
            g_hr_valid = true;
            g_hr_source_max30102 = false;
            g_last_hr_update = now;
        }
    }

    // 读取MPU6050数据
    mpu6050_data_t mpu_data;
    if(mpu6050_read_processed(&mpu_data)) {
        g_accel[0] = mpu_data.accel_g[0];
        g_accel[1] = mpu_data.accel_g[1];
        g_accel[2] = mpu_data.accel_g[2];
        g_gyro[0] = mpu_data.gyro_dps[0];
        g_gyro[1] = mpu_data.gyro_dps[1];
        g_gyro[2] = mpu_data.gyro_dps[2];
        g_temperature = mpu_data.temperature;
        g_imu_valid = true;
        g_temp_valid = true;
    } else {
        g_imu_valid = false;
        g_temp_valid = false;
    }
#endif

    // 姿态解算：融合加速度+陀螺仪，输出 Pitch/Roll（两种模式都执行）
    float dt = (float)SYSTEM_UPDATE_INTERVAL_MS / 1000.0f;
    attitude_update(g_accel, g_gyro, dt);

    // 更新跌倒检测（传入互补滤波输出的 Pitch/Roll）
#if MOCK_HARDWARE_MODE
    fall_detection_update(g_accel, g_gyro, false,
                          attitude_get_pitch(), attitude_get_roll());
#else
    fall_detection_update(g_accel, g_gyro, sw420_read(),
                          attitude_get_pitch(), attitude_get_roll());
#endif

    // 更新健康提醒
    health_alert_update(g_heart_rate, g_spo2, g_temperature);

    // 检查跌倒状态
    if(fall_detection_get_state() == FALL_STATE_FALLEN) {
        health_alert_update_fall(true);
    }
}

// [四柱-架构] 更新OLED显示（状态驱动）
static void update_oled_display(void){
    char buf[32];

    switch(g_system_state) {
        case SYS_STATE_ALERT:
            // 报警状态：全屏显示报警信息
            ssd1306_Fill(Black);
            ssd1306_SetCursor(0, 0);
            ssd1306_DrawString("!!! ALERT !!!", Font_7x10, White);
            health_alert_display(14);
            ssd1306_SetCursor(0, 30);
            if(g_hr_valid) {
                snprintf(buf, sizeof(buf), "HR:%lu SpO2:%lu%%", g_heart_rate, g_spo2);
            } else {
                snprintf(buf, sizeof(buf), "HR:--- SpO2:---%%");
            }
            ssd1306_DrawString(buf, Font_7x10, White);
            ssd1306_SetCursor(0, 44);
            snprintf(buf, sizeof(buf), "Btn1:Confirm      ");
            ssd1306_DrawString(buf, Font_7x10, White);
            break;

        case SYS_STATE_BREATH_GUIDE:
            // 呼吸引导状态：显示引导动画
            ssd1306_Fill(Black);
            breath_guide_display(0);
            break;

        default:
            // 正常状态：显示完整数据
            // 第1行：心率血氧（无效时显示"---"）
            ssd1306_SetCursor(0, 0);
            if(g_hr_valid) {
                snprintf(buf, sizeof(buf), "HR:%lu SpO2:%lu%%   ", g_heart_rate, g_spo2);
            } else {
                snprintf(buf, sizeof(buf), "HR:--- SpO2:---%%   ");
            }
            ssd1306_DrawString(buf, Font_7x10, White);

            // 第2行：温度
            ssd1306_SetCursor(0, 12);
            if(g_temp_valid) {
                snprintf(buf, sizeof(buf), "Temp:%.1fC         ", g_temperature);
            } else {
                snprintf(buf, sizeof(buf), "Temp:---C          ");
            }
            ssd1306_DrawString(buf, Font_7x10, White);

            // 第3行：加速度
            ssd1306_SetCursor(0, 24);
            if(g_imu_valid) {
                snprintf(buf, sizeof(buf), "A:%.1f,%.1f,%.1f   ", g_accel[0], g_accel[1], g_accel[2]);
            } else {
                snprintf(buf, sizeof(buf), "A:---,---,---      ");
            }
            ssd1306_DrawString(buf, Font_7x10, White);

            // 第4行：健康状态
            health_alert_display(36);

            // 第5行：跌倒检测状态
            ssd1306_SetCursor(0, 52);
            if(fall_detection_get_state() != FALL_STATE_NORMAL) {
                snprintf(buf, sizeof(buf), "Fall:%d%%   ", fall_detection_get_confidence());
                ssd1306_DrawString(buf, Font_7x10, White);
            } else {
                ssd1306_DrawString("                    ", Font_7x10, Black);
            }
            break;
    }

    ssd1306_UpdateScreen();
}

// 发送数据到上位机（串口）
static void send_data_to_serial(void){
    char buf[400];

    // [四柱-监测] JSON格式输出，包含 pitch/roll/fall_alert 新字段
    snprintf(buf, sizeof(buf),
        "{\"hr\":%lu,\"spo2\":%lu,\"temp\":%.1f,"
        "\"accel\":[%.2f,%.2f,%.2f],"
        "\"gyro\":[%.2f,%.2f,%.2f],"
        "\"pitch\":%.1f,\"roll\":%.1f,"
        "\"fall_conf\":%d,\"fall_alert\":%s,"
        "\"status\":\"%s\","
        "\"hr_source\":\"%s\","
        "\"valid\":{\"hr\":%s,\"spo2\":%s,\"temp\":%s,\"imu\":%s}}\r\n",
        g_heart_rate, g_spo2, g_temperature,
        g_accel[0], g_accel[1], g_accel[2],
        g_gyro[0], g_gyro[1], g_gyro[2],
        attitude_get_pitch(), attitude_get_roll(),
        fall_detection_get_confidence(),
        (fall_detection_get_state() == FALL_STATE_FALLEN) ? "true" : "false",
        health_alert_get_text(),
        g_hr_source_max30102 ? "max30102" : "ky039",
        g_hr_valid ? "true" : "false",
        g_spo2_valid ? "true" : "false",
        g_temp_valid ? "true" : "false",
        g_imu_valid ? "true" : "false"
    );

    osal_printk("%s", buf);
}

// 发送传感器数据到手机端（通过SLE）
void health_monitor_send_data(void){
    char buf[400];

    if(!sle_comm_is_connected()) {
        return;
    }

    // [四柱-监测] JSON格式输出，与串口格式一致，包含 pitch/roll/fall_alert
    snprintf(buf, sizeof(buf),
        "{\"type\":\"data\","
        "\"hr\":%lu,\"spo2\":%lu,\"temp\":%.1f,"
        "\"accel\":[%.2f,%.2f,%.2f],"
        "\"gyro\":[%.2f,%.2f,%.2f],"
        "\"pitch\":%.1f,\"roll\":%.1f,"
        "\"fall_conf\":%d,\"fall_alert\":%s,"
        "\"status\":\"%s\","
        "\"hr_source\":\"%s\","
        "\"valid\":{\"hr\":%s,\"spo2\":%s,\"temp\":%s,\"imu\":%s}}\r\n",
        g_heart_rate, g_spo2, g_temperature,
        g_accel[0], g_accel[1], g_accel[2],
        g_gyro[0], g_gyro[1], g_gyro[2],
        attitude_get_pitch(), attitude_get_roll(),
        fall_detection_get_confidence(),
        (fall_detection_get_state() == FALL_STATE_FALLEN) ? "true" : "false",
        health_alert_get_text(),
        g_hr_source_max30102 ? "max30102" : "ky039",
        g_hr_valid ? "true" : "false",
        g_spo2_valid ? "true" : "false",
        g_temp_valid ? "true" : "false",
        g_imu_valid ? "true" : "false"
    );

    sle_comm_send_json(buf);
}

// 处理接收到的命令
void health_monitor_process_command(const char *cmd){
    if(cmd == NULL) return;

    osal_printk("Received command: %s\r\n", cmd);

    // 解析命令
    if(strstr(cmd, "breath_start") != NULL) {
        health_monitor_start_breath_guide();
    } else if(strstr(cmd, "breath_stop") != NULL) {
        health_monitor_stop_breath_guide();
    } else if(strstr(cmd, "mute") != NULL) {
        health_alert_mute();
    } else if(strstr(cmd, "confirm") != NULL) {
        health_alert_confirm();
        if(fall_detection_get_state() == FALL_STATE_FALLEN) {
            fall_detection_confirm();
        }
    }
}

// SLE数据接收回调
static void sle_data_callback(uint8_t *data, uint16_t len){
    if(data == NULL || len == 0) return;

    // 转换为字符串处理
    char cmd_buf[64];
    uint16_t copy_len = (len < sizeof(cmd_buf) - 1) ? len : sizeof(cmd_buf) - 1;
    memcpy(cmd_buf, data, copy_len);
    cmd_buf[copy_len] = '\0';

    health_monitor_process_command(cmd_buf);
}

// 系统主循环
void health_monitor_loop(void){
    uint32_t current_time = get_time_ms();

    // [四柱-架构] 状态机驱动：根据当前状态决定行为
    switch(g_system_state) {
        case SYS_STATE_NORMAL:
        case SYS_STATE_SLE_CONNECTED:
            // 正常状态：定期更新传感器数据
            if(current_time - g_last_update_time >= SYSTEM_UPDATE_INTERVAL_MS) {
                g_last_update_time = current_time;
                update_sensor_data();

                // [四柱-架构] 自动检测异常状态 → 进入ALERT
                if(health_alert_get_status() != HEALTH_NORMAL ||
                   fall_detection_get_state() == FALL_STATE_FALLEN) {
                    state_transition(SYS_STATE_ALERT, "abnormal detected");
                }
            }
            break;

        case SYS_STATE_ALERT:
            // 报警状态：持续更新传感器（检查是否恢复）
            if(current_time - g_last_update_time >= SYSTEM_UPDATE_INTERVAL_MS) {
                g_last_update_time = current_time;
                update_sensor_data();

                // 恢复正常 → 自动退出ALERT
                if(health_alert_get_status() == HEALTH_NORMAL &&
                   fall_detection_get_state() == FALL_STATE_NORMAL) {
                    state_transition(SYS_STATE_NORMAL, "recovered");
                }
            }
            break;

        case SYS_STATE_BREATH_GUIDE:
            // 呼吸引导状态：更新引导动画
            breath_guide_update();
            // 检测自动停止（达到最大轮数）
            if(breath_guide_is_finished()) {
                health_monitor_stop_breath_guide();
            }
            break;

        default:
            break;
    }

    // 定期更新OLED显示（仅真实模式）
#if !MOCK_HARDWARE_MODE
    if(current_time - g_last_oled_time >= OLED_REFRESH_INTERVAL_MS) {
        g_last_oled_time = current_time;
        update_oled_display();
    }
#endif

    // 定期发送数据（两种模式都发串口，仅真实模式发SLE）
    if(current_time - g_last_data_send_time >= DATA_SEND_INTERVAL_MS) {
        g_last_data_send_time = current_time;
        send_data_to_serial();
#if !MOCK_HARDWARE_MODE
        health_monitor_send_data();
#endif
    }

    // 更新提醒动画和震动（仅真实模式）
#if !MOCK_HARDWARE_MODE
    health_alert_update_animation();
    vibration_motor_update();
#endif
}

// 获取系统状态
system_state_t health_monitor_get_state(void){
    return g_system_state;
}

// 启动呼吸引导
void health_monitor_start_breath_guide(void){
    // [四柱-提醒] 记录引导前心率基准
    breath_guide_set_baseline_hr(g_heart_rate);
    state_transition(SYS_STATE_BREATH_GUIDE, "breath guide start");
    breath_guide_start();
}

// 停止呼吸引导
void health_monitor_stop_breath_guide(void){
    state_transition(SYS_STATE_NORMAL, "breath guide stop");
    breath_guide_stop(g_heart_rate);
    // [四柱-提醒] 显示呼吸效果反馈
    int32_t delta = breath_guide_get_hr_delta();
    if(delta != 0) {
        osal_printk("[BREATH] HR effect: %lu -> %lu (%+ld BPM)\r\n",
               g_heart_rate - delta, g_heart_rate, delta);
    }
}

// [四柱-架构] 处理按键事件（状态驱动）
void health_monitor_button_handler(uint8_t button_id){
    switch(g_system_state) {
        case SYS_STATE_ALERT:
            if(button_id == 1) {
                // 确认报警
                health_alert_confirm();
                if(fall_detection_get_state() == FALL_STATE_FALLEN) {
                    fall_detection_confirm();
                }
                state_transition(SYS_STATE_NORMAL, "button confirm");
            } else if(button_id == 3) {
                health_alert_mute();
            }
            break;

        case SYS_STATE_BREATH_GUIDE:
            if(button_id == 1 || button_id == 2) {
                health_monitor_stop_breath_guide();
            } else if(button_id == 3) {
                health_alert_mute();
            }
            break;

        case SYS_STATE_NORMAL:
        case SYS_STATE_SLE_CONNECTED:
            if(button_id == 2) {
                health_monitor_start_breath_guide();
            } else if(button_id == 3) {
                health_alert_mute();
            }
            break;

        default:
            break;
    }
}
