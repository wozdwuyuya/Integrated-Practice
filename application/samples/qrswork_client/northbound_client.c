/**
 * qrswork northbound UDP uplink.
 *
 * 功能概述：
 *   北向传输模块，负责将 qrswork_client 通过 SLE 收到的心率/原始数据
 *   封装成 JSON 格式，通过 WiFi UDP 发送到电脑或上位服务器。
 *
 * 架构说明：
 *   SLE 回调 --> 消息队列 --> 北向任务（本文件）--> UDP socket --> 电脑
 *   设计上 SLE 回调只负责入队，实际网络发送由独立的 QrsworkNorthbound 任务完成，
 *   避免在中断/回调上下文中执行耗时的网络操作。
 */

#include <string.h>
#include "common_def.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "securec.h"
#include "soc_osal.h"
#include "northbound_client.h"
#include "wifi_connect.h"

/* ==========================================================================
 * 宏定义
 * ========================================================================== */

// [AI 优化] 原教程 (udp_client.c) 任务优先级为 13 (osPriority_t)。
// 海思 LiteOS 中数值越大优先级越高。原代码误设为 27，会抢占 SLE 回调和系统任务，
// 现修正为 13，与 SDK 官方范例对齐。
#define QRSWORK_NB_TASK_PRIO             13

// 任务栈大小 0x1800 = 6KB，足够 UDP 发送和 JSON 序列化使用
#define QRSWORK_NB_TASK_STACK_SIZE       0x1800

// 消息队列深度：最多缓存 12 条待发送消息，队列满时丢弃新消息
#define QRSWORK_NB_QUEUE_LEN             12

// 单条消息最大负载长度（字节），JSON 心率数据约 80 字节，留足余量
#define QRSWORK_NB_PAYLOAD_MAX_LEN       192

// publish_raw 时最多展示的原始数据字节数（十六进制摘要）
#define QRSWORK_NB_RAW_HEX_BYTES         32

// WiFi 连接/socket 创建失败后的重试间隔（毫秒）
#define QRSWORK_NB_RETRY_DELAY_MS        5000

// 从电脑端接收 UDP 命令的缓冲区大小（预留扩展用）
#define QRSWORK_NB_COMMAND_MAX_LEN       128

// 日志前缀，方便在串口日志中过滤北向模块输出
#define QRSWORK_NB_LOG                   "[qrswork northbound]"

// [AI 优化] 消息类型标识。原代码没有区分消息类型，WiFi 断线后无法通知北向任务
// 重建 socket。新增 MSG_RECONNECT 类型，由 WiFi 断线回调通过消息队列触发重连。
#define QRSWORK_NB_MSG_DATA              0   // 正常数据消息
#define QRSWORK_NB_MSG_RECONNECT         1   // WiFi 断线重连信号

/* ==========================================================================
 * 消息结构体
 * ========================================================================== */

// [AI 优化] 在原结构体基础上新增 type 字段，用于区分数据消息和重连信号。
// 原教程逻辑位置：原结构体只有 len + payload，无类型区分。
typedef struct {
    uint8_t type;                          // 消息类型：MSG_DATA 或 MSG_RECONNECT
    uint16_t len;                          // payload 有效数据长度
    char payload[QRSWORK_NB_PAYLOAD_MAX_LEN]; // JSON 数据负载
} qrswork_nb_msg_t;

/* ==========================================================================
 * 模块全局变量
 * ========================================================================== */

static unsigned long g_qrswork_nb_queue_id;   // OSAL 消息队列 ID

// [AI 优化] 添加 volatile 修饰。这两个变量在 northbound_client_start()（主任务上下文）
// 中写入，在 northbound_client_task（内核线程上下文）中读取。若不加 volatile，
// 编译器优化可能缓存旧值，导致任务启动后仍认为队列未就绪。
static volatile int g_qrswork_nb_queue_ready = 0;
static volatile int g_qrswork_nb_task_started = 0;

static int g_qrswork_nb_sock = -1;            // UDP socket 文件描述符，-1 表示未创建
static struct sockaddr_in g_qrswork_nb_server_addr;   // 目标服务器地址结构
static socklen_t g_qrswork_nb_server_addr_len = sizeof(g_qrswork_nb_server_addr);

/* ==========================================================================
 * Socket 管理
 * ========================================================================== */

/**
 * @brief 关闭 UDP socket 并重置描述符
 * @note  [AI 优化] 原代码没有关闭 socket 的函数，WiFi 断线后旧 socket 成为悬空描述符。
 *        新增此函数供重连流程调用，确保资源不泄漏。
 */
static void northbound_client_close_socket(void)
{
    if (g_qrswork_nb_sock >= 0) {
        lwip_close(g_qrswork_nb_sock);
        g_qrswork_nb_sock = -1;
        osal_printk("%s socket closed\r\n", QRSWORK_NB_LOG);
    }
}

/**
 * @brief 创建 UDP socket 并配置目标服务器地址
 * @return 0 成功，-1 失败
 *
 * @note  [AI 优化] 原代码中 memset_s 失败时不会关闭已打开的 socket，导致文件描述符泄漏。
 *        现在任何步骤失败都会调用 northbound_client_close_socket() 回滚。
 *        参考：SDK 官方 udp_client.c 中 socket 创建后未做防御性关闭（SDK 范例较简单）。
 */
static int northbound_client_create_socket(void)
{
    // 创建 UDP 数据报 socket
    g_qrswork_nb_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_qrswork_nb_sock < 0) {
        osal_printk("%s create UDP socket failed\r\n", QRSWORK_NB_LOG);
        return -1;
    }

    // 清零服务器地址结构体，然后填入目标 IP 和端口
    if (memset_s(&g_qrswork_nb_server_addr, sizeof(g_qrswork_nb_server_addr), 0,
        sizeof(g_qrswork_nb_server_addr)) != EOK) {
        osal_printk("%s memset server addr failed\r\n", QRSWORK_NB_LOG);
        northbound_client_close_socket();  // [AI 优化] 失败时关闭 socket，防止 fd 泄漏
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

/* ==========================================================================
 * 数据收发
 * ========================================================================== */

/**
 * @brief 非阻塞轮询电脑端发来的 UDP 命令（预留扩展）
 * @note  当前仅打印收到的内容，后续可扩展为控制协议解析。
 *        MSG_DONTWAIT 使 recvfrom 立即返回，无数据时返回 -1（errno = EAGAIN），属正常情况。
 */
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
    // [AI 优化] ret == -1 时无需处理，MSG_DONTWAIT 模式下无数据即返回 -1，属于正常行为
}

/**
 * @brief 通过 UDP 发送一条消息到电脑端
 * @param msg 指向消息结构体的指针（type 必须为 MSG_DATA）
 */
static void northbound_client_send_message(const qrswork_nb_msg_t *msg)
{
    int ret = sendto(g_qrswork_nb_sock, msg->payload, msg->len, 0,
        (struct sockaddr *)&g_qrswork_nb_server_addr, g_qrswork_nb_server_addr_len);
    if (ret < 0) {
        osal_printk("%s send failed\r\n", QRSWORK_NB_LOG);
    } else {
        osal_printk("%s sent %d bytes\r\n", QRSWORK_NB_LOG, ret);
    }
    // 每次发送后顺便检查是否有电脑端发来的命令
    northbound_client_poll_command();
}

/* ==========================================================================
 * WiFi 断线重连处理
 * ========================================================================== */

/**
 * @brief 处理 WiFi 断线后的 socket 重建
 * @note  [AI 优化] 原代码没有断线重连机制。WiFi 断开后，旧 socket 持续 sendto 失败但无法恢复。
 *        现在的流程：
 *          1. WiFi 回调检测到断线 → 通过消息队列发送 MSG_RECONNECT
 *          2. 北向任务收到 MSG_RECONNECT → 调用本函数
 *          3. 关闭旧 socket → 重新创建新 socket
 *        整个过程在北向任务上下文中完成，不涉及中断/回调中的网络操作。
 */
static void northbound_client_handle_reconnect(void)
{
    osal_printk("%s reconnect triggered\r\n", QRSWORK_NB_LOG);
    northbound_client_close_socket();

    // 循环重试直到 socket 创建成功
    while (northbound_client_create_socket() != 0) {
        osal_printk("%s socket recreate failed, retry\r\n", QRSWORK_NB_LOG);
        osal_msleep(QRSWORK_NB_RETRY_DELAY_MS);
    }
    osal_printk("%s reconnect success\r\n", QRSWORK_NB_LOG);
}

/* ==========================================================================
 * 北向传输主任务
 * ========================================================================== */

/**
 * @brief 北向传输主任务入口
 * @note  执行流程：
 *   1. 阻塞等待 WiFi 连接成功
 *   2. 创建 UDP socket
 *   3. 进入消息循环：从队列读取消息并处理
 *      - MSG_DATA: 通过 UDP 发送到电脑
 *      - MSG_RECONNECT: 关闭旧 socket 并重建
 *
 *   [AI 优化] 原代码任务函数中使用 unused(arg) 小写宏，SDK 官方范例使用 UNUSED(param) 大写。
 *   海思 SDK 的 common_def.h 通常定义大写 UNUSED 宏，小写可能未定义导致编译警告。
 *   已修正为 UNUSED(arg)。
 */
static void *northbound_client_task(const char *arg)
{
    UNUSED(arg);

    // 第一步：阻塞等待 WiFi 连接（失败则每 5 秒重试）
    while (qrswork_wifi_connect(CONFIG_QRSWORK_WIFI_SSID, CONFIG_QRSWORK_WIFI_PSK) != 0) {
        osal_printk("%s WiFi connect failed, retry later\r\n", QRSWORK_NB_LOG);
        osal_msleep(QRSWORK_NB_RETRY_DELAY_MS);
    }

    // 第二步：创建 UDP socket（失败则每 5 秒重试）
    while (northbound_client_create_socket() != 0) {
        osal_msleep(QRSWORK_NB_RETRY_DELAY_MS);
    }

    // 第三步：消息主循环
    while (1) {
        qrswork_nb_msg_t msg = {0};
        unsigned int msg_size = sizeof(msg);
        if (osal_msg_queue_read_copy(g_qrswork_nb_queue_id, &msg, &msg_size, OSAL_MSGQ_WAIT_FOREVER) ==
            OSAL_SUCCESS) {
            // [AI 优化] 根据消息类型分发处理。原代码没有此分支，所有消息都直接发送。
            if (msg.type == QRSWORK_NB_MSG_RECONNECT) {
                northbound_client_handle_reconnect();
            } else {
                northbound_client_send_message(&msg);
            }
        }
    }

    return NULL;
}

/* ==========================================================================
 * 消息入队接口（供 SLE 回调调用）
 * ========================================================================== */

/**
 * @brief 将 JSON 数据入队到北向消息队列
 * @param payload JSON 字符串指针
 * @param len     JSON 字符串长度
 * @return 0 成功入队，-1 失败（参数无效/队列满/队列未就绪）
 *
 * @note  此函数设计为可在 SLE 回调上下文中安全调用：
 *   - 使用 OSAL_MSGQ_NO_WAIT 非阻塞写入，不会挂起
 *   - 队列满时直接丢弃并打印日志，不阻塞 SLE 回调
 *   - len 超限时自动截断，防止缓冲区溢出
 */
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

/* ==========================================================================
 * 对外接口
 * ========================================================================== */

/**
 * @brief 通知北向任务 WiFi 已断线，需要重建 socket
 * @note  [AI 优化] 原代码没有此接口。WiFi 断线后北向任务无法感知，持续在旧 socket 上发送失败。
 *        此函数由 wifi_connect.c 中的 WiFi 断线回调调用，通过消息队列发送 MSG_RECONNECT 信号。
 *        设计上保证：
 *   - 可在 WiFi 回调上下文（可能是事件任务）中安全调用
 *   - 队列未就绪时静默返回，不崩溃
 *   - 队列满时打印日志，不影响 WiFi 重连流程
 */
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

/**
 * @brief 启动北向传输模块（创建消息队列 + 启动内核任务）
 * @return 0 成功，-1 失败
 * @note  此函数由 qrswork_client_main.c 在系统初始化阶段调用，只执行一次。
 *        任务优先级 13 与 SDK 官方 UDP Client 范例对齐。
 */
int northbound_client_start(void)
{
    osal_task *task_handle = NULL;

    // 防止重复启动
    if (g_qrswork_nb_task_started != 0) {
        return 0;
    }

    // 创建消息队列：每条消息大小 = sizeof(qrswork_nb_msg_t)
    if (osal_msg_queue_create("qrswork_nb", QRSWORK_NB_QUEUE_LEN, &g_qrswork_nb_queue_id, 0,
        sizeof(qrswork_nb_msg_t)) != OSAL_SUCCESS) {
        osal_printk("%s queue create failed\r\n", QRSWORK_NB_LOG);
        return -1;
    }
    g_qrswork_nb_queue_ready = 1;

    // 创建内核线程
    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)northbound_client_task, 0, "QrsworkNorthbound",
        QRSWORK_NB_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, QRSWORK_NB_TASK_PRIO);
        osal_kfree(task_handle);  // kthread_create 返回的 handle 需要手动释放
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

/**
 * @brief 发布心率数据到北向通道
 * @param adc ADC 周期值（来自 SLE 通知解析）
 * @param bpm 由 ADC 周期换算出的心率值
 *
 * @note  JSON 格式：{"type":"heart","adc":1234,"bpm":72,"source":"qrswork_client"}
 *        此函数可在 SLE 回调中直接调用，内部通过消息队列异步发送。
 */
void northbound_client_publish_heart(uint32_t adc, uint32_t bpm)
{
    char payload[QRSWORK_NB_PAYLOAD_MAX_LEN] = {0};
    int ret = snprintf_s(payload, sizeof(payload), sizeof(payload) - 1,
        "{\"type\":\"heart\",\"adc\":%u,\"bpm\":%u,\"source\":\"qrswork_client\"}\r\n", adc, bpm);
    if (ret > 0) {
        (void)northbound_client_enqueue(payload, (uint16_t)ret);
    }
}

/**
 * @brief 发布原始数据到北向通道（十六进制摘要，最多 32 字节）
 * @param data 原始字节数组
 * @param len  数据长度
 *
 * @note  JSON 格式：{"type":"raw","len":4,"hex":"01 02 03 04","source":"qrswork_client"}
 *        超过 32 字节的数据只展示前 32 字节的十六进制摘要。
 */
void northbound_client_publish_raw(const uint8_t *data, uint16_t len)
{
    char hex[(QRSWORK_NB_RAW_HEX_BYTES * 3) + 1] = {0};  // 每字节 "xx " = 3 字符 + 结尾 '\0'
    char payload[QRSWORK_NB_PAYLOAD_MAX_LEN] = {0};
    uint16_t hex_len = (len > QRSWORK_NB_RAW_HEX_BYTES) ? QRSWORK_NB_RAW_HEX_BYTES : len;
    int offset = 0;
    int ret;

    if ((data == NULL) || (len == 0)) {
        return;
    }

    // 将原始字节转为十六进制字符串，字节间用空格分隔
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
