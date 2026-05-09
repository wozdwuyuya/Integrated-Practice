/**
 * qrswork WiFi STA connection helper.
 *
 * 功能概述：
 *   WiFi STA 模式连接辅助模块。负责扫描、匹配目标 AP、连接、DHCP 获取 IP。
 *   基于海思 SDK 官方 sta_sample.c 范例改造，新增参数化 SSID/PSK 和断线通知机制。
 *
 * 状态机流程：
 *   INIT → SCANNING → SCAN_DONE → FOUND_TARGET → CONNECTING → CONNECT_DONE → GET_IP → 完成
 *   任何阶段失败都会回退到 INIT 重新开始。
 *
 * 与 SDK 范例的关系：
 *   本文件的 WiFi 连接逻辑（状态机、扫描匹配、DHCP 检查）与
 *   application/samples/wifi/sta_sample/sta_sample.c 基本一致，
 *   主要差异在于：
 *   1. SSID/PSK 通过函数参数传入（SDK 范例硬编码）
 *   2. 断线时通知北向任务重建 socket（SDK 范例无此机制）
 *   3. 共享变量添加 volatile 修饰（SDK 范例遗漏了这一点）
 */

#include <string.h>
#include "cmsis_os2.h"
#include "lwip/netifapi.h"
#include "securec.h"
#include "soc_osal.h"
#include "td_base.h"
#include "td_type.h"
#include "wifi_connect.h"
#include "northbound_client.h"  // [AI 优化] 引入北向模块接口，用于断线通知
#include "wifi_hotspot.h"
#include "wifi_hotspot_config.h"

/* ==========================================================================
 * 宏定义（与 SDK sta_sample.c 对齐）
 * ========================================================================== */

#define QRSWORK_WIFI_IFNAME_MAX_SIZE     16   // WiFi 接口名最大长度
#define QRSWORK_WIFI_MAX_SSID_LEN        33   // SSID 最大长度（含 '\0'）
#define QRSWORK_WIFI_SCAN_AP_LIMIT       64   // 单次扫描最多记录的 AP 数量
#define QRSWORK_WIFI_MAC_LEN             6    // MAC 地址长度（BSSID）
#define QRSWORK_WIFI_NOT_AVAILABLE       0    // WiFi 连接状态：不可用/断开

// DHCP 获取 IP 的最大等待次数（每次 10ms，共 3 秒超时）
// 原教程逻辑位置：SDK sta_sample.c 中同样定义为 300
#define QRSWORK_WIFI_GET_IP_MAX_COUNT    300

#define QRSWORK_WIFI_LOG                 "[qrswork wifi]"

/* ==========================================================================
 * WiFi 连接状态机枚举
 * ========================================================================== */

// 状态机定义，与 SDK sta_sample.c 中的 wifi_state_enum 一一对应
// 原教程逻辑位置：SDK 定义为 WIFI_STA_SAMPLE_INIT / SCANING / SCAN_DONE 等
enum {
    QRSWORK_WIFI_STA_INIT = 0,         // 初始态，准备开始扫描
    QRSWORK_WIFI_STA_SCANNING,         // 扫描中
    QRSWORK_WIFI_STA_SCAN_DONE,        // 扫描完成
    QRSWORK_WIFI_STA_FOUND_TARGET,     // 匹配到目标 AP
    QRSWORK_WIFI_STA_CONNECTING,       // 连接中
    QRSWORK_WIFI_STA_CONNECT_DONE,     // 关联成功
    QRSWORK_WIFI_STA_GET_IP,           // 等待 DHCP 获取 IP
};

// [AI 优化] 添加 volatile 修饰。此变量在 WiFi 回调（事件任务上下文）中写入，
// 在 qrswork_wifi_sta_connect（主任务上下文）的 do-while 循环中读取。
// 若不加 volatile，编译器在 -O2 优化下可能将 g_qrswork_wifi_state 缓存到寄存器，
// 导致状态机永远卡在某个状态。SDK 官方范例 (sta_sample.c) 同样遗漏了 volatile。
static volatile td_u8 g_qrswork_wifi_state = QRSWORK_WIFI_STA_INIT;

/* ==========================================================================
 * WiFi 事件回调函数
 * ========================================================================== */

/**
 * @brief WiFi 扫描完成回调
 * @note  原教程逻辑位置：SDK sta_sample.c 中 wifi_scan_state_changed() 实现完全相同。
 *        由 WiFi 驱动在扫描完成后调用，将状态机推进到 SCAN_DONE。
 */
static td_void qrswork_wifi_scan_state_changed(td_s32 state, td_s32 size)
{
    (void)state;
    (void)size;
    osal_printk("%s scan done\r\n", QRSWORK_WIFI_LOG);
    g_qrswork_wifi_state = QRSWORK_WIFI_STA_SCAN_DONE;
}

/**
 * @brief WiFi 连接状态变更回调
 * @param state 连接状态，0 = QRSWORK_WIFI_NOT_AVAILABLE（断开），其他 = 连接成功
 *
 * @note  原教程逻辑位置：SDK sta_sample.c 中 wifi_connection_changed() 逻辑相同。
 *
 *   [AI 优化] 原代码在断线时只重置状态机为 INIT，不会通知北向任务。
 *   北向任务仍在旧 socket 上 sendto，会持续失败且无法恢复。
 *   现在断线时额外调用 northbound_client_notify_wifi_disconnect()，
 *   通过消息队列通知北向任务关闭旧 socket 并重建。
 */
static td_void qrswork_wifi_connection_changed(td_s32 state, const wifi_linked_info_stru *info, td_s32 reason_code)
{
    (void)info;
    (void)reason_code;

    if (state == QRSWORK_WIFI_NOT_AVAILABLE) {
        osal_printk("%s connect failed, retrying\r\n", QRSWORK_WIFI_LOG);
        g_qrswork_wifi_state = QRSWORK_WIFI_STA_INIT;
        northbound_client_notify_wifi_disconnect();  // [AI 优化] 断线通知
    } else {
        osal_printk("%s connect success\r\n", QRSWORK_WIFI_LOG);
        g_qrswork_wifi_state = QRSWORK_WIFI_STA_CONNECT_DONE;
    }
}

// WiFi 事件回调结构体，注册到驱动层
static wifi_event_stru g_qrswork_wifi_event_cb = {
    .wifi_event_connection_changed = qrswork_wifi_connection_changed,
    .wifi_event_scan_state_changed = qrswork_wifi_scan_state_changed,
};

/* ==========================================================================
 * AP 匹配与连接
 * ========================================================================== */

/**
 * @brief 从扫描结果中匹配目标 SSID，填充连接配置结构体
 * @param expected_bss [out] 连接配置（SSID、BSSID、密码、加密类型）
 * @param ssid 目标 WiFi 名称
 * @param psk  WiFi 密码
 * @return 0 匹配成功，-1 未找到或内存分配失败
 *
 * @note  原教程逻辑位置：SDK sta_sample.c 中 example_get_match_network() 实现几乎相同。
 *        主要差异：SDK 范例硬编码 SSID/PSK，本函数通过参数传入。
 *        内存管理：使用 osal_kmalloc 分配扫描结果缓冲区，用完必须 osal_kfree 释放，
 *        所有错误路径都已检查并释放内存（共 6 个 return -1 分支，每个都先 free）。
 */
static td_s32 qrswork_wifi_get_match_network(wifi_sta_config_stru *expected_bss,
    const char *ssid, const char *psk)
{
    td_u32 num = QRSWORK_WIFI_SCAN_AP_LIMIT;
    td_bool find_ap = TD_FALSE;
    td_u8 bss_index = 0;

    // 分配扫描结果缓冲区（64 个 AP 信息结构体）
    td_u32 scan_len = sizeof(wifi_scan_info_stru) * QRSWORK_WIFI_SCAN_AP_LIMIT;
    wifi_scan_info_stru *result = osal_kmalloc(scan_len, OSAL_GFP_ATOMIC);
    if (result == TD_NULL) {
        return -1;
    }
    memset_s(result, scan_len, 0, scan_len);

    // 获取扫描结果
    if (wifi_sta_get_scan_info(result, &num) != 0) {
        osal_kfree(result);
        return -1;
    }

    // 遍历扫描结果，逐个比较 SSID 名称
    for (bss_index = 0; bss_index < num; bss_index++) {
        if ((strlen(ssid) == strlen(result[bss_index].ssid)) &&
            (memcmp(ssid, result[bss_index].ssid, strlen(ssid)) == 0)) {
            find_ap = TD_TRUE;
            break;
        }
    }

    if (find_ap == TD_FALSE) {
        osal_kfree(result);
        return -1;
    }

    // 找到目标 AP，复制连接信息到输出参数
    if (memcpy_s(expected_bss->ssid, QRSWORK_WIFI_MAX_SSID_LEN, ssid, strlen(ssid)) != EOK) {
        osal_kfree(result);
        return -1;
    }
    if (memcpy_s(expected_bss->bssid, QRSWORK_WIFI_MAC_LEN, result[bss_index].bssid,
        QRSWORK_WIFI_MAC_LEN) != EOK) {
        osal_kfree(result);
        return -1;
    }
    expected_bss->security_type = result[bss_index].security_type;
    if (memcpy_s(expected_bss->pre_shared_key, QRSWORK_WIFI_MAX_SSID_LEN, psk, strlen(psk)) != EOK) {
        osal_kfree(result);
        return -1;
    }
    expected_bss->ip_type = 1;  // 1 = DHCP 动态获取 IP

    osal_kfree(result);
    return 0;
}

/**
 * @brief 检查 DHCP 是否成功获取到 IP 地址
 * @param netif_p    网络接口指针
 * @param wait_count [in/out] 已等待次数
 * @return 0 DHCP 成功，-1 仍在等待或超时
 *
 * @note  原教程逻辑位置：SDK sta_sample.c 中 example_check_dhcp_status() 逻辑相同。
 *
 *   [AI 优化] 原代码返回类型为 td_bool，但实际返回 0 和 -1（int 语义）。
 *   td_bool 在海思 SDK 中通常表示 TD_TRUE/TD_FALSE，与 0/-1 不匹配。
 *   已修正返回类型为 int，消除隐式类型转换的隐患。
 */
static int qrswork_wifi_check_dhcp_status(struct netif *netif_p, td_u32 *wait_count)
{
    // ip_addr_isany 返回 0 表示 IP 地址已分配（非全零）
    if ((ip_addr_isany(&(netif_p->ip_addr)) == 0) && (*wait_count <= QRSWORK_WIFI_GET_IP_MAX_COUNT)) {
        osal_printk("%s DHCP success\r\n", QRSWORK_WIFI_LOG);
        return 0;
    }

    if (*wait_count > QRSWORK_WIFI_GET_IP_MAX_COUNT) {
        osal_printk("%s DHCP timeout, retrying\r\n", QRSWORK_WIFI_LOG);
        *wait_count = 0;
        g_qrswork_wifi_state = QRSWORK_WIFI_STA_INIT;  // 超时回退到初始态
    }
    return -1;
}

/**
 * @brief WiFi STA 连接状态机主函数
 * @param ssid 目标 WiFi 名称
 * @param psk  WiFi 密码
 * @return 0 连接并获取 IP 成功，-1 wifi_sta_enable 失败
 *
 * @note  原教程逻辑位置：SDK sta_sample.c 中 example_sta_function() 实现几乎相同。
 *        状态机在 do-while(1) 循环中运行，每 10ms 检查一次状态。
 *        唯一的退出点是 DHCP 成功（break 跳出循环）。
 */
static td_s32 qrswork_wifi_sta_connect(const char *ssid, const char *psk)
{
    td_char ifname[QRSWORK_WIFI_IFNAME_MAX_SIZE + 1] = "wlan0";  // WiFi 接口名
    wifi_sta_config_stru expected_bss = {0};  // AP 连接配置
    struct netif *netif_p = TD_NULL;          // 网络接口指针（DHCP 用）
    td_u32 wait_count = 0;                    // DHCP 等待计数器

    // 使能 WiFi STA 模式
    if (wifi_sta_enable() != 0) {
        return -1;
    }
    osal_printk("%s STA enable success\r\n", QRSWORK_WIFI_LOG);

    // 状态机主循环
    do {
        (void)osDelay(1);  // 每次循环等待 10ms（osDelay(1) = 10ms in LiteOS）
        if (g_qrswork_wifi_state == QRSWORK_WIFI_STA_INIT) {
            osal_printk("%s scan start\r\n", QRSWORK_WIFI_LOG);
            g_qrswork_wifi_state = QRSWORK_WIFI_STA_SCANNING;
            if (wifi_sta_scan() != 0) {
                g_qrswork_wifi_state = QRSWORK_WIFI_STA_INIT;
                continue;
            }
        } else if (g_qrswork_wifi_state == QRSWORK_WIFI_STA_SCAN_DONE) {
            if (qrswork_wifi_get_match_network(&expected_bss, ssid, psk) != 0) {
                osal_printk("%s target AP not found, retrying\r\n", QRSWORK_WIFI_LOG);
                g_qrswork_wifi_state = QRSWORK_WIFI_STA_INIT;
                continue;
            }
            g_qrswork_wifi_state = QRSWORK_WIFI_STA_FOUND_TARGET;
        } else if (g_qrswork_wifi_state == QRSWORK_WIFI_STA_FOUND_TARGET) {
            osal_printk("%s connect start\r\n", QRSWORK_WIFI_LOG);
            g_qrswork_wifi_state = QRSWORK_WIFI_STA_CONNECTING;
            if (wifi_sta_connect(&expected_bss) != 0) {
                g_qrswork_wifi_state = QRSWORK_WIFI_STA_INIT;
                continue;
            }
        } else if (g_qrswork_wifi_state == QRSWORK_WIFI_STA_CONNECT_DONE) {
            osal_printk("%s DHCP start\r\n", QRSWORK_WIFI_LOG);
            g_qrswork_wifi_state = QRSWORK_WIFI_STA_GET_IP;
            netif_p = netifapi_netif_find(ifname);
            if ((netif_p == TD_NULL) || (netifapi_dhcp_start(netif_p) != 0)) {
                osal_printk("%s netif or DHCP start failed, retrying\r\n", QRSWORK_WIFI_LOG);
                g_qrswork_wifi_state = QRSWORK_WIFI_STA_INIT;
                continue;
            }
        } else if (g_qrswork_wifi_state == QRSWORK_WIFI_STA_GET_IP) {
            if (qrswork_wifi_check_dhcp_status(netif_p, &wait_count) == 0) {
                break;  // DHCP 成功，退出状态机
            }
            wait_count++;
        }
    } while (1);

    return 0;
}

/**
 * @brief WiFi 连接入口函数（对外接口）
 * @param ssid 目标 WiFi 名称
 * @param psk  WiFi 密码
 * @return 0 连接成功，-1 失败
 *
 * @note  原教程逻辑位置：SDK sta_sample.c 中 wifi_connect() 实现相同。
 *        执行顺序：注册事件回调 → 等待 WiFi 子系统初始化 → 启动状态机连接。
 */
int qrswork_wifi_connect(const char *ssid, const char *psk)
{
    g_qrswork_wifi_state = QRSWORK_WIFI_STA_INIT;

    // 注册 WiFi 事件回调（扫描完成、连接状态变更）
    if (wifi_register_event_cb(&g_qrswork_wifi_event_cb) != 0) {
        osal_printk("%s event callback register failed\r\n", QRSWORK_WIFI_LOG);
        return -1;
    }
    osal_printk("%s event callback registered\r\n", QRSWORK_WIFI_LOG);

    // 等待 WiFi 子系统初始化完成（驱动加载、固件就绪等）
    while (wifi_is_wifi_inited() == 0) {
        (void)osDelay(10);  // 每 100ms 检查一次
    }
    osal_printk("%s init success\r\n", QRSWORK_WIFI_LOG);

    // 启动连接状态机
    if (qrswork_wifi_sta_connect(ssid, psk) != 0) {
        osal_printk("%s STA connect failed\r\n", QRSWORK_WIFI_LOG);
        return -1;
    }
    return 0;
}
