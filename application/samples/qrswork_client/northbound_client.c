/**
 * qrswork northbound UDP uplink.
 */

#include <string.h>
#include "common_def.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "securec.h"
#include "soc_osal.h"
#include "northbound_client.h"
#include "wifi_connect.h"

#define QRSWORK_NB_TASK_PRIO             13
#define QRSWORK_NB_TASK_STACK_SIZE       0x1800
#define QRSWORK_NB_QUEUE_LEN             12
#define QRSWORK_NB_PAYLOAD_MAX_LEN       192
#define QRSWORK_NB_RAW_HEX_BYTES         32
#define QRSWORK_NB_RETRY_DELAY_MS        5000
#define QRSWORK_NB_COMMAND_MAX_LEN       128
#define QRSWORK_NB_LOG                   "[qrswork northbound]"

#define QRSWORK_NB_MSG_DATA              0
#define QRSWORK_NB_MSG_RECONNECT         1

typedef struct {
    uint8_t type;
    uint16_t len;
    char payload[QRSWORK_NB_PAYLOAD_MAX_LEN];
} qrswork_nb_msg_t;

static unsigned long g_qrswork_nb_queue_id;
static volatile int g_qrswork_nb_queue_ready = 0;
static volatile int g_qrswork_nb_task_started = 0;
static int g_qrswork_nb_sock = -1;
static struct sockaddr_in g_qrswork_nb_server_addr;
static socklen_t g_qrswork_nb_server_addr_len = sizeof(g_qrswork_nb_server_addr);

static void northbound_client_close_socket(void)
{
    if (g_qrswork_nb_sock >= 0) {
        lwip_close(g_qrswork_nb_sock);
        g_qrswork_nb_sock = -1;
        osal_printk("%s socket closed\r\n", QRSWORK_NB_LOG);
    }
}

static int northbound_client_create_socket(void)
{
    g_qrswork_nb_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_qrswork_nb_sock < 0) {
        osal_printk("%s create UDP socket failed\r\n", QRSWORK_NB_LOG);
        return -1;
    }

    if (memset_s(&g_qrswork_nb_server_addr, sizeof(g_qrswork_nb_server_addr), 0,
        sizeof(g_qrswork_nb_server_addr)) != EOK) {
        osal_printk("%s memset server addr failed\r\n", QRSWORK_NB_LOG);
        northbound_client_close_socket();
        return -1;
    }
    g_qrswork_nb_server_addr.sin_family = AF_INET;
    g_qrswork_nb_server_addr.sin_port = htons(CONFIG_QRSWORK_SERVER_PORT);
    g_qrswork_nb_server_addr.sin_addr.s_addr = inet_addr(CONFIG_QRSWORK_SERVER_IP);
    g_qrswork_nb_server_addr_len = sizeof(g_qrswork_nb_server_addr);

    osal_printk("%s UDP target %s:%d\r\n",
        QRSWORK_NB_LOG, CONFIG_QRSWORK_SERVER_IP, CONFIG_QRSWORK_SERVER_PORT);
    return 0;
}

static void northbound_client_poll_command(void)
{
    char command[QRSWORK_NB_COMMAND_MAX_LEN] = {0};
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);
    int ret = recvfrom(g_qrswork_nb_sock, command, sizeof(command) - 1, MSG_DONTWAIT,
        (struct sockaddr *)&from_addr, &from_len);
    if (ret > 0) {
        command[ret] = '\0';
        osal_printk("%s command reserved: %s\r\n", QRSWORK_NB_LOG, command);
    }
}

static void northbound_client_send_message(const qrswork_nb_msg_t *msg)
{
    int ret = sendto(g_qrswork_nb_sock, msg->payload, msg->len, 0,
        (struct sockaddr *)&g_qrswork_nb_server_addr, g_qrswork_nb_server_addr_len);
    if (ret < 0) {
        osal_printk("%s send failed\r\n", QRSWORK_NB_LOG);
    } else {
        osal_printk("%s sent %d bytes\r\n", QRSWORK_NB_LOG, ret);
    }
    northbound_client_poll_command();
}

static void northbound_client_handle_reconnect(void)
{
    osal_printk("%s reconnect triggered\r\n", QRSWORK_NB_LOG);
    northbound_client_close_socket();

    while (northbound_client_create_socket() != 0) {
        osal_printk("%s socket recreate failed, retry\r\n", QRSWORK_NB_LOG);
        osal_msleep(QRSWORK_NB_RETRY_DELAY_MS);
    }
    osal_printk("%s reconnect success\r\n", QRSWORK_NB_LOG);
}

static void *northbound_client_task(const char *arg)
{
    UNUSED(arg);

    while (qrswork_wifi_connect(CONFIG_QRSWORK_WIFI_SSID, CONFIG_QRSWORK_WIFI_PSK) != 0) {
        osal_printk("%s WiFi connect failed, retry later\r\n", QRSWORK_NB_LOG);
        osal_msleep(QRSWORK_NB_RETRY_DELAY_MS);
    }

    while (northbound_client_create_socket() != 0) {
        osal_msleep(QRSWORK_NB_RETRY_DELAY_MS);
    }

    while (1) {
        qrswork_nb_msg_t msg = {0};
        unsigned int msg_size = sizeof(msg);
        if (osal_msg_queue_read_copy(g_qrswork_nb_queue_id, &msg, &msg_size, OSAL_MSGQ_WAIT_FOREVER) ==
            OSAL_SUCCESS) {
            if (msg.type == QRSWORK_NB_MSG_RECONNECT) {
                northbound_client_handle_reconnect();
            } else {
                northbound_client_send_message(&msg);
            }
        }
    }

    return NULL;
}

static int northbound_client_enqueue(const char *payload, uint16_t len)
{
    qrswork_nb_msg_t msg = {0};
    if ((payload == NULL) || (len == 0)) {
        return -1;
    }
    if (len >= QRSWORK_NB_PAYLOAD_MAX_LEN) {
        len = QRSWORK_NB_PAYLOAD_MAX_LEN - 1;
    }

    msg.type = QRSWORK_NB_MSG_DATA;
    msg.len = len;
    if (memcpy_s(msg.payload, sizeof(msg.payload), payload, len) != EOK) {
        return -1;
    }

    if (g_qrswork_nb_queue_ready == 0) {
        osal_printk("%s queue not ready, drop message\r\n", QRSWORK_NB_LOG);
        return -1;
    }
    if (osal_msg_queue_write_copy(g_qrswork_nb_queue_id, &msg, sizeof(msg), OSAL_MSGQ_NO_WAIT) != OSAL_SUCCESS) {
        osal_printk("%s queue full, drop message\r\n", QRSWORK_NB_LOG);
        return -1;
    }
    return 0;
}

void northbound_client_notify_wifi_disconnect(void)
{
    qrswork_nb_msg_t msg = {0};

    if (g_qrswork_nb_queue_ready == 0) {
        return;
    }

    msg.type = QRSWORK_NB_MSG_RECONNECT;
    msg.len = 0;
    if (osal_msg_queue_write_copy(g_qrswork_nb_queue_id, &msg, sizeof(msg), OSAL_MSGQ_NO_WAIT) != OSAL_SUCCESS) {
        osal_printk("%s enqueue reconnect failed\r\n", QRSWORK_NB_LOG);
    }
}

int northbound_client_start(void)
{
    osal_task *task_handle = NULL;

    if (g_qrswork_nb_task_started != 0) {
        return 0;
    }

    if (osal_msg_queue_create("qrswork_nb", QRSWORK_NB_QUEUE_LEN, &g_qrswork_nb_queue_id, 0,
        sizeof(qrswork_nb_msg_t)) != OSAL_SUCCESS) {
        osal_printk("%s queue create failed\r\n", QRSWORK_NB_LOG);
        return -1;
    }
    g_qrswork_nb_queue_ready = 1;

    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)northbound_client_task, 0, "QrsworkNorthbound",
        QRSWORK_NB_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, QRSWORK_NB_TASK_PRIO);
        osal_kfree(task_handle);
        g_qrswork_nb_task_started = 1;
    }
    osal_kthread_unlock();

    if (g_qrswork_nb_task_started == 0) {
        osal_printk("%s task create failed\r\n", QRSWORK_NB_LOG);
        return -1;
    }
    osal_printk("%s task started\r\n", QRSWORK_NB_LOG);
    return 0;
}

void northbound_client_publish_heart(uint32_t adc, uint32_t bpm)
{
    char payload[QRSWORK_NB_PAYLOAD_MAX_LEN] = {0};
    int ret = snprintf_s(payload, sizeof(payload), sizeof(payload) - 1,
        "{\"type\":\"heart\",\"adc\":%u,\"bpm\":%u,\"source\":\"qrswork_client\"}\r\n", adc, bpm);
    if (ret > 0) {
        (void)northbound_client_enqueue(payload, (uint16_t)ret);
    }
}

void northbound_client_publish_raw(const uint8_t *data, uint16_t len)
{
    char hex[(QRSWORK_NB_RAW_HEX_BYTES * 3) + 1] = {0};
    char payload[QRSWORK_NB_PAYLOAD_MAX_LEN] = {0};
    uint16_t hex_len = (len > QRSWORK_NB_RAW_HEX_BYTES) ? QRSWORK_NB_RAW_HEX_BYTES : len;
    int offset = 0;
    int ret;

    if ((data == NULL) || (len == 0)) {
        return;
    }

    for (uint16_t i = 0; i < hex_len; i++) {
        ret = snprintf_s(hex + offset, sizeof(hex) - offset, sizeof(hex) - offset - 1,
            "%02x%s", data[i], (i + 1 == hex_len) ? "" : " ");
        if (ret <= 0) {
            return;
        }
        offset += ret;
    }

    ret = snprintf_s(payload, sizeof(payload), sizeof(payload) - 1,
        "{\"type\":\"raw\",\"len\":%u,\"hex\":\"%s\",\"source\":\"qrswork_client\"}\r\n", len, hex);
    if (ret > 0) {
        (void)northbound_client_enqueue(payload, (uint16_t)ret);
    }
}
