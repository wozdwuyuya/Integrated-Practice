// 协作提示：此文件已由 lib/ 迁移，硬件引脚严格保持小熊派原始定义。
/**
 * @file health_monitor_main.h
 * @brief 智能健康监测系统主程序头文件
 * @note 整合所有传感器、SLE通信和功能模块
 */

#ifndef HEALTH_MONITOR_MAIN_H
#define HEALTH_MONITOR_MAIN_H

#include <stdint.h>
#include <stdbool.h>

// 系统状态定义
typedef enum {
    SYS_STATE_INIT = 0,        // 初始化状态
    SYS_STATE_NORMAL,          // 正常工作状态
    SYS_STATE_ALERT,           // 报警状态
    SYS_STATE_BREATH_GUIDE,    // 呼吸引导状态
    SYS_STATE_SLE_CONNECTED    // SLE已连接状态
} system_state_t;

// 系统配置
#define SYSTEM_UPDATE_INTERVAL_MS   100     // 系统更新间隔 (ms)
#define OLED_REFRESH_INTERVAL_MS    500     // OLED刷新间隔 (ms)
#define DATA_SEND_INTERVAL_MS       1000    // 数据发送间隔 (ms)

// [四柱-监测] 数据有效性：心率/血氧超过此时间无更新视为无效
#define DATA_STALE_TIMEOUT_MS       5000

// 硬件模拟模式：1=使用假数据（无硬件调试），0=使用真实传感器
// 优先从 Kconfig 读取（menuconfig → QRSWORK_MOCK_HARDWARE），
// 未配置时回退到默认值 1（开发阶段默认开启模拟）
#ifdef CONFIG_QRSWORK_MOCK_HARDWARE
#define MOCK_HARDWARE_MODE  1
#else
#ifndef MOCK_HARDWARE_MODE
#define MOCK_HARDWARE_MODE  1
#endif
#endif

/**
 * @brief 系统初始化
 * @return true成功，false失败
 */
bool health_monitor_init(void);

/**
 * @brief 系统主循环（需要在main函数中调用）
 */
void health_monitor_loop(void);

/**
 * @brief 获取系统状态
 * @return 系统状态枚举值
 */
system_state_t health_monitor_get_state(void);

/**
 * @brief 启动呼吸引导
 */
void health_monitor_start_breath_guide(void);

/**
 * @brief 停止呼吸引导
 */
void health_monitor_stop_breath_guide(void);

/**
 * @brief 处理按键事件
 * @param button_id 按键ID
 */
void health_monitor_button_handler(uint8_t button_id);

/**
 * @brief 发送传感器数据到手机端
 */
void health_monitor_send_data(void);

/**
 * @brief 处理接收到的命令
 * @param cmd 命令字符串
 */
void health_monitor_process_command(const char *cmd);

#endif // HEALTH_MONITOR_MAIN_H
