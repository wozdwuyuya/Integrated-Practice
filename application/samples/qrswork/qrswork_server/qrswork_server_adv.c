/**
 * qrswork server advertisement implementation
 */
#include "securec.h"
#include "errcode.h"
#include "osal_addr.h"
#include "product.h"
#include "sle_common.h"
#include "qrswork_server.h"
#include "sle_device_discovery.h"
#include "sle_errcode.h"
#include "osal_debug.h"
#include "osal_task.h"
#include "string.h"
#include "qrswork_server_adv.h"

#define NAME_MAX_LENGTH 32
#define SLE_CONN_INTV_MIN_DEFAULT                 0x64
#define SLE_CONN_INTV_MAX_DEFAULT                 0x64
#define SLE_ADV_INTERVAL_MIN_DEFAULT              0xC8
#define SLE_ADV_INTERVAL_MAX_DEFAULT              0xC8
#define SLE_CONN_SUPERVISION_TIMEOUT_DEFAULT      0x1F4
#define SLE_CONN_MAX_LATENCY                      0x1F3
#define SLE_ADV_TX_POWER  10
#define SLE_ADV_HANDLE_DEFAULT                    1
#define SLE_ADV_DATA_LEN_MAX                      251

static uint8_t qrswork_local_name[NAME_MAX_LENGTH] = QRSWORK_SERVER_NAME;
#define QRSWORK_SERVER_LOG "[qrswork server]"

static uint16_t qrswork_set_adv_local_name(uint8_t *adv_data, uint16_t max_len)
{
    errno_t ret;
    uint8_t index = 0;

    uint8_t *local_name = qrswork_local_name;
    uint8_t local_name_len = strlen((char *)qrswork_local_name);
    osal_printk("%s local_name_len = %d\r\n", QRSWORK_SERVER_LOG, local_name_len);
    
    adv_data[index++] = local_name_len + 1;
    adv_data[index++] = SLE_ADV_DATA_TYPE_COMPLETE_LOCAL_NAME;
    ret = memcpy_s(&adv_data[index], max_len - index, local_name, local_name_len);
    if (ret != EOK) {
        osal_printk("%s memcpy fail\r\n", QRSWORK_SERVER_LOG);
        return 0;
    }
    return (uint16_t)index + local_name_len;
}

static uint16_t qrswork_set_adv_data(uint8_t *adv_data)
{
    size_t len = 0;
    uint16_t idx = 0;
    errno_t  ret = 0;

    len = sizeof(struct sle_adv_common_value);
    struct sle_adv_common_value adv_disc_level = {
        .length = len - 1,
        .type = SLE_ADV_DATA_TYPE_DISCOVERY_LEVEL,
        .value = SLE_ANNOUNCE_LEVEL_NORMAL,
    };
    ret = memcpy_s(&adv_data[idx], SLE_ADV_DATA_LEN_MAX - idx, &adv_disc_level, len);
    if (ret != EOK) {
        osal_printk("%s adv_disc_level memcpy fail\r\n", QRSWORK_SERVER_LOG);
        return 0;
    }
    idx += len;

    len = sizeof(struct sle_adv_common_value);
    struct sle_adv_common_value adv_access_mode = {
        .length = len - 1,
        .type = SLE_ADV_DATA_TYPE_ACCESS_MODE,
        .value = 0,
    };
    ret = memcpy_s(&adv_data[idx], SLE_ADV_DATA_LEN_MAX - idx, &adv_access_mode, len);
    if (ret != EOK) {
        osal_printk("%s adv_access_mode memcpy fail\r\n", QRSWORK_SERVER_LOG);
        return 0;
    }
    idx += len;

    return idx;
}

static uint16_t qrswork_set_scan_response_data(uint8_t *scan_rsp_data)
{
    uint16_t idx = 0;
    errno_t ret;
    size_t scan_rsp_data_len = sizeof(struct sle_adv_common_value);

    struct sle_adv_common_value tx_power_level = {
        .length = scan_rsp_data_len - 1,
        .type = SLE_ADV_DATA_TYPE_TX_POWER_LEVEL,
        .value = SLE_ADV_TX_POWER,
    };
    ret = memcpy_s(scan_rsp_data, SLE_ADV_DATA_LEN_MAX, &tx_power_level, scan_rsp_data_len);
    if (ret != EOK) {
        osal_printk("%s scan response data memcpy fail\r\n", QRSWORK_SERVER_LOG);
        return 0;
    }
    idx += scan_rsp_data_len;

    idx += qrswork_set_adv_local_name(&scan_rsp_data[idx], SLE_ADV_DATA_LEN_MAX - idx);
    return idx;
}

static int qrswork_set_default_announce_param(void)
{
    errno_t ret;
    sle_announce_param_t param = {0};
    unsigned char local_addr[SLE_ADDR_LEN] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
    
    param.announce_mode = SLE_ANNOUNCE_MODE_CONNECTABLE_SCANABLE;
    param.announce_handle = SLE_ADV_HANDLE_DEFAULT;
    param.announce_gt_role = SLE_ANNOUNCE_ROLE_T_CAN_NEGO;
    param.announce_level = SLE_ANNOUNCE_LEVEL_NORMAL;
    param.announce_channel_map = SLE_ADV_CHANNEL_MAP_DEFAULT;
    param.announce_interval_min = SLE_ADV_INTERVAL_MIN_DEFAULT;
    param.announce_interval_max = SLE_ADV_INTERVAL_MAX_DEFAULT;
    param.conn_interval_min = SLE_CONN_INTV_MIN_DEFAULT;
    param.conn_interval_max = SLE_CONN_INTV_MAX_DEFAULT;
    param.conn_max_latency = SLE_CONN_MAX_LATENCY;
    param.conn_supervision_timeout = SLE_CONN_SUPERVISION_TIMEOUT_DEFAULT;
    param.announce_tx_power = 18;
    param.own_addr.type = 0;
    ret = memcpy_s(param.own_addr.addr, SLE_ADDR_LEN, local_addr, SLE_ADDR_LEN);
    if (ret != EOK) {
        osal_printk("%s announce param memcpy fail\r\n", QRSWORK_SERVER_LOG);
        return ERRCODE_SLE_FAIL;
    }

    if (sle_set_announce_param(param.announce_handle, &param) != ERRCODE_SLE_SUCCESS) {
        osal_printk("%s set announce param fail\r\n", QRSWORK_SERVER_LOG);
        return ERRCODE_SLE_FAIL;
    }
    return ERRCODE_SLE_SUCCESS;
}

static int qrswork_set_adv_data_and_start(void)
{
    uint8_t adv_handle = SLE_ADV_HANDLE_DEFAULT;
    uint8_t adv_data[SLE_ADV_DATA_LEN_MAX] = {0};
    uint8_t scan_rsp_data[SLE_ADV_DATA_LEN_MAX] = {0};
    uint16_t adv_data_len;
    uint16_t scan_rsp_data_len;
    sle_announce_data_t data = {0};

    adv_data_len = qrswork_set_adv_data(adv_data);
    scan_rsp_data_len = qrswork_set_scan_response_data(scan_rsp_data);
    
    data.announce_data = adv_data;
    data.announce_data_len = adv_data_len;
    data.seek_rsp_data = scan_rsp_data;
    data.seek_rsp_data_len = scan_rsp_data_len;
    
    if (sle_set_announce_data(adv_handle, &data) != ERRCODE_SLE_SUCCESS) {
        osal_printk("%s set announce data fail\r\n", QRSWORK_SERVER_LOG);
        return ERRCODE_SLE_FAIL;
    }
    
    if (sle_start_announce(adv_handle) != ERRCODE_SLE_SUCCESS) {
        osal_printk("%s start announce fail\r\n", QRSWORK_SERVER_LOG);
        return ERRCODE_SLE_FAIL;
    }
    
    osal_printk("%s announce started\r\n", QRSWORK_SERVER_LOG);
    return ERRCODE_SLE_SUCCESS;
}

errcode_t qrswork_server_adv_init(void)
{
    if (qrswork_set_default_announce_param() != ERRCODE_SLE_SUCCESS) {
        return ERRCODE_SLE_FAIL;
    }
    
    if (qrswork_set_adv_data_and_start() != ERRCODE_SLE_SUCCESS) {
        return ERRCODE_SLE_FAIL;
    }
    
    return ERRCODE_SLE_SUCCESS;
}
