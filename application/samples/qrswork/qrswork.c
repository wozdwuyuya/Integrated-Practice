#include "qrswork.h"
#include <stdio.h>
#include <stdint.h>

#ifdef CONFIG_SAMPLE_SUPPORT_QRSWORK_SERVER
#include "securec.h"
#include "sle_low_latency.h"
#include "sle_common.h"
#include "qrswork_server/qrswork_server.h"
#include "qrswork_server/qrswork_server_adv.h"
#include "sle_device_discovery.h"
#include "sle_errcode.h"
#include "sle_connection_manager.h"
#endif

// 心跳检测独立任务（核心逻辑移至此）
void heart_detect_task(void *arg) {
    osal_printk("Heart detect task start running...\r\n");
    
#ifdef CONFIG_SAMPLE_SUPPORT_QRSWORK_SERVER
    // 等待SLE服务初始化
    osDelay(200);
#endif
    
    while(1) {
        uint32_t heart_adc_val = 0;

        // 1. 读取ADC值（增加3次重试，避免单次采样失败）
        int retry = 3;
        while (retry-- > 0) {
            heart_adc_val = KY_read_heart_adc();
            if (heart_adc_val > 0) break; // 采样成功则退出重试
            osal_msleep(100); // 重试前短延时
        }
        if(heart_adc_val==0){
            osal_printk("Heart ADC raw val: %d\r\n", heart_adc_val);
        }
        else {
            uint32_t heart_bpm = 60000/heart_adc_val;
            osal_printk("Heart ADC raw val: %d heart val: %d\r\n", heart_adc_val, heart_bpm);
        }

        // 2. 根据ADC值控制RGB灯
        heart_status_set_rgb(heart_adc_val);

#ifdef CONFIG_SAMPLE_SUPPORT_QRSWORK_SERVER
        // 3. 通过SLE发送心跳数据到客户端
        if (qrswork_server_is_connected()) {
            uint8_t heart_data[4];
            heart_data[0] = (uint8_t)(heart_adc_val & 0xFF);
            heart_data[1] = (uint8_t)((heart_adc_val >> 8) & 0xFF);
            heart_data[2] = (uint8_t)((heart_adc_val >> 16) & 0xFF);
            heart_data[3] = (uint8_t)((heart_adc_val >> 24) & 0xFF);
            
            errcode_t ret = qrswork_server_send_heart_data(heart_data, sizeof(heart_data));
            if (ret == ERRCODE_SLE_SUCCESS) {
                osal_printk("Heart data sent via SLE successfully\r\n");
            } else {
                osal_printk("Heart data send failed, ret: 0x%x\r\n", ret);
            }
        } else {
            osal_printk("No client connected, skip sending\r\n");
        }
#endif

        // 4. 阻塞式延时1秒（RTOS标准接口，让出CPU）
        osDelay(100); // CMSIS-RTOS2接口，tick=1ms时对应1秒
    }
}


#ifdef CONFIG_SAMPLE_SUPPORT_QRSWORK_SERVER
// SLE服务端回调函数
static void ssaps_server_read_request_cbk(uint8_t server_id, uint16_t conn_id, ssaps_req_read_cb_t *read_cb_para,
    errcode_t status)
{
    osal_printk("[qrswork server] read request callback server_id:%x, conn_id:%x, handle:%x, status:%x\r\n",
        server_id, conn_id, read_cb_para->handle, status);
}

static void ssaps_server_write_request_cbk(uint8_t server_id, uint16_t conn_id, ssaps_req_write_cb_t *write_cb_para,
    errcode_t status)
{
    osal_printk("[qrswork server] write request callback server_id:%x, conn_id:%x, handle:%x, status:%x\r\n",
        server_id, conn_id, write_cb_para->handle, status);
}
#endif

void init_task(void *arg) {
    osal_printk("Init task start...\r\n");
    bool init_ok = true;

    // 硬件初始化
    if(LED_init()) {
        osal_printk("Led init successful\r\n");
    } else {
        osal_printk("LED failed\r\n");
        init_ok = false;
    }
    if(RGB_init()) {
        osal_printk("RGB init successful\r\n");
    } else {
        osal_printk("RGB failed\r\n");
        init_ok = false;
    }
    if(KY_init()){
        osal_printk("KY init successful\r\n");
    }else{
        osal_printk("KY init failed\r\n");
        init_ok = false;
    }

#ifdef CONFIG_SAMPLE_SUPPORT_QRSWORK_SERVER
    // 初始化SLE服务端
    osal_printk("Initializing SLE server...\r\n");
    enable_sle();
    osal_msleep(1000); // 等待SLE启用
    
    errcode_t ret = qrswork_server_init(ssaps_server_read_request_cbk, ssaps_server_write_request_cbk);
    if (ret == ERRCODE_SLE_SUCCESS) {
        osal_printk("SLE server init successful\r\n");
    } else {
        osal_printk("SLE server init failed: %x\r\n", ret);
        init_ok = false;
    }
#endif

    // 初始化成功则创建心跳检测任务
    if (init_ok) {
        osal_printk("All hardware init success!\r\n");
        osThreadAttr_t heart_task_attr = {
            .name = "heart_detect",
            .stack_size = 4096, // 增大栈大小，避免栈溢出
            .priority = osPriorityNormal // 优先级低于Idle，避免抢占
        };
        osThreadId_t heart_task_id = osThreadNew(heart_detect_task, NULL, &heart_task_attr);
        if (heart_task_id == NULL) {
            osal_printk("Heart detect task create failed!\r\n");
        } else {
            osal_printk("Heart detect task create success!\r\n");
        }
    }

    osal_printk("Init task finish, terminate self\r\n");
    osThreadTerminate(NULL); // 终止初始化任务
}

void service(void) {
    // 创建初始化任务
    osThreadAttr_t init_task_attr = {
        .name = "init_task",    
        .stack_size = 2048,     
        .priority = osPriorityNormal
    };
    osThreadId_t init_task_id = osThreadNew(init_task, NULL, &init_task_attr);
    if (init_task_id == NULL) {
        osal_printk("Failed to create init task\r\n");
        return;
    }
    osal_printk("Init task create success!\r\n");

    // 启动RTOS内核（调度器开始工作）
    osal_printk("Start RTOS kernel...\r\n");
    osKernelStart();
}

void qrswork(void){
    osal_printk("App entry: qrswork start\r\n");
    service(); // 启动初始化和内核
    // osKernelStart()后代码不会执行到此处（调度器接管）
}

app_run(qrswork);