/**
 * @file sle_comm.c
 * @brief 星闪SLE无线通信模块实现
 * @note [四柱-问答] SLE 通信模块（条件编译保护）
 * @note 基于星闪NearLink SLE协议，实现采集端与接收端数据传输
 * @note 未定义 FEATURE_SLE 时退化为空实现，不影响其他模块编译
 */

#include "sle_comm.h"

// ==================== 条件编译：有SLE SDK时使用完整实现 ====================
#ifdef FEATURE_SLE

#include "sle_ssap_server.h"
#include "sle_connection_manager.h"
#include "sle_device_discovery.h"
#include "errcode.h"
#include "soc_osal.h"
#include "osal_debug.h"

// SLE UUID定义
#define SLE_UUID_SERVER_SERVICE        0x2222  // 服务UUID
#define SLE_UUID_SERVER_NTF_REPORT     0x2323  // 通知属性UUID

// 广播句柄
#define SLE_ADV_HANDLE_DEFAULT         1

// 状态变量
static sle_state_t g_sle_state = SLE_STATE_IDLE;
static uint16_t g_sle_conn_hdl = 0;
static uint8_t g_server_id = 0;
static uint16_t g_service_handle = 0;
static uint16_t g_property_handle = 0;
static uint16_t g_sle_pair_hdl = 0;

// 回调函数
static sle_data_callback_t g_data_callback = NULL;

// UUID基址
static uint8_t g_sle_uuid_base[] = {
    0x37, 0xBE, 0xA8, 0x80, 0xFC, 0x70, 0x11, 0xEA,
    0xB7, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// UUID应用标识
static char g_sle_uuid_app_uuid[2] = { 0x12, 0x34 };

// 属性值
static char g_sle_property_value[6] = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };

// 编码小端数据
static void encode2byte_little(uint8_t *ptr, uint16_t data){
    *(ptr + 1) = (uint8_t)(data >> 8);
    *ptr = (uint8_t)(data);
}

// 设置UUID基址
static void sle_uuid_set_base(sle_uuid_t *out){
    if(out == NULL) return;

    memcpy(out->uuid, g_sle_uuid_base, SLE_UUID_LEN);
    out->len = 2;
}

// 设置UUID
static void sle_uuid_setu2(uint16_t u2, sle_uuid_t *out){
    sle_uuid_set_base(out);
    out->len = 2;
    encode2byte_little(&out->uuid[14], u2);
}

// MTU变化回调
static void ssaps_mtu_changed_cbk(uint8_t server_id, uint16_t conn_id,
    ssap_exchange_info_t *mtu_size, errcode_t status){
    osal_printk("[SLE] MTU changed: conn_id=0x%x, mtu=%x, status=%x\r\n",
           conn_id, mtu_size->mtu_size, status);

    if(g_sle_pair_hdl == 0) {
        g_sle_pair_hdl = conn_id + 1;
    }
}

// 服务启动回调
static void ssaps_start_service_cbk(uint8_t server_id, uint16_t handle, errcode_t status){
    osal_printk("[SLE] Service started: server_id=%d, handle=%x, status=%x\r\n",
           server_id, handle, status);
}

// 添加服务回调
static void ssaps_add_service_cbk(uint8_t server_id, sle_uuid_t *uuid,
    uint16_t handle, errcode_t status){
    osal_printk("[SLE] Service added: server_id=%x, handle=%x, status=%x\r\n",
           server_id, handle, status);
}

// 添加属性回调
static void ssaps_add_property_cbk(uint8_t server_id, sle_uuid_t *uuid,
    uint16_t service_handle, uint16_t handle, errcode_t status){
    osal_printk("[SLE] Property added: server_id=%x, service=%x, handle=%x, status=%x\r\n",
           server_id, service_handle, handle, status);
}

// 添加描述符回调
static void ssaps_add_descriptor_cbk(uint8_t server_id, sle_uuid_t *uuid,
    uint16_t service_handle, uint16_t property_handle, errcode_t status){
    osal_printk("[SLE] Descriptor added: status=%x\r\n", status);
}

// 删除服务回调
static void ssaps_delete_all_service_cbk(uint8_t server_id, errcode_t status){
    osal_printk("[SLE] All services deleted: server_id=%x, status=%x\r\n",
           server_id, status);
}

// 写请求回调
static void ssaps_write_request_cbk(uint8_t server_id, uint16_t conn_id,
    uint16_t handle, uint16_t length, uint8_t *value){
    osal_printk("[SLE] Write request: handle=%x, len=%d\r\n", handle, length);

    // 调用用户注册的回调
    if(g_data_callback != NULL) {
        g_data_callback(value, length);
    }
}

// 读请求回调
static void ssaps_read_request_cbk(uint8_t server_id, uint16_t conn_id,
    uint16_t handle){
    osal_printk("[SLE] Read request: handle=%x\r\n", handle);
}

// 注册SLE回调
static errcode_t sle_ssaps_register_cbks(void){
    errcode_t ret;
    ssaps_callbacks_t ssaps_cbk = {0};

    ssaps_cbk.add_service_cb = ssaps_add_service_cbk;
    ssaps_cbk.add_property_cb = ssaps_add_property_cbk;
    ssaps_cbk.add_descriptor_cb = ssaps_add_descriptor_cbk;
    ssaps_cbk.start_service_cb = ssaps_start_service_cbk;
    ssaps_cbk.delete_all_service_cb = ssaps_delete_all_service_cbk;
    ssaps_cbk.mtu_changed_cb = ssaps_mtu_changed_cbk;
    ssaps_cbk.read_request_cb = ssaps_read_request_cbk;
    ssaps_cbk.write_request_cb = ssaps_write_request_cbk;

    ret = ssaps_register_callbacks(&ssaps_cbk);
    if(ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[SLE] Register callbacks failed: %x\r\n", ret);
        return ret;
    }
    return ERRCODE_SLE_SUCCESS;
}

// 添加服务
static errcode_t sle_uuid_server_service_add(void){
    errcode_t ret;
    sle_uuid_t service_uuid = {0};

    sle_uuid_setu2(SLE_UUID_SERVER_SERVICE, &service_uuid);
    ret = ssaps_add_service_sync(g_server_id, &service_uuid, 1, &g_service_handle);
    if(ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[SLE] Add service failed: %x\r\n", ret);
        return ERRCODE_SLE_FAIL;
    }
    return ERRCODE_SLE_SUCCESS;
}

// 添加属性
static errcode_t sle_uuid_server_property_add(void){
    errcode_t ret;
    ssaps_property_info_t property = {0};
    ssaps_desc_info_t descriptor = {0};
    uint8_t ntf_value[] = {0x01, 0x0};

    property.permissions = SLE_UUID_TEST_PROPERTIES;
    property.operate_indication = SSAP_OPERATE_INDICATION_BIT_READ | SSAP_OPERATE_INDICATION_BIT_NOTIFY;
    sle_uuid_setu2(SLE_UUID_SERVER_NTF_REPORT, &property.uuid);

    property.value = (uint8_t *)osal_vmalloc(sizeof(g_sle_property_value));
    if(property.value == NULL) {
        return ERRCODE_SLE_FAIL;
    }
    memcpy(property.value, g_sle_property_value, sizeof(g_sle_property_value));

    ret = ssaps_add_property_sync(g_server_id, g_service_handle, &property, &g_property_handle);
    if(ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[SLE] Add property failed: %x\r\n", ret);
        osal_vfree(property.value);
        return ERRCODE_SLE_FAIL;
    }

    descriptor.permissions = SLE_UUID_TEST_DESCRIPTOR;
    descriptor.type = SSAP_DESCRIPTOR_USER_DESCRIPTION;
    descriptor.operate_indication = SSAP_OPERATE_INDICATION_BIT_READ | SSAP_OPERATE_INDICATION_BIT_WRITE;
    descriptor.value = ntf_value;
    descriptor.value_len = sizeof(ntf_value);

    ret = ssaps_add_descriptor_sync(g_server_id, g_service_handle, g_property_handle, &descriptor);
    if(ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[SLE] Add descriptor failed: %x\r\n", ret);
        osal_vfree(property.value);
        return ERRCODE_SLE_FAIL;
    }

    osal_vfree(property.value);
    return ERRCODE_SLE_SUCCESS;
}

// 添加SLE服务
static errcode_t sle_uart_server_add(void){
    errcode_t ret;
    sle_uuid_t app_uuid = {0};

    osal_printk("[SLE] Adding server...\r\n");

    app_uuid.len = sizeof(g_sle_uuid_app_uuid);
    memcpy(app_uuid.uuid, g_sle_uuid_app_uuid, sizeof(g_sle_uuid_app_uuid));

    ssaps_register_server(&app_uuid, &g_server_id);

    if(sle_uuid_server_service_add() != ERRCODE_SLE_SUCCESS) {
        ssaps_unregister_server(g_server_id);
        return ERRCODE_SLE_FAIL;
    }

    if(sle_uuid_server_property_add() != ERRCODE_SLE_SUCCESS) {
        ssaps_unregister_server(g_server_id);
        return ERRCODE_SLE_FAIL;
    }

    osal_printk("[SLE] Server added: server_id=%x, service=%x, property=%x\r\n",
           g_server_id, g_service_handle, g_property_handle);

    ret = ssaps_start_service(g_server_id, g_service_handle);
    if(ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[SLE] Start service failed: %x\r\n", ret);
        return ERRCODE_SLE_FAIL;
    }

    osal_printk("[SLE] Server init complete\r\n");
    return ERRCODE_SLE_SUCCESS;
}

// 连接状态变化回调
static void sle_connect_state_changed_cbk(uint16_t conn_id, const sle_addr_t *addr,
    sle_acb_state_t conn_state, sle_pair_state_t pair_state, sle_disc_reason_t disc_reason){
    osal_printk("[SLE] Connect state changed: conn_id=0x%x, state=%x, pair=%x, reason=%x\r\n",
           conn_id, conn_state, pair_state, disc_reason);

    if(conn_state == SLE_ACB_STATE_CONNECTED) {
        g_sle_conn_hdl = conn_id;
        g_sle_state = SLE_STATE_CONNECTED;
    } else if(conn_state == SLE_ACB_STATE_DISCONNECTED) {
        g_sle_conn_hdl = 0;
        g_sle_pair_hdl = 0;
        g_sle_state = SLE_STATE_IDLE;
    }
}

// 配对完成回调
static void sle_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status){
    osal_printk("[SLE] Pair complete: conn_id=%x, status=%x\r\n", conn_id, status);

    g_sle_pair_hdl = conn_id + 1;
    g_sle_state = SLE_STATE_PAIRED;

    // 设置MTU
    ssap_exchange_info_t parameter = {0};
    parameter.mtu_size = 520;
    parameter.version = 1;
    ssaps_set_info(g_server_id, &parameter);
}

// 注册连接回调
static errcode_t sle_conn_register_cbks(void){
    errcode_t ret;
    sle_connection_callbacks_t conn_cbks = {0};

    conn_cbks.connect_state_changed_cb = sle_connect_state_changed_cbk;
    conn_cbks.pair_complete_cb = sle_pair_complete_cbk;

    ret = sle_connection_register_callbacks(&conn_cbks);
    if(ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[SLE] Register connection callbacks failed: %x\r\n", ret);
        return ret;
    }
    return ERRCODE_SLE_SUCCESS;
}

// SLE通信初始化
bool sle_comm_init(void){
    errcode_t ret;

    osal_printk("[SLE] Initializing...\r\n");

    // 使能SLE
    if(enable_sle() != ERRCODE_SUCC) {
        osal_printk("[SLE] Enable SLE failed!\r\n");
        return false;
    }

    // 注册广播回调
    ret = sle_uart_announce_register_cbks();
    if(ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[SLE] Register announce callbacks failed: %x\r\n", ret);
        return false;
    }

    // 注册连接回调
    ret = sle_conn_register_cbks();
    if(ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[SLE] Register connection callbacks failed: %x\r\n", ret);
        return false;
    }

    // 注册服务回调
    ret = sle_ssaps_register_cbks();
    if(ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[SLE] Register service callbacks failed: %x\r\n", ret);
        return false;
    }

    // 添加服务
    ret = sle_uart_server_add();
    if(ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[SLE] Add server failed: %x\r\n", ret);
        return false;
    }

    g_sle_state = SLE_STATE_IDLE;
    osal_printk("[SLE] Init success\r\n");
    return true;
}

// 启动SLE广播
bool sle_comm_start_adv(void){
    errcode_t ret;

    ret = sle_uart_server_adv_init();
    if(ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[SLE] Start advertising failed: %x\r\n", ret);
        return false;
    }

    g_sle_state = SLE_STATE_ADV;
    osal_printk("[SLE] Advertising started\r\n");
    return true;
}

// 停止SLE广播
void sle_comm_stop_adv(void){
    // 停止广播
    g_sle_state = SLE_STATE_IDLE;
}

// 发送数据到对端
bool sle_comm_send_data(const uint8_t *data, uint16_t len){
    if(!sle_comm_is_connected()) {
        osal_printk("[SLE] Not connected\r\n");
        return false;
    }

    errcode_t ret;
    ssaps_ntf_ind_by_uuid_t param = {0};

    param.type = SSAP_PROPERTY_TYPE_VALUE;
    param.start_handle = g_service_handle;
    param.end_handle = g_property_handle;
    param.value_len = len;
    param.value = (uint8_t *)osal_vmalloc(len);

    if(param.value == NULL) {
        osal_printk("[SLE] Allocate memory failed\r\n");
        return false;
    }

    memcpy(param.value, data, len);
    sle_uuid_setu2(SLE_UUID_SERVER_NTF_REPORT, &param.uuid);

    ret = ssaps_notify_indicate_by_uuid(g_server_id, g_sle_conn_hdl, &param);
    if(ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[SLE] Send data failed: %x\r\n", ret);
        osal_vfree(param.value);
        return false;
    }

    osal_vfree(param.value);
    return true;
}

// 发送JSON格式数据
bool sle_comm_send_json(const char *json_str){
    if(json_str == NULL) return false;
    return sle_comm_send_data((const uint8_t *)json_str, strlen(json_str));
}

// 注册数据接收回调
void sle_comm_register_callback(sle_data_callback_t callback){
    g_data_callback = callback;
}

// 获取SLE连接状态
sle_state_t sle_comm_get_state(void){
    return g_sle_state;
}

// 检查是否已连接
bool sle_comm_is_connected(void){
    return (g_sle_pair_hdl != 0);
}

// 获取连接句柄
uint16_t sle_comm_get_conn_id(void){
    return g_sle_conn_hdl;
}

// ==================== 条件编译：无SLE SDK时退化为空实现 ====================
#else  // !FEATURE_SLE

static sle_state_t g_sle_state = SLE_STATE_IDLE;
static sle_data_callback_t g_data_callback = NULL;

bool sle_comm_init(void){
    osal_printk("[SLE] Disabled (no SDK), stub mode\r\n");
    g_sle_state = SLE_STATE_IDLE;
    return true;
}

bool sle_comm_start_adv(void){
    osal_printk("[SLE] Adv stub (no SDK)\r\n");
    return true;
}

void sle_comm_stop_adv(void){}

bool sle_comm_send_data(const uint8_t *data, uint16_t len){
    (void)data; (void)len;
    return false;
}

bool sle_comm_send_json(const char *json_str){
    (void)json_str;
    return false;
}

void sle_comm_register_callback(sle_data_callback_t callback){
    g_data_callback = callback;
}

sle_state_t sle_comm_get_state(void){
    return g_sle_state;
}

bool sle_comm_is_connected(void){
    return false;
}

uint16_t sle_comm_get_conn_id(void){
    return 0;
}

#endif  // FEATURE_SLE
