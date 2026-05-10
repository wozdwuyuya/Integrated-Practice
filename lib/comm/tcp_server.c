/**
 * @file tcp_server.c
 * @brief TCP Server 与 WiFi AP 模式初始化
 * @note  阶段 2：实现 WiFi SoftAP 热点初始化
 * @note  阶段 3 补充：TCP socket 监听、数据泵、命令接收
 */

#include "comm/tcp_server.h"
#include "lwip/netifapi.h"
#include "wifi_hotspot.h"
#include "wifi_hotspot_config.h"
#include "wifi_device.h"
#include "td_base.h"
#include "td_type.h"
#include "soc_osal.h"
#include "osal_debug.h"
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

// 阶段 3 实现：tcp_server_start()
bool tcp_server_start(void)
{
    // TODO: 阶段 3 - TCP socket 监听、accept、数据泵、命令接收
    return false;
}
