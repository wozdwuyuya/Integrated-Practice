/**
 * @file tcp_server.h
 * @brief TCP Server 与 WiFi AP 模式初始化
 * @note  提供 SoftAP 热点启动和 TCP 数据推送通道
 */

#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <stdbool.h>

/**
 * @brief 初始化 WiFi AP 模式（SoftAP 热点）
 * @return true 成功，false 失败
 * @note  阻塞等待 WiFi 子系统就绪后配置 SoftAP，失败不阻塞系统启动
 */
bool tcp_server_wifi_init(void);

/**
 * @brief 启动 TCP Server 内核任务
 * @return true 成功，false 失败
 * @note  阶段 3 实现：socket 监听、数据泵、命令接收
 */
bool tcp_server_start(void);

#endif // TCP_SERVER_H
