/**
 * @file breath_guide.h
 * @brief 呼吸节奏引导功能头文件
 * @note 用于OLED显示呼吸引导，帮助用户进行呼吸调节
 */

#ifndef BREATH_GUIDE_H
#define BREATH_GUIDE_H

#include <stdint.h>
#include <stdbool.h>

// 呼吸阶段定义
typedef enum {
    BREATH_IDLE = 0,        // 空闲状态
    BREATH_INHALE,          // 吸气阶段 (4秒)
    BREATH_HOLD,            // 屏息阶段 (7秒)
    BREATH_EXHALE           // 呼气阶段 (8秒)
} breath_phase_t;

// 呼吸引导配置
#define BREATH_INHALE_TIME   4000   // 吸气时间 (ms)
#define BREATH_HOLD_TIME     7000   // 屏息时间 (ms)
#define BREATH_EXHALE_TIME   8000   // 呼气时间 (ms)
#define BREATH_CYCLE_TIME    (BREATH_INHALE_TIME + BREATH_HOLD_TIME + BREATH_EXHALE_TIME)  // 完整周期 (19秒)
#define BREATH_MAX_CYCLES    3      // 最大循环次数（约57秒后自动停止）

/**
 * @brief 呼吸引导初始化
 */
void breath_guide_init(void);

/**
 * @brief 启动呼吸引导
 */
void breath_guide_start(void);

/**
 * @brief 停止呼吸引导
 * @param current_hr 停止时的当前心率（用于HR变化反馈）
 */
void breath_guide_stop(uint32_t current_hr);

/**
 * @brief 更新呼吸引导状态（需要在定时器中调用）
 */
void breath_guide_update(void);

/**
 * @brief 获取当前呼吸阶段
 * @return 当前阶段枚举值
 */
breath_phase_t breath_guide_get_phase(void);

/**
 * @brief 获取当前阶段进度百分比
 * @return 0-100的进度值
 */
uint8_t breath_guide_get_progress(void);

/**
 * @brief 在OLED上显示呼吸引导
 * @param y 起始Y坐标
 */
void breath_guide_display(uint8_t y);

/**
 * @brief 获取呼吸引导提示文字
 * @return 当前阶段的提示文字
 */
const char* breath_guide_get_text(void);

// [四柱-提醒] 呼吸引导增强功能

/**
 * @brief 获取已完成的呼吸轮数
 * @return 已完成轮数
 */
uint8_t breath_guide_get_cycle_count(void);

/**
 * @brief 记录引导开始前的心率基准
 * @param hr 引导开始前的心率值
 */
void breath_guide_set_baseline_hr(uint32_t hr);

/**
 * @brief 获取引导前后的HR变化
 * @return HR变化值（正数=升高，负数=降低）
 */
int32_t breath_guide_get_hr_delta(void);

/**
 * @brief 获取呼吸是否已完成（达到最大轮数）
 * @return true已完成，false未完成
 */
bool breath_guide_is_finished(void);

#endif // BREATH_GUIDE_H
