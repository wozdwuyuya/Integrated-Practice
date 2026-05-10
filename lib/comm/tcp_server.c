/**
 * @file tcp_server.c
 * @brief TCP Server 与 WiFi AP 模式初始化
 * @note  阶段 2：实现 WiFi SoftAP 热点初始化
 * @note  阶段 3：TCP socket 监听、数据泵、命令接收
 */

#include "comm/tcp_server.h"
#include "app/health_monitor_main.h"
#include "lwip/netifapi.h"
#include "lwip/sockets.h"
#include "wifi_hotspot.h"
#include "wifi_hotspot_config.h"
#include "wifi_device.h"
#include "td_base.h"
#include "td_type.h"
#include "cmsis_os2.h"
#include "soc_osal.h"
#include "osal_debug.h"
#include "cJSON.h"
#include <string.h>

// ========== WiFi AP 配置 ==========
#define WIFI_AP_SSID            "HealthMonitor"
#define WIFI_AP_PASSWORD        "12345678"
#define WIFI_AP_CHANNEL         6
#define WIFI_AP_IFNAME          "ap0"

// 网络配置
#define WIFI_AP_IP_ADDR         "192.168.4.1"
#define WIFI_AP_NETMASK_ADDR    "255.255.255.0"
#define WIFI_AP_GW_ADDR         "192.168.4.1"

// WiFi 初始化超时
#define WIFI_INIT_TIMEOUT_MS    10000
#define WIFI_INIT_POLL_MS       100

// TCP Server 配置
#define TCP_SERVER_PORT         5000
#define TCP_SERVER_BACKLOG      1
#define TCP_SERVER_TASK_STACK   0x2000
#define TCP_SERVER_TASK_PRIO    (osPriority_t)(12)
#define TCP_SEND_INTERVAL_MS    1000
#define TCP_RECV_BUF_SIZE       256
#define TCP_SELECT_TIMEOUT_SEC  1

bool tcp_server_wifi_init(void)
{
    // 1. 等待 WiFi 子系统初始化完成
    uint32_t wait_ms = 0;
    while (wifi_is_wifi_inited() == 0) {
        osDelay(WIFI_INIT_POLL_MS / 10);
        wait_ms += WIFI_INIT_POLL_MS;
        if (wait_ms >= WIFI_INIT_TIMEOUT_MS) {
            osal_printk("[TCP] WiFi init timeout!\r\n");
            return false;
        }
    }
    osal_printk("[TCP] WiFi subsystem ready\r\n");

    // 2. 配置 SoftAP 扩展参数
    softap_config_advance_stru adv_config = {0};
    adv_config.beacon_interval = 100;
    adv_config.dtim_period = 2;
    adv_config.gi = 0;
    adv_config.group_rekey = 86400;
    adv_config.protocol_mode = 4;
    adv_config.hidden_ssid_flag = 1;
    if (wifi_set_softap_config_advance(&adv_config) != 0) {
        osal_printk("[TCP] SoftAP advance config failed\r\n");
        return false;
    }

    // 3. 配置并启动 SoftAP
    softap_config_stru ap_config = {0};
    memcpy_s(ap_config.ssid, sizeof(ap_config.ssid),
             WIFI_AP_SSID, sizeof(WIFI_AP_SSID));
    memcpy_s(ap_config.pre_shared_key, WIFI_MAX_KEY_LEN,
             WIFI_AP_PASSWORD, sizeof(WIFI_AP_PASSWORD));
    ap_config.security_type = WIFI_SEC_TYPE_WPA2PSK;
    ap_config.channel_num = WIFI_AP_CHANNEL;
    ap_config.wifi_psk_type = 0;
    if (wifi_softap_enable(&ap_config) != 0) {
        osal_printk("[TCP] SoftAP enable failed\r\n");
        return false;
    }

    // 4. 配置网络接口 IP 地址
    struct netif *netif_p = netif_find(WIFI_AP_IFNAME);
    if (netif_p == NULL) {
        osal_printk("[TCP] netif '%s' not found\r\n", WIFI_AP_IFNAME);
        wifi_softap_disable();
        return false;
    }
    ip4_addr_t ip, netmask, gw;
    ip4_addr_set_u32(&ip,      ipaddr_addr(WIFI_AP_IP_ADDR));
    ip4_addr_set_u32(&netmask, ipaddr_addr(WIFI_AP_NETMASK_ADDR));
    ip4_addr_set_u32(&gw,      ipaddr_addr(WIFI_AP_GW_ADDR));
    if (netifapi_netif_set_addr(netif_p, &ip, &netmask, &gw) != 0) {
        osal_printk("[TCP] netif set addr failed\r\n");
        wifi_softap_disable();
        return false;
    }

    // 5. 启动 DHCP 服务器（给连接的 App 自动分配 IP）
    if (netifapi_dhcps_start(netif_p, NULL, 0) != 0) {
        osal_printk("[TCP] DHCP server start failed\r\n");
        wifi_softap_disable();
        return false;
    }

    osal_printk("[TCP] SoftAP started: SSID=%s, IP=%s\r\n",
                WIFI_AP_SSID, WIFI_AP_IP_ADDR);
    return true;
}

// TCP Server 内核任务：socket 监听、数据泵、命令接收
static void *tcp_server_task(const char *arg)
{
    unused(arg);
    int listen_fd = -1;
    int client_fd = -1;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char recv_buf[TCP_RECV_BUF_SIZE];

    osal_printk("[TCP] Server task started\r\n");

    // 1. 创建 TCP socket
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        osal_printk("[TCP] Socket create failed: %d\r\n", listen_fd);
        return NULL;
    }

    // 2. 设置 SO_REUSEADDR，允许快速重启
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 3. 绑定端口
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TCP_SERVER_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        osal_printk("[TCP] Bind port %d failed\r\n", TCP_SERVER_PORT);
        lwip_close(listen_fd);
        return NULL;
    }

    // 4. 开始监听
    if (listen(listen_fd, TCP_SERVER_BACKLOG) != 0) {
        osal_printk("[TCP] Listen failed\r\n");
        lwip_close(listen_fd);
        return NULL;
    }

    osal_printk("[TCP] Server listening on port %d\r\n", TCP_SERVER_PORT);

    // 5. 外层循环：accept 等待客户端连接
    while (1) {
        osal_printk("[TCP] Waiting for client connection...\r\n");
        client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd < 0) {
            osal_printk("[TCP] Accept failed: %d\r\n", client_fd);
            osDelay(100);
            continue;
        }
        osal_printk("[TCP] Client connected: %s:%d\r\n",
                    inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // 6. 内层循环：数据泵 + 命令接收
        uint32_t last_send_time = 0;
        while (1) {
            // 使用 select 实现"定时发送 + 实时接收"
            fd_set read_fds;
            struct timeval tv;
            FD_ZERO(&read_fds);
            FD_SET(client_fd, &read_fds);
            tv.tv_sec = TCP_SELECT_TIMEOUT_SEC;
            tv.tv_usec = 0;

            int ret = select(client_fd + 1, &read_fds, NULL, NULL, &tv);

            if (ret < 0) {
                // select 错误
                osal_printk("[TCP] Select error\r\n");
                break;
            }

            // 检查是否有命令可读
            if (ret > 0 && FD_ISSET(client_fd, &read_fds)) {
                memset(recv_buf, 0, sizeof(recv_buf));
                int recv_len = recv(client_fd, recv_buf, sizeof(recv_buf) - 1, 0);
                if (recv_len <= 0) {
                    // recv 返回 0 = 客户端断开，返回负数 = 错误
                    osal_printk("[TCP] Client disconnected (recv=%d)\r\n", recv_len);
                    break;
                }
                recv_buf[recv_len] = '\0';
                osal_printk("[TCP] Recv command: %s\r\n", recv_buf);
                health_monitor_process_command(recv_buf);
            }

            // 定时发送数据（不受 select 超时影响，独立计时）
            uint32_t now = osKernelGetTickCount() * 1000 / osKernelGetTickFreq();
            if (now - last_send_time >= TCP_SEND_INTERVAL_MS) {
                last_send_time = now;
                char *json = data_fusion_build_json();
                if (json != NULL) {
                    // 发送 JSON + 换行符（协议要求 \n 分隔）
                    size_t json_len = strlen(json);
                    char *send_buf = cJSON_malloc(json_len + 2);
                    if (send_buf != NULL) {
                        memcpy(send_buf, json, json_len);
                        send_buf[json_len] = '\n';
                        send_buf[json_len + 1] = '\0';
                        int send_ret = send(client_fd, send_buf, json_len + 1, 0);
                        cJSON_free(send_buf);
                        if (send_ret < 0) {
                            osal_printk("[TCP] Send error: %d\r\n", send_ret);
                            cJSON_free(json);
                            break;
                        }
                    }
                    cJSON_free(json);
                }
            }
        }

        // 7. 客户端断开，关闭 socket，回到 accept 等待重连
        lwip_close(client_fd);
        client_fd = -1;
        osal_printk("[TCP] Client connection closed, waiting for reconnect\r\n");
    }

    // 理论上不会到达这里
    lwip_close(listen_fd);
    return NULL;
}

// 启动 TCP Server 内核任务
bool tcp_server_start(void)
{
    osThreadAttr_t attr;
    attr.name = "TCPServerTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = TCP_SERVER_TASK_STACK;
    attr.priority = TCP_SERVER_TASK_PRIO;

    if (osThreadNew((osThreadFunc_t)tcp_server_task, NULL, &attr) == NULL) {
        osal_printk("[TCP] Server task create FAILED!\r\n");
        return false;
    }
    osal_printk("[TCP] Server task created\r\n");
    return true;
}
