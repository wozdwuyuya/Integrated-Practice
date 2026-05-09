/**
 * qrswork client implementation
 * Receives heart rate data from server and outputs via UART
 */
#include "common_def.h"
#include "soc_osal.h"
#include "securec.h"
#include "product.h"
#include "bts_le_gap.h"
#include "sle_device_discovery.h"
#include "sle_connection_manager.h"
#include "sle_ssap_client.h"
#include "sle_errcode.h"
#include "qrswork_client.h"
#include "uart.h"
#if defined(CONFIG_QRSWORK_NORTHBOUND_ENABLE)
#include "northbound_client.h"
#endif

#define SLE_MTU_SIZE_DEFAULT            520
#define SLE_SEEK_INTERVAL_DEFAULT       100
#define SLE_SEEK_WINDOW_DEFAULT         100
#define UUID_16BIT_LEN                  2
#define UUID_128BIT_LEN                 16
#define SLE_UART_TASK_DELAY_MS          1000
#define SLE_UART_WAIT_SLE_CORE_READY_MS 5000
#define QRSWORK_CLIENT_LOG              "[qrswork client]"

static ssapc_find_service_result_t g_qrswork_find_service_result = { 0 };
static sle_announce_seek_callbacks_t g_qrswork_seek_cbk = { 0 };
static sle_connection_callbacks_t g_qrswork_connect_cbk = { 0 };
static ssapc_callbacks_t g_qrswork_ssapc_cbk = { 0 };
static sle_addr_t g_qrswork_remote_addr = { 0 };
static uint16_t g_qrswork_conn_id = 0;

uint16_t get_qrswork_conn_id(void)
{
    return g_qrswork_conn_id;
}

void qrswork_start_scan(void)
{
    sle_seek_param_t param = { 0 };
    param.own_addr_type = 0;
    param.filter_duplicates = 0;
    param.seek_filter_policy = 0;
    param.seek_phys = 1;
    param.seek_type[0] = 1;
    param.seek_interval[0] = SLE_SEEK_INTERVAL_DEFAULT;
    param.seek_window[0] = SLE_SEEK_WINDOW_DEFAULT;
    sle_set_seek_param(&param);
    sle_start_seek();
}

static void qrswork_client_sample_sle_enable_cbk(errcode_t status)
{
    osal_printk("%s sle enable callback, status: %d\r\n", QRSWORK_CLIENT_LOG, status);
    if (status == ERRCODE_SLE_SUCCESS) {
        osal_printk("%s starting scan...\r\n", QRSWORK_CLIENT_LOG);
        qrswork_start_scan();
    }
}

static void qrswork_client_sample_seek_enable_cbk(errcode_t status)
{
    if (status != 0) {
        osal_printk("%s seek enable error, status=%x\r\n", QRSWORK_CLIENT_LOG, status);
    }
}

static void qrswork_client_sample_seek_result_info_cbk(sle_seek_result_info_t *seek_result_data)
{
    osal_printk("%s scan data: %s\r\n", QRSWORK_CLIENT_LOG, seek_result_data->data);
    if (seek_result_data == NULL) {
        osal_printk("status error\r\n");
    } else if (strstr((const char *)seek_result_data->data, QRSWORK_SERVER_NAME) != NULL) {
        memcpy_s(&g_qrswork_remote_addr, sizeof(sle_addr_t), &seek_result_data->addr, sizeof(sle_addr_t));
        sle_stop_seek();
    }
}

static void qrswork_client_sample_seek_disable_cbk(errcode_t status)
{
    if (status != 0) {
        osal_printk("%s seek disable error=%x\r\n", QRSWORK_CLIENT_LOG, status);
    } else {
        sle_remove_paired_remote_device(&g_qrswork_remote_addr);
        sle_connect_remote_device(&g_qrswork_remote_addr);
    }
}

static void qrswork_client_sample_seek_cbk_register(void)
{
    g_qrswork_seek_cbk.sle_enable_cb = qrswork_client_sample_sle_enable_cbk;
    g_qrswork_seek_cbk.seek_enable_cb = qrswork_client_sample_seek_enable_cbk;
    g_qrswork_seek_cbk.seek_result_cb = qrswork_client_sample_seek_result_info_cbk;
    g_qrswork_seek_cbk.seek_disable_cb = qrswork_client_sample_seek_disable_cbk;
    sle_announce_seek_register_callbacks(&g_qrswork_seek_cbk);
}

static void qrswork_client_connect_state_changed_cbk(uint16_t conn_id, const sle_addr_t *addr,
    sle_acb_state_t conn_state, sle_pair_state_t pair_state, sle_disc_reason_t disc_reason)
{
    osal_printk("%s conn state changed conn_id:%d, conn_state:%d, pair_state:%d, disc_reason:%x\r\n",
        QRSWORK_CLIENT_LOG, conn_id, conn_state, pair_state, disc_reason);
    g_qrswork_conn_id = conn_id;
    if (conn_state == SLE_ACB_STATE_CONNECTED) {
        osal_printk("%s SLE connected\r\n", QRSWORK_CLIENT_LOG);
        if (pair_state == SLE_PAIR_NONE) {
            osal_printk("%s start pairing...\r\n", QRSWORK_CLIENT_LOG);
            sle_pair_remote_device(&g_qrswork_remote_addr);
        }
    } else if (conn_state == SLE_ACB_STATE_DISCONNECTED) {
        osal_printk("%s SLE disconnected\r\n", QRSWORK_CLIENT_LOG);
        qrswork_start_scan();
    }
}

static void qrswork_client_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status)
{
    osal_printk("%s pair complete conn_id:%d, status:%d\r\n", QRSWORK_CLIENT_LOG, conn_id, status);
    if (status == ERRCODE_SLE_SUCCESS) {
        ssap_exchange_info_t info = {0};
        info.mtu_size = SLE_MTU_SIZE_DEFAULT;
        info.version = 1;
        ssapc_exchange_info_req(0, g_qrswork_conn_id, &info);
    }
}

static void qrswork_client_connect_cbk_register(void)
{
    g_qrswork_connect_cbk.connect_state_changed_cb = qrswork_client_connect_state_changed_cbk;
    g_qrswork_connect_cbk.pair_complete_cb = qrswork_client_pair_complete_cbk;
    sle_connection_register_callbacks(&g_qrswork_connect_cbk);
}

static void qrswork_client_exchange_info_cbk(uint8_t client_id, uint16_t conn_id, 
    ssap_exchange_info_t *param, errcode_t status)
{
    osal_printk("%s exchange mtu, client id:%d, conn id:%d, mtu size:%d, status:%d\r\n",
        QRSWORK_CLIENT_LOG, client_id, conn_id, param->mtu_size, status);
    
    // 交换完 MTU 后，开始发现属性
    ssapc_find_structure_param_t find_param = {0};
    find_param.type = SSAP_FIND_TYPE_PROPERTY;
    find_param.start_hdl = 1;
    find_param.end_hdl = 0xFFFF;
    ssapc_find_structure(0, conn_id, &find_param);
}

static void qrswork_client_find_structure_cbk(uint8_t client_id, uint16_t conn_id,
    ssapc_find_service_result_t *service, errcode_t status)
{
    osal_printk("%s find structure, client id:%d, conn id:%d, status:%d\r\n",
        QRSWORK_CLIENT_LOG, client_id, conn_id, status);
    if (service != NULL) {
        g_qrswork_find_service_result.start_hdl = service->start_hdl;
        g_qrswork_find_service_result.end_hdl = service->end_hdl;
        osal_printk("%s find service, start_hdl:%d, end_hdl:%d\r\n", 
            QRSWORK_CLIENT_LOG, service->start_hdl, service->end_hdl);
    }
}

static void qrswork_client_find_property_cbk(uint8_t client_id, uint16_t conn_id,
    ssapc_find_property_result_t *property, errcode_t status)
{
    osal_printk("%s find property, client id:%d, conn id:%d, handle:%d, status:%d\r\n",
        QRSWORK_CLIENT_LOG, client_id, conn_id, property->handle, status);
    osal_printk("%s property operate_indication:0x%x, descriptors_count:%d\r\n", 
        QRSWORK_CLIENT_LOG, property->operate_indication, property->descriptors_count);
    
    // 保存property handle供后续使用
    g_qrswork_find_service_result.start_hdl = property->handle;
    
    // 如果属性支持notification，尝试写入CCCD
    // SSAP_OPERATE_INDICATION_BIT_NOTIFY = 0x08
    if (property->operate_indication & 0x08) {  // bit 3 = notify capability
        osal_printk("%s property supports notification, descriptors_count:%d\r\n", 
            QRSWORK_CLIENT_LOG, property->descriptors_count);
        
        // 方法1: 写入CCCD，使用property handle + descriptors_count作为描述符句柄
        uint8_t enable_ntf[] = {0x01, 0x00};  // 0x0001 = Enable Notification
        ssapc_write_param_t write_param = {0};
        
        // 根据descriptors_count判断descriptor位置
        if (property->descriptors_count > 0) {
            write_param.handle = property->handle + property->descriptors_count;
            write_param.type = SSAP_DESCRIPTOR_CLIENT_CONFIGURATION;
        } else {
            // 没有descriptor，尝试直接写property
            write_param.handle = property->handle;
            write_param.type = SSAP_PROPERTY_TYPE_VALUE;
        }
        
        write_param.data_len = sizeof(enable_ntf);
        write_param.data = enable_ntf;
        
        errcode_t ret = ssapc_write_req(client_id, conn_id, &write_param);
        if (ret == ERRCODE_SLE_SUCCESS) {
            osal_printk("%s write request sent, handle:0x%x, type:0x%x\r\n", 
                QRSWORK_CLIENT_LOG, write_param.handle, write_param.type);
        } else {
            osal_printk("%s write request failed, ret:0x%x\r\n", QRSWORK_CLIENT_LOG, ret);
        }
    } else {
        osal_printk("%s property doesn't support notification\r\n", QRSWORK_CLIENT_LOG);
    }
}

static void qrswork_client_find_structure_cmp_cbk(uint8_t client_id, uint16_t conn_id,
    ssapc_find_structure_result_t *structure_result, errcode_t status)
{
    osal_printk("%s find structure complete, client id:%d, status:%d\r\n",
        QRSWORK_CLIENT_LOG, client_id, status);
}

void qrswork_notification_cb(uint8_t client_id, uint16_t conn_id, 
    ssapc_handle_value_t *data, errcode_t status)
{
    osal_printk("%s notification callback called! client_id:%d, conn_id:%d, status:%d\r\n",
        QRSWORK_CLIENT_LOG, client_id, conn_id, status);
        
    if (data != NULL && data->data_len > 0) {
        osal_printk("%s Heart rate data received, len:%d\r\n", QRSWORK_CLIENT_LOG, data->data_len);
        
        // 输出心跳数据（假设为整数）
        if (data->data_len == 2) {
            uint16_t heart_rate = (data->data[1] << 8) | data->data[0];
            osal_printk("Heart Rate = %d BPM\r\n", heart_rate);
#if defined(CONFIG_QRSWORK_NORTHBOUND_ENABLE)
            northbound_client_publish_heart(0, heart_rate);
#endif
        } else if (data->data_len == 4) {
            uint32_t heart_adc = (data->data[3] << 24) | (data->data[2] << 16) | 
                                 (data->data[1] << 8) | data->data[0];
            if (heart_adc > 0) {
                uint32_t heart_bpm = 60000 / heart_adc;
                osal_printk("ADC=%u, Heart Rate=%u BPM\r\n", heart_adc, heart_bpm);
#if defined(CONFIG_QRSWORK_NORTHBOUND_ENABLE)
                northbound_client_publish_heart(heart_adc, heart_bpm);
#endif
            } else {
                osal_printk("ADC=%u (No heart detected)\r\n", heart_adc);
#if defined(CONFIG_QRSWORK_NORTHBOUND_ENABLE)
                northbound_client_publish_heart(heart_adc, 0);
#endif
            }
        } else {
            // 以原始数据格式输出
            osal_printk("Raw data: ");
            for (uint16_t i = 0; i < data->data_len; i++) {
                osal_printk("0x%02x ", data->data[i]);
            }
            osal_printk("\r\n");
#if defined(CONFIG_QRSWORK_NORTHBOUND_ENABLE)
            northbound_client_publish_raw(data->data, data->data_len);
#endif
        }
    } else {
        osal_printk("%s notification data is NULL or empty\r\n", QRSWORK_CLIENT_LOG);
    }
}

void qrswork_indication_cb(uint8_t client_id, uint16_t conn_id,
    ssapc_handle_value_t *data, errcode_t status)
{
    osal_printk("%s indication callback, client_id:%d, conn_id:%d, status:%d\r\n",
        QRSWORK_CLIENT_LOG, client_id, conn_id, status);
}

static void qrswork_client_write_cfm_cb(uint8_t client_id, uint16_t conn_id,
    ssapc_write_result_t *write_result, errcode_t status)
{
    osal_printk("%s write_cfm callback, client_id:%d, conn_id:%d, handle:0x%x, type:0x%x, status:0x%x\r\n",
        QRSWORK_CLIENT_LOG, client_id, conn_id, write_result->handle, write_result->type, status);
}

errcode_t qrswork_client_init(ssapc_notification_callback notification_cb,
    ssapc_indication_callback indication_cb)
{
    g_qrswork_ssapc_cbk.exchange_info_cb = qrswork_client_exchange_info_cbk;
    g_qrswork_ssapc_cbk.find_structure_cb = qrswork_client_find_structure_cbk;
    g_qrswork_ssapc_cbk.ssapc_find_property_cbk = qrswork_client_find_property_cbk;
    g_qrswork_ssapc_cbk.find_structure_cmp_cb = qrswork_client_find_structure_cmp_cbk;
    g_qrswork_ssapc_cbk.write_cfm_cb = qrswork_client_write_cfm_cb;
    g_qrswork_ssapc_cbk.notification_cb = notification_cb;
    g_qrswork_ssapc_cbk.indication_cb = indication_cb;
    ssapc_register_callbacks(&g_qrswork_ssapc_cbk);
    
    qrswork_client_connect_cbk_register();
    qrswork_client_sample_seek_cbk_register();
    
    return ERRCODE_SLE_SUCCESS;
}
