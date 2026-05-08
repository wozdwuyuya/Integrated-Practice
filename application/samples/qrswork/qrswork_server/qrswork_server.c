/**
 * qrswork SLE server implementation
 * Sends heart rate data to connected clients
 */
#include "common_def.h"
#include "securec.h"
#include "soc_osal.h"
#include "sle_errcode.h"
#include "sle_connection_manager.h"
#include "sle_device_discovery.h"
#include "qrswork_server_adv.h"
#include "qrswork_server.h"

#define OCTET_BIT_LEN           8
#define UUID_LEN_2              2
#define UUID_INDEX              14
#define UUID_16BIT_LEN          2
#define UUID_128BIT_LEN         16

#define QRSWORK_SERVER_LOG "[qrswork server]"

/* sle server app uuid */
static uint8_t g_qrswork_uuid_app[UUID_LEN_2] = { 0x33, 0x33 };
/* server notify property value */
static char g_qrswork_property_value[OCTET_BIT_LEN] = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };
/* sle connect handle */
static uint16_t g_qrswork_conn_hdl = 0;
/* sle connected flag */
static bool g_qrswork_is_connected = false;
/* sle server handle */
static uint8_t g_qrswork_server_id = 0;
/* sle service handle */
static uint16_t g_qrswork_service_handle = 0;
/* sle ntf property handle */
static uint16_t g_qrswork_property_handle = 0;
/* sle pair handle */
static uint16_t g_qrswork_pair_hdl = 0;

static uint8_t g_qrswork_uuid_base[] = { 0x37, 0xBE, 0xA8, 0x80, 0xFC, 0x70, 0x11, 0xEA, \
    0xB7, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

uint16_t qrswork_server_is_connected(void)
{
    return g_qrswork_is_connected;
}

static void encode2byte_little(uint8_t *_ptr, uint16_t data)
{
    *(uint8_t *)((_ptr) + 1) = (uint8_t)((data) >> 0x8);
    *(uint8_t *)(_ptr) = (uint8_t)(data);
}

static void qrswork_uuid_set_base(sle_uuid_t *out)
{
    errcode_t ret;
    ret = memcpy_s(out->uuid, SLE_UUID_LEN, g_qrswork_uuid_base, SLE_UUID_LEN);
    if (ret != EOK) {
        osal_printk("%s uuid_set_base memcpy fail\r\n", QRSWORK_SERVER_LOG);
        out->len = 0;
        return;
    }
    out->len = UUID_LEN_2;
}

static void qrswork_uuid_setu2(uint16_t u2, sle_uuid_t *out)
{
    qrswork_uuid_set_base(out);
    out->len = UUID_LEN_2;
    encode2byte_little(&out->uuid[UUID_INDEX], u2);
}

static void ssaps_mtu_changed_cbk(uint8_t server_id, uint16_t conn_id, ssap_exchange_info_t *mtu_size,
    errcode_t status)
{
    osal_printk("%s mtu_changed callback server_id:%x, conn_id:%x, mtu_size:%x, status:%x\r\n",
        QRSWORK_SERVER_LOG, server_id, conn_id, mtu_size->mtu_size, status);
    if (g_qrswork_pair_hdl == 0) {
        g_qrswork_pair_hdl = conn_id + 1;
    }
}

static void ssaps_start_service_cbk(uint8_t server_id, uint16_t handle, errcode_t status)
{
    osal_printk("%s start service callback server_id:%d, handle:%x, status:%x\r\n", 
        QRSWORK_SERVER_LOG, server_id, handle, status);
}

static void ssaps_add_service_cbk(uint8_t server_id, sle_uuid_t *uuid, uint16_t handle, errcode_t status)
{
    osal_printk("%s add service callback server_id:%x, handle:%x, status:%x\r\n", 
        QRSWORK_SERVER_LOG, server_id, handle, status);
}

static void ssaps_add_property_cbk(uint8_t server_id, sle_uuid_t *uuid, uint16_t service_handle,
    uint16_t handle, errcode_t status)
{
    osal_printk("%s add property callback server_id:%x, service_handle:%x, handle:%x, status:%x\r\n",
        QRSWORK_SERVER_LOG, server_id, service_handle, handle, status);
}

static void ssaps_add_descriptor_cbk(uint8_t server_id, sle_uuid_t *uuid, uint16_t service_handle,
    uint16_t property_handle, errcode_t status)
{
    osal_printk("%s add descriptor callback server_id:%x, service_handle:%x, property_handle:%x, status:%x\r\n", 
        QRSWORK_SERVER_LOG, server_id, service_handle, property_handle, status);
}

static void ssaps_delete_all_service_cbk(uint8_t server_id, errcode_t status)
{
    osal_printk("%s delete all service callback server_id:%x, status:%x\r\n", 
        QRSWORK_SERVER_LOG, server_id, status);
}

static errcode_t qrswork_ssaps_register_cbks(ssaps_read_request_callback ssaps_read_callback, 
    ssaps_write_request_callback ssaps_write_callback)
{
    errcode_t ret;
    ssaps_callbacks_t ssaps_cbk = {0};
    ssaps_cbk.add_service_cb = ssaps_add_service_cbk;
    ssaps_cbk.add_property_cb = ssaps_add_property_cbk;
    ssaps_cbk.add_descriptor_cb = ssaps_add_descriptor_cbk;
    ssaps_cbk.start_service_cb = ssaps_start_service_cbk;
    ssaps_cbk.delete_all_service_cb = ssaps_delete_all_service_cbk;
    ssaps_cbk.mtu_changed_cb = ssaps_mtu_changed_cbk;
    ssaps_cbk.read_request_cb = ssaps_read_callback;
    ssaps_cbk.write_request_cb = ssaps_write_callback;
    ret = ssaps_register_callbacks(&ssaps_cbk);
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("%s register callbacks fail :%x\r\n", QRSWORK_SERVER_LOG, ret);
        return ret;
    }
    return ERRCODE_SLE_SUCCESS;
}

static errcode_t qrswork_server_service_add(void)
{
    errcode_t ret;
    sle_uuid_t service_uuid = {0};
    qrswork_uuid_setu2(SLE_UUID_HEART_SERVICE, &service_uuid);
    ret = ssaps_add_service_sync(g_qrswork_server_id, &service_uuid, 1, &g_qrswork_service_handle);
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("%s add service fail, ret:%x\r\n", QRSWORK_SERVER_LOG, ret);
        return ERRCODE_SLE_FAIL;
    }
    return ERRCODE_SLE_SUCCESS;
}

static errcode_t qrswork_server_property_add(void)
{
    errcode_t ret;
    ssaps_property_info_t property = {0};
    ssaps_desc_info_t descriptor = {0};
    uint8_t ntf_value[] = {0x01, 0x0};

    property.permissions = SLE_UUID_HEART_PROPERTIES;
    property.operate_indication = SSAP_OPERATE_INDICATION_BIT_READ | SSAP_OPERATE_INDICATION_BIT_NOTIFY;
    qrswork_uuid_setu2(SLE_UUID_HEART_NTF_REPORT, &property.uuid);
    property.value = (uint8_t *)osal_vmalloc(sizeof(g_qrswork_property_value));
    if (property.value == NULL) {
        return ERRCODE_SLE_FAIL;
    }
    if (memcpy_s(property.value, sizeof(g_qrswork_property_value), g_qrswork_property_value,
        sizeof(g_qrswork_property_value)) != EOK) {
        osal_vfree(property.value);
        return ERRCODE_SLE_FAIL;
    }
    ret = ssaps_add_property_sync(g_qrswork_server_id, g_qrswork_service_handle, &property, &g_qrswork_property_handle);
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("%s add property fail, ret:%x\r\n", QRSWORK_SERVER_LOG, ret);
        osal_vfree(property.value);
        return ERRCODE_SLE_FAIL;
    }

    descriptor.permissions = SLE_UUID_HEART_DESCRIPTOR;
    qrswork_uuid_setu2(0x2902, &descriptor.uuid);
    descriptor.value = osal_vmalloc(sizeof(ntf_value));
    if (descriptor.value == NULL) {
        osal_vfree(property.value);
        return ERRCODE_SLE_FAIL;
    }
    if (memcpy_s(descriptor.value, sizeof(ntf_value), ntf_value, sizeof(ntf_value)) != EOK) {
        osal_vfree(property.value);
        osal_vfree(descriptor.value);
        return ERRCODE_SLE_FAIL;
    }
    descriptor.value_len = sizeof(ntf_value);
    ret = ssaps_add_descriptor_sync(g_qrswork_server_id, g_qrswork_service_handle, g_qrswork_property_handle,
        &descriptor);
    osal_vfree(property.value);
    osal_vfree(descriptor.value);
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("%s add descriptor fail, ret:%x\r\n", QRSWORK_SERVER_LOG, ret);
        return ERRCODE_SLE_FAIL;
    }
    return ERRCODE_SLE_SUCCESS;
}

static errcode_t qrswork_server_service_start(void)
{
    errcode_t ret = ssaps_start_service(g_qrswork_server_id, g_qrswork_service_handle);
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("%s start service fail, ret:%x\r\n", QRSWORK_SERVER_LOG, ret);
        return ERRCODE_SLE_FAIL;
    }
    return ERRCODE_SLE_SUCCESS;
}

errcode_t qrswork_server_send_heart_data(const uint8_t *data, uint16_t len)
{
    ssaps_ntf_ind_t param = {0};
    param.handle = g_qrswork_property_handle;
    param.type = SSAP_PROPERTY_TYPE_VALUE;
    param.value = (uint8_t *)data;
    param.value_len = len;
    
    if (!g_qrswork_is_connected) {
        return ERRCODE_SLE_FAIL;
    }
    
    return ssaps_notify_indicate(g_qrswork_server_id, g_qrswork_conn_hdl, &param);
}

static void qrswork_server_connect_state_changed_cbk(uint16_t conn_id, const sle_addr_t *addr,
    sle_acb_state_t conn_state, sle_pair_state_t pair_state, sle_disc_reason_t disc_reason)
{
    osal_printk("%s conn state changed conn_id:0x%x, conn_state:%d, pair_state:%d, disc_reason:%x\r\n",
        QRSWORK_SERVER_LOG, conn_id, conn_state, pair_state, disc_reason);
    g_qrswork_conn_hdl = conn_id;
    if (conn_state == SLE_ACB_STATE_CONNECTED) {
        osal_printk("%s SLE connected\r\n", QRSWORK_SERVER_LOG);
        g_qrswork_is_connected = true;
    } else if (conn_state == SLE_ACB_STATE_DISCONNECTED) {
        osal_printk("%s SLE disconnected\r\n", QRSWORK_SERVER_LOG);
        g_qrswork_is_connected = false;
        g_qrswork_conn_hdl = 0;
        qrswork_server_adv_init();
    }
}

static void qrswork_server_connect_cbk_register(void)
{
    sle_connection_callbacks_t conn_cbk = {0};
    conn_cbk.connect_state_changed_cb = qrswork_server_connect_state_changed_cbk;
    sle_connection_register_callbacks(&conn_cbk);
}

errcode_t qrswork_server_init(ssaps_read_request_callback ssaps_read_callback,
    ssaps_write_request_callback ssaps_write_callback)
{
    errcode_t ret;
    
    osal_printk("%s init start\r\n", QRSWORK_SERVER_LOG);
    
    enable_sle();
    qrswork_server_connect_cbk_register();
    
    ret = qrswork_ssaps_register_cbks(ssaps_read_callback, ssaps_write_callback);
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("%s register callbacks fail\r\n", QRSWORK_SERVER_LOG);
        return ret;
    }
    
    sle_uuid_t app_uuid = {0};
    qrswork_uuid_setu2(0x3333, &app_uuid);
    ssaps_register_server(&app_uuid, &g_qrswork_server_id);
    
    ret = qrswork_server_service_add();
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("%s add service fail\r\n", QRSWORK_SERVER_LOG);
        return ret;
    }
    
    ret = qrswork_server_property_add();
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("%s add property fail\r\n", QRSWORK_SERVER_LOG);
        return ret;
    }
    
    ret = qrswork_server_service_start();
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("%s start service fail\r\n", QRSWORK_SERVER_LOG);
        return ret;
    }
    
    qrswork_server_adv_init();
    
    osal_printk("%s init success\r\n", QRSWORK_SERVER_LOG);
    return ERRCODE_SLE_SUCCESS;
}
