/**
 * @file sle_comm.h
 * @brief 星闪SLE无线通信模块头文件
 * @note [四柱-问答] SLE 通信模块
 * @note 基于星闪NearLink SLE协议，实现采集端与接收端数据传输
 * @note 无SDK时通过 FEATURE_SLE 宏退化为空实现
 */

#ifndef SLE_COMM_H
#define SLE_COMM_H

#include <stdint.h>
#include <stdbool.h>

// SLE通信状态
typedef enum {
    SLE_STATE_IDLE = 0,         // 空闲状态
    SLE_STATE_ADV,              // 广播中
    SLE_STATE_CONNECTED,        // 已连接
    SLE_STATE_PAIRED,           // 已配对
    SLE_STATE_ERROR             // 错误状态
} sle_state_t;

// 数据接收回调函数类型
typedef void (*sle_data_callback_t)(uint8_t *data, uint16_t len);

/**
 * @brief SLE通信初始化
 * @return true成功，false失败
 */
bool sle_comm_init(void);

/**
 * @brief 启动SLE广播
 * @return true成功，false失败
 */
bool sle_comm_start_adv(void);

/**
 * @brief 停止SLE广播
 */
void sle_comm_stop_adv(void);

/**
 * @brief 发送数据到对端
 * @param data 数据缓冲区
 * @param len 数据长度
 * @return true成功，false失败
 */
bool sle_comm_send_data(const uint8_t *data, uint16_t len);

/**
 * @brief 发送JSON格式传感器数据
 * @param json_str JSON字符串
 * @return true成功，false失败
 */
bool sle_comm_send_json(const char *json_str);

/**
 * @brief 注册数据接收回调
 * @param callback 接收回调函数
 */
void sle_comm_register_callback(sle_data_callback_t callback);

/**
 * @brief 获取SLE连接状态
 * @return 连接状态
 */
sle_state_t sle_comm_get_state(void);

/**
 * @brief 检查是否已连接
 * @return true已连接，false未连接
 */
bool sle_comm_is_connected(void);

/**
 * @brief 获取连接句柄
 * @return 连接句柄
 */
uint16_t sle_comm_get_conn_id(void);

#endif // SLE_COMM_H
