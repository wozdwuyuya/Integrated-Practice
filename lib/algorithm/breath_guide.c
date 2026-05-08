/**
 * @file breath_guide.c
 * @brief 呼吸节奏引导功能实现
 * @note 实现4-7-8呼吸法引导，帮助用户放松
 */

#include "breath_guide.h"
#include "system/system_utils.h"
#include "output/ssd1306.h"
#include "osal_debug.h"
#include <stdio.h>

// 呼吸引导状态
static breath_phase_t g_breath_phase = BREATH_IDLE;
static uint32_t g_breath_start_time = 0;
static uint32_t g_phase_start_time = 0;
static bool g_breath_active = false;

// [四柱-提醒] 呼吸引导增强：轮次计数、HR基准
static uint8_t g_cycle_count = 0;        // 已完成轮数
static uint32_t g_baseline_hr = 0;       // 引导前心率基准
static uint32_t g_end_hr = 0;            // 引导结束后心率
static bool g_finished = false;          // 是否已完成最大轮数


// 呼吸引导初始化
void breath_guide_init(void){
    g_breath_phase = BREATH_IDLE;
    g_breath_active = false;
    g_cycle_count = 0;
    g_finished = false;
}

// 启动呼吸引导
void breath_guide_start(void){
    g_breath_active = true;
    g_breath_start_time = get_time_ms();
    g_phase_start_time = g_breath_start_time;
    g_breath_phase = BREATH_INHALE;
    g_cycle_count = 0;
    g_finished = false;
}

// 停止呼吸引导（传入当前心率用于效果反馈）
void breath_guide_stop(uint32_t current_hr){
    g_end_hr = current_hr;
    g_breath_active = false;
    g_breath_phase = BREATH_IDLE;
}

// 更新呼吸引导状态
void breath_guide_update(void){
    if(!g_breath_active) return;

    uint32_t current_time = get_time_ms();
    uint32_t elapsed = current_time - g_phase_start_time;

    switch(g_breath_phase) {
        case BREATH_INHALE:
            if(elapsed >= BREATH_INHALE_TIME) {
                g_breath_phase = BREATH_HOLD;
                g_phase_start_time = current_time;
            }
            break;

        case BREATH_HOLD:
            if(elapsed >= BREATH_HOLD_TIME) {
                g_breath_phase = BREATH_EXHALE;
                g_phase_start_time = current_time;
            }
            break;

        case BREATH_EXHALE:
            if(elapsed >= BREATH_EXHALE_TIME) {
                // [四柱-提醒] 一轮完成，计数+1
                g_cycle_count++;
                osal_printk("[BREATH] Cycle %d/%d completed\r\n", g_cycle_count, BREATH_MAX_CYCLES);

                // 达到最大轮数 → 标记完成，由调用方检测后调用 stop
                if(g_cycle_count >= BREATH_MAX_CYCLES) {
                    g_finished = true;
                    g_breath_active = false;
                    g_breath_phase = BREATH_IDLE;
                    osal_printk("[BREATH] Max cycles reached, auto-stopped\r\n");
                    return;
                }

                g_breath_phase = BREATH_INHALE;
                g_phase_start_time = current_time;
            }
            break;

        default:
            break;
    }
}

// 获取当前呼吸阶段
breath_phase_t breath_guide_get_phase(void){
    return g_breath_phase;
}

// 获取当前阶段进度百分比
uint8_t breath_guide_get_progress(void){
    if(!g_breath_active) return 0;

    uint32_t current_time = get_time_ms();
    uint32_t elapsed = current_time - g_phase_start_time;
    uint32_t phase_time;

    switch(g_breath_phase) {
        case BREATH_INHALE:
            phase_time = BREATH_INHALE_TIME;
            break;
        case BREATH_HOLD:
            phase_time = BREATH_HOLD_TIME;
            break;
        case BREATH_EXHALE:
            phase_time = BREATH_EXHALE_TIME;
            break;
        default:
            return 0;
    }

    if(elapsed >= phase_time) return 100;
    return (uint8_t)((elapsed * 100) / phase_time);
}

// 在OLED上显示呼吸引导
void breath_guide_display(uint8_t y){
    if(!g_breath_active) return;

    const char *text = breath_guide_get_text();
    uint8_t progress = breath_guide_get_progress();

    // [四柱-提醒] 第1行：阶段 + 轮次
    ssd1306_SetCursor(0, y);
    char line1[22];
    snprintf(line1, sizeof(line1), "%s %d/%d", text, g_cycle_count + 1, BREATH_MAX_CYCLES);
    ssd1306_DrawString(line1, Font_7x10, White);

    // 显示进度条
    ssd1306_SetCursor(0, y + 12);
    ssd1306_DrawString("[", Font_7x10, White);

    // 绘制进度条
    uint8_t bar_width = progress * 14 / 100;
    for(uint8_t i = 0; i < 14; i++) {
        if(i < bar_width) {
            ssd1306_DrawString("=", Font_7x10, White);
        } else {
            ssd1306_DrawString("-", Font_7x10, White);
        }
    }
    ssd1306_DrawString("]", Font_7x10, White);

    // 显示秒数倒计时
    uint32_t current_time = get_time_ms();
    uint32_t elapsed = current_time - g_phase_start_time;
    uint32_t phase_time;

    switch(g_breath_phase) {
        case BREATH_INHALE:
            phase_time = BREATH_INHALE_TIME;
            break;
        case BREATH_HOLD:
            phase_time = BREATH_HOLD_TIME;
            break;
        case BREATH_EXHALE:
            phase_time = BREATH_EXHALE_TIME;
            break;
        default:
            phase_time = 0;
            break;
    }

    if(phase_time > 0 && elapsed < phase_time) {
        uint32_t remaining = (phase_time - elapsed) / 1000;
        char time_buf[8];
        snprintf(time_buf, sizeof(time_buf), " %lus", remaining);
        ssd1306_DrawString(time_buf, Font_7x10, White);
    }
}

// 获取呼吸引导提示文字
const char* breath_guide_get_text(void){
    switch(g_breath_phase) {
        case BREATH_INHALE:
            return "Inhale...";    // 吸气
        case BREATH_HOLD:
            return "Hold...";      // 屏息
        case BREATH_EXHALE:
            return "Exhale...";    // 呼气
        default:
            return "Ready...";
    }
}

// [四柱-提醒] 获取已完成的呼吸轮数
uint8_t breath_guide_get_cycle_count(void){
    return g_cycle_count;
}

// [四柱-提醒] 记录引导前的HR基准
void breath_guide_set_baseline_hr(uint32_t hr){
    g_baseline_hr = hr;
}

// [四柱-提醒] 获取HR变化值
int32_t breath_guide_get_hr_delta(void){
    if(g_end_hr == 0 || g_baseline_hr == 0) return 0;
    return (int32_t)g_end_hr - (int32_t)g_baseline_hr;
}

// [四柱-提醒] 呼吸是否已完成
bool breath_guide_is_finished(void){
    return g_finished;
}
