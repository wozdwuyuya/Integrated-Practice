/**
 * qrswork client main file
 * Receives and displays heart rate data via SLE
 */
#include "common_def.h"
#include "soc_osal.h"
#include "app_init.h"
#include "pinctrl.h"
#include "uart.h"
#include "sle_low_latency.h"
#include "securec.h"
#include "qrswork_client.h"
#include "sle_device_discovery.h"
#include "sle_errcode.h"
#include "sle_connection_manager.h"
#include "sle_ssap_client.h"

#define QRSWORK_CLIENT_TASK_PRIO         28
#define QRSWORK_CLIENT_TASK_STACK_SIZE   0x1000
#define QRSWORK_CLIENT_TASK_DURATION_MS  2000
#define QRSWORK_UART_BAUDRATE            115200
#define QRSWORK_UART_TRANSFER_SIZE       512
#define QRSWORK_CLIENT_LOG               "[qrswork client main]"

static uint8_t g_app_uart_rx_buff[QRSWORK_UART_TRANSFER_SIZE] = { 0 };

static uart_buffer_config_t g_app_uart_buffer_config = {
    .rx_buffer = g_app_uart_rx_buff,
    .rx_buffer_size = QRSWORK_UART_TRANSFER_SIZE
};

static void uart_init_pin(void)
{
    if (CONFIG_QRSWORK_UART_BUS == 0) {
        uapi_pin_set_mode(CONFIG_QRSWORK_UART_TXD_PIN, PIN_MODE_1);
        uapi_pin_set_mode(CONFIG_QRSWORK_UART_RXD_PIN, PIN_MODE_1);       
    } else if (CONFIG_QRSWORK_UART_BUS == 1) {
        uapi_pin_set_mode(CONFIG_QRSWORK_UART_TXD_PIN, PIN_MODE_1);
        uapi_pin_set_mode(CONFIG_QRSWORK_UART_RXD_PIN, PIN_MODE_1);       
    }
}

static void uart_init_config(void)
{
    uart_attr_t attr = {
        .baud_rate = QRSWORK_UART_BAUDRATE,
        .data_bits = UART_DATA_BIT_8,
        .stop_bits = UART_STOP_BIT_1,
        .parity = UART_PARITY_NONE
    };

    uart_pin_config_t pin_config = {
        .tx_pin = CONFIG_QRSWORK_UART_TXD_PIN,
        .rx_pin = CONFIG_QRSWORK_UART_RXD_PIN,
        .cts_pin = PIN_NONE,
        .rts_pin = PIN_NONE
    };
    uapi_uart_deinit(CONFIG_QRSWORK_UART_BUS);
    uapi_uart_init(CONFIG_QRSWORK_UART_BUS, &pin_config, &attr, NULL, &g_app_uart_buffer_config);
}

static void *qrswork_client_task(const char *arg)
{
    unused(arg);
    
    osal_printk("%s qrswork client task start\r\n", QRSWORK_CLIENT_LOG);
    
    // 初始化 UART
    uart_init_pin();
    uart_init_config();
    
    // 初始化 SLE 客户端（注册回调）
    qrswork_client_init(qrswork_notification_cb, qrswork_indication_cb);
    
    // 启用 SLE（会触发 sle_enable_cb 回调，然后自动开始扫描）
    enable_sle();
    
    osal_printk("%s waiting for connection and heart data...\r\n", QRSWORK_CLIENT_LOG);
    
    while (1) {
        osal_msleep(QRSWORK_CLIENT_TASK_DURATION_MS);
    }
    
    return NULL;
}

static void qrswork_client_entry(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)qrswork_client_task, 0, "QrsworkClientTask",
        QRSWORK_CLIENT_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, QRSWORK_CLIENT_TASK_PRIO);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}

app_run(qrswork_client_entry);
