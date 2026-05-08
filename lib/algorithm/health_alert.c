/**
 * @file health_alert.c
 * @brief 心率异常智能提醒系统实现
 * @note [四柱-提醒] 分级报警系统 + 震动模式细化
 * @note 整合RGB灯、蜂鸣器、震动马达、OLED进行多模式提醒
 * @note 改进：1.三级报警(INFO/WARNING/DANGER) 2.滞后防抖 3.静音自动恢复
 * @note WARNING用脉冲震动，INFO不用震动
 */

#include "health_alert.h"
#include "system/system_utils.h"
#include "output/rgb_led.h"
#include "output/beep.h"
#include "output/vibration_motor.h"
#include "output/ssd1306.h"
#include "osal_debug.h"

// 默认阈值配置
static health_threshold_t g_threshold = {
    .hr_high_threshold = 100,       // 心率上限 100 BPM
    .hr_low_threshold = 60,         // 心率下限 60 BPM
    .spo2_low_threshold = 90,       // 血氧下限 90%
    .temp_high_threshold = 37.5f    // 温度上限 37.5℃
};

// 健康状态
static health_status_t g_health_status = HEALTH_NORMAL;
static alert_level_t g_alert_level = ALERT_LEVEL_NONE;
static alert_mode_t g_alert_mode = ALERT_MODE_NONE;
static bool g_alert_active = false;
static bool g_alert_muted = false;
static uint32_t g_alert_start_time = 0;   // 报警开始时间（用于静音超时）
static uint32_t g_alert_confirm_time = 0; // 上次确认时间（用于冷却期）

// 滞后状态：记录是否已触发（防止边界抖动）
static bool g_hr_high_triggered = false;
static bool g_hr_low_triggered = false;
static bool g_spo2_triggered = false;
static bool g_fever_triggered = false;

// 动画状态
static uint32_t g_animation_last_time = 0;
static uint8_t g_animation_step = 0;


// [四柱-提醒] 根据健康状态确定报警等级
static alert_level_t determine_alert_level(health_status_t status){
    switch(status) {
        case HEALTH_FALL_DETECTED:
        case HEALTH_LOW_SPO2:
            return ALERT_LEVEL_DANGER;   // 跌倒/血氧低 = 危险
        case HEALTH_HIGH_HR:
        case HEALTH_LOW_HR:
            return ALERT_LEVEL_WARNING;  // 心率异常 = 警告
        case HEALTH_FEVER:
            return ALERT_LEVEL_INFO;     // 发烧 = 注意
        default:
            return ALERT_LEVEL_NONE;
    }
}

// [四柱-提醒] 根据报警等级确定提醒模式
static alert_mode_t level_to_mode(alert_level_t level){
    switch(level) {
        case ALERT_LEVEL_INFO:    return ALERT_MODE_VISUAL;
        case ALERT_LEVEL_WARNING: return ALERT_MODE_VIBRATION;
        case ALERT_LEVEL_DANGER:  return ALERT_MODE_ALL;
        default:                  return ALERT_MODE_NONE;
    }
}

// 健康提醒系统初始化
bool health_alert_init(void){
    g_health_status = HEALTH_NORMAL;
    g_alert_level = ALERT_LEVEL_NONE;
    g_alert_mode = ALERT_MODE_NONE;
    g_alert_active = false;
    g_alert_muted = false;
    g_alert_start_time = 0;
    g_hr_high_triggered = false;
    g_hr_low_triggered = false;
    g_spo2_triggered = false;
    g_fever_triggered = false;

    rgb_led_status_normal();
    osal_printk("Health alert init success\r\n");
    return true;
}

// 设置健康阈值
void health_alert_set_threshold(health_threshold_t threshold){
    g_threshold = threshold;
}

// [四柱-提醒] 更新健康数据（带滞后防抖）
void health_alert_update(uint32_t heart_rate, uint32_t spo2, float temperature){
    health_status_t new_status = HEALTH_NORMAL;
    alert_level_t new_level = ALERT_LEVEL_NONE;
    uint32_t current_time = get_time_ms();

    // 心率检测（带滞后）
    if(!g_hr_high_triggered) {
        // 未触发状态：心率超过上限才触发
        if(heart_rate > g_threshold.hr_high_threshold) {
            g_hr_high_triggered = true;
            new_status = HEALTH_HIGH_HR;
        }
    } else {
        // 已触发状态：心率降到(上限-滞后)才解除
        if(heart_rate < g_threshold.hr_high_threshold - ALERT_HYSTERESIS_HR) {
            g_hr_high_triggered = false;
        } else {
            new_status = HEALTH_HIGH_HR;
        }
    }

    if(!g_hr_low_triggered) {
        if(heart_rate > 0 && heart_rate < g_threshold.hr_low_threshold) {
            g_hr_low_triggered = true;
            new_status = HEALTH_LOW_HR;
        }
    } else {
        if(heart_rate > g_threshold.hr_low_threshold + ALERT_HYSTERESIS_HR) {
            g_hr_low_triggered = false;
        } else {
            new_status = HEALTH_LOW_HR;
        }
    }

    // 血氧检测（带滞后）
    if(!g_spo2_triggered) {
        if(spo2 > 0 && spo2 < g_threshold.spo2_low_threshold) {
            g_spo2_triggered = true;
            new_status = HEALTH_LOW_SPO2;
        }
    } else {
        if(spo2 > g_threshold.spo2_low_threshold + ALERT_HYSTERESIS_SPO2) {
            g_spo2_triggered = false;
        } else {
            new_status = HEALTH_LOW_SPO2;
        }
    }

    // 发烧检测
    if(!g_fever_triggered) {
        if(temperature > g_threshold.temp_high_threshold) {
            g_fever_triggered = true;
            new_status = HEALTH_FEVER;
        }
    } else {
        if(temperature < g_threshold.temp_high_threshold - 0.5f) {
            g_fever_triggered = false;
        } else {
            new_status = HEALTH_FEVER;
        }
    }

    // 确定报警等级和提醒模式
    new_level = determine_alert_level(new_level != ALERT_LEVEL_NONE ? new_level :
                (new_status != HEALTH_NORMAL ? new_status : HEALTH_NORMAL));
    // 修正：用实际new_status确定等级
    if(new_status != HEALTH_NORMAL) {
        new_level = determine_alert_level(new_status);
    }

    // [四柱-提醒] 静音自动恢复
    if(g_alert_muted && g_alert_start_time > 0) {
        if(current_time - g_alert_start_time >= ALERT_MUTE_TIMEOUT_MS) {
            g_alert_muted = false;
            osal_printk("[ALERT] Mute auto-recovered\r\n");
        }
    }

    // 更新状态（只在状态变化时更新）
    if(new_status != HEALTH_NORMAL && new_status != g_health_status) {
        g_health_status = new_status;
        g_alert_level = new_level;
        g_alert_mode = level_to_mode(new_level);
        g_alert_active = true;
        g_alert_start_time = current_time;
        g_animation_step = 0;
        g_alert_muted = false;  // 新异常自动解除静音
        osal_printk("[ALERT] Status -> %d, Level -> %d\r\n", new_status, new_level);
    } else if(new_status == HEALTH_NORMAL) {
        if(g_health_status != HEALTH_NORMAL) {
            osal_printk("[ALERT] Recovered to NORMAL\r\n");
            health_alert_confirm();
        }
    }
}

// 更新跌倒检测状态
void health_alert_update_fall(bool fall_detected){
    if(fall_detected) {
        g_health_status = HEALTH_FALL_DETECTED;
        g_alert_level = ALERT_LEVEL_DANGER;
        g_alert_mode = ALERT_MODE_ALL;
        g_alert_active = true;
        g_alert_start_time = get_time_ms();
        g_animation_step = 0;
        g_alert_muted = false;
        osal_printk("[ALERT] Fall detected! Level=DANGER\r\n");
    }
}

// 获取当前健康状态
health_status_t health_alert_get_status(void){
    return g_health_status;
}

// 获取当前提醒模式
alert_mode_t health_alert_get_mode(void){
    return g_alert_mode;
}

// [四柱-提醒] 获取当前报警等级
alert_level_t health_alert_get_level(void){
    return g_alert_level;
}

// 确认提醒
void health_alert_confirm(void){
    g_health_status = HEALTH_NORMAL;
    g_alert_level = ALERT_LEVEL_NONE;
    g_alert_mode = ALERT_MODE_NONE;
    g_alert_active = false;
    g_animation_step = 0;
    g_alert_confirm_time = get_time_ms();
    rgb_led_status_normal();
    vibration_motor_off();
}

// 静音提醒
void health_alert_mute(void){
    g_alert_muted = true;
    g_alert_start_time = get_time_ms();
    vibration_motor_off();
}

// [四柱-提醒] 更新提醒动画（分级不同闪烁频率）
void health_alert_update_animation(void){
    if(!g_alert_active || g_alert_muted) return;

    uint32_t current_time = get_time_ms();
    uint32_t interval;

    // 根据报警等级调整闪烁频率
    switch(g_alert_level) {
        case ALERT_LEVEL_INFO:    interval = 1000; break;  // 1Hz 慢闪
        case ALERT_LEVEL_WARNING: interval = 500;  break;  // 2Hz 中闪
        case ALERT_LEVEL_DANGER:  interval = 250;  break;  // 4Hz 快闪
        default:                  interval = 500;  break;
    }

    if(current_time - g_animation_last_time < interval) return;
    g_animation_last_time = current_time;

    switch(g_alert_mode) {
        case ALERT_MODE_VISUAL:
            if(g_animation_step % 2 == 0) {
                rgb_led_set_color(RGB_COLOR_YELLOW);
            } else {
                rgb_led_set_color(RGB_COLOR_OFF);
            }
            break;

        case ALERT_MODE_BUZZER:
            if(g_animation_step % 2 == 0) {
                beep_time(100);
            }
            break;

        case ALERT_MODE_VIBRATION:
            // [四柱-提醒] 脉冲震动：每2秒一次短震(200ms)
            // WARNING级别500ms间隔，4步=2秒周期，仅第0步震动
            if(g_animation_step % 4 == 0) {
                vibration_motor_pulse(200);
            }
            break;

        case ALERT_MODE_ALL:
            if(g_animation_step % 2 == 0) {
                rgb_led_set_color(RGB_COLOR_RED);
                vibration_motor_on();
                beep_time(80);
            } else {
                rgb_led_set_color(RGB_COLOR_OFF);
                vibration_motor_off();
            }
            break;

        default:
            break;
    }

    g_animation_step++;
}

// 在OLED上显示健康状态
void health_alert_display(uint8_t y){
    const char *text = health_alert_get_text();

    ssd1306_SetCursor(0, y);
    ssd1306_DrawString("                ", Font_7x10, Black);

    ssd1306_SetCursor(0, y);
    ssd1306_DrawString((char*)text, Font_7x10, White);
}

// 获取健康状态提示文字（带等级标识）
const char* health_alert_get_text(void){
    switch(g_health_status) {
        case HEALTH_NORMAL:
            return "Status: Normal";
        case HEALTH_HIGH_HR:
            return "[!] High HR";
        case HEALTH_LOW_HR:
            return "[!] Low HR";
        case HEALTH_LOW_SPO2:
            return "[!!] Low SpO2!";
        case HEALTH_FALL_DETECTED:
            return "[!!!] FALL!";
        case HEALTH_FEVER:
            return "[i] Fever";
        default:
            return "Status: Unknown";
    }
}
