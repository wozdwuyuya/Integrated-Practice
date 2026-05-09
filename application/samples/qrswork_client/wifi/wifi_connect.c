/**
 * qrswork WiFi STA connection helper.
 */

#include <string.h>
#include "cmsis_os2.h"
#include "lwip/netifapi.h"
#include "securec.h"
#include "soc_osal.h"
#include "td_base.h"
#include "td_type.h"
#include "wifi_connect.h"
#include "northbound_client.h"
#include "wifi_hotspot.h"
#include "wifi_hotspot_config.h"

#define QRSWORK_WIFI_IFNAME_MAX_SIZE     16
#define QRSWORK_WIFI_MAX_SSID_LEN        33
#define QRSWORK_WIFI_SCAN_AP_LIMIT       64
#define QRSWORK_WIFI_MAC_LEN             6
#define QRSWORK_WIFI_NOT_AVAILABLE       0
#define QRSWORK_WIFI_GET_IP_MAX_COUNT    300
#define QRSWORK_WIFI_LOG                 "[qrswork wifi]"

enum {
    QRSWORK_WIFI_STA_INIT = 0,
    QRSWORK_WIFI_STA_SCANNING,
    QRSWORK_WIFI_STA_SCAN_DONE,
    QRSWORK_WIFI_STA_FOUND_TARGET,
    QRSWORK_WIFI_STA_CONNECTING,
    QRSWORK_WIFI_STA_CONNECT_DONE,
    QRSWORK_WIFI_STA_GET_IP,
};

static volatile td_u8 g_qrswork_wifi_state = QRSWORK_WIFI_STA_INIT;

static td_void qrswork_wifi_scan_state_changed(td_s32 state, td_s32 size)
{
    (void)state;
    (void)size;
    osal_printk("%s scan done\r\n", QRSWORK_WIFI_LOG);
    g_qrswork_wifi_state = QRSWORK_WIFI_STA_SCAN_DONE;
}

static td_void qrswork_wifi_connection_changed(td_s32 state, const wifi_linked_info_stru *info, td_s32 reason_code)
{
    (void)info;
    (void)reason_code;

    if (state == QRSWORK_WIFI_NOT_AVAILABLE) {
        osal_printk("%s connect failed, retrying\r\n", QRSWORK_WIFI_LOG);
        g_qrswork_wifi_state = QRSWORK_WIFI_STA_INIT;
        northbound_client_notify_wifi_disconnect();
    } else {
        osal_printk("%s connect success\r\n", QRSWORK_WIFI_LOG);
        g_qrswork_wifi_state = QRSWORK_WIFI_STA_CONNECT_DONE;
    }
}

static wifi_event_stru g_qrswork_wifi_event_cb = {
    .wifi_event_connection_changed = qrswork_wifi_connection_changed,
    .wifi_event_scan_state_changed = qrswork_wifi_scan_state_changed,
};

static td_s32 qrswork_wifi_get_match_network(wifi_sta_config_stru *expected_bss,
    const char *ssid, const char *psk)
{
    td_u32 num = QRSWORK_WIFI_SCAN_AP_LIMIT;
    td_bool find_ap = TD_FALSE;
    td_u8 bss_index = 0;
    td_u32 scan_len = sizeof(wifi_scan_info_stru) * QRSWORK_WIFI_SCAN_AP_LIMIT;
    wifi_scan_info_stru *result = osal_kmalloc(scan_len, OSAL_GFP_ATOMIC);
    if (result == TD_NULL) {
        return -1;
    }
    memset_s(result, scan_len, 0, scan_len);

    if (wifi_sta_get_scan_info(result, &num) != 0) {
        osal_kfree(result);
        return -1;
    }

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
    expected_bss->ip_type = 1;

    osal_kfree(result);
    return 0;
}

static int qrswork_wifi_check_dhcp_status(struct netif *netif_p, td_u32 *wait_count)
{
    if ((ip_addr_isany(&(netif_p->ip_addr)) == 0) && (*wait_count <= QRSWORK_WIFI_GET_IP_MAX_COUNT)) {
        osal_printk("%s DHCP success\r\n", QRSWORK_WIFI_LOG);
        return 0;
    }

    if (*wait_count > QRSWORK_WIFI_GET_IP_MAX_COUNT) {
        osal_printk("%s DHCP timeout, retrying\r\n", QRSWORK_WIFI_LOG);
        *wait_count = 0;
        g_qrswork_wifi_state = QRSWORK_WIFI_STA_INIT;
    }
    return -1;
}

static td_s32 qrswork_wifi_sta_connect(const char *ssid, const char *psk)
{
    td_char ifname[QRSWORK_WIFI_IFNAME_MAX_SIZE + 1] = "wlan0";
    wifi_sta_config_stru expected_bss = {0};
    struct netif *netif_p = TD_NULL;
    td_u32 wait_count = 0;

    if (wifi_sta_enable() != 0) {
        return -1;
    }
    osal_printk("%s STA enable success\r\n", QRSWORK_WIFI_LOG);

    do {
        (void)osDelay(1);
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
                break;
            }
            wait_count++;
        }
    } while (1);

    return 0;
}

int qrswork_wifi_connect(const char *ssid, const char *psk)
{
    g_qrswork_wifi_state = QRSWORK_WIFI_STA_INIT;
    if (wifi_register_event_cb(&g_qrswork_wifi_event_cb) != 0) {
        osal_printk("%s event callback register failed\r\n", QRSWORK_WIFI_LOG);
        return -1;
    }
    osal_printk("%s event callback registered\r\n", QRSWORK_WIFI_LOG);

    while (wifi_is_wifi_inited() == 0) {
        (void)osDelay(10);
    }
    osal_printk("%s init success\r\n", QRSWORK_WIFI_LOG);

    if (qrswork_wifi_sta_connect(ssid, psk) != 0) {
        osal_printk("%s STA connect failed\r\n", QRSWORK_WIFI_LOG);
        return -1;
    }
    return 0;
}
