#include "pinctrl.h"
#include "soc_osal.h"
#include "adc.h"          // 3863 SDK自带ADC_CLOCK_250KHZ宏，无需手动定义
#include "errcode.h"
#include "osal_debug.h"
#include "cmsis_os2.h"
#include "app_init.h"

// 复用原有KY通道定义（3863芯片KY-039硬件对应通道2）
#define KY_ADC_CHANNEL  2

// 提前声明函数，解决隐式声明错误（核心修复点1）
int ky_adc_test_init(void);

// 全局变量：标记ADC是否初始化完成（供KY_read_heart_adc调用）
static int g_ky_adc_inited = 0;

/**
 * @brief 适配原有qrswork的KY_init函数（对外暴露的初始化接口）
 * @return 0:成功 非0:失败
 */
int KY_init(void) {
    osal_printk("[KY-COMPAT] KY_init called (adapt qrswork)\r\n");
    int ret = ky_adc_test_init();
    if (ret == 0) {
        g_ky_adc_inited = 1; // 标记初始化完成
    }
    return ret;
}

/**
 * @brief 适配原有qrswork的KY_read_heart_adc函数（对外暴露的采样接口）
 * @return ADC原始值（负数表示失败）
 */
int32_t KY_read_heart_adc(void) {
    if (!g_ky_adc_inited) {
        osal_printk("[KY-COMPAT] KY_read_heart_adc: ADC not inited!\r\n");
        return -1; // 返回负数表示未初始化
    }
    // 调用3863底层ADC采样接口，兼容原有逻辑
    int32_t raw_adc = uapi_adc_manual_sample(KY_ADC_CHANNEL);
    osal_printk("[KY-COMPAT] KY_read_heart_adc: %d\r\n", raw_adc);
    return raw_adc;
}

/**
 * @brief 仅初始化KY-039对应的ADC模块（适配3863芯片）
 * @return 0:成功 非0:失败（返回错误码）
 */
int ky_adc_test_init(void) {
    errcode_t ret;
    osal_printk("[KY-TEST] Start ADC init (3863 chip)...\r\n");

    // 1. 打印ADC初始化入参（使用3863 SDK原生ADC_CLOCK_250KHZ宏）
    osal_printk("[KY-TEST] ADC clock config: %d (3863 SDK macro)\r\n", ADC_CLOCK_250KHZ);
    osal_printk("[KY-TEST] ADC channel: %d\r\n", KY_ADC_CHANNEL);

    // 2. 初始化ADC核心（核心修复点2：适配3863的uint16_t参数类型，无溢出）
    ret = uapi_adc_init((uint16_t)ADC_CLOCK_250KHZ);  // 3863实际参数为uint16_t
    if (ret != ERRCODE_SUCC) {
        osal_printk("[KY-TEST] uapi_adc_init FAILED! ret=0x%x\r\n", ret);
        return ret;
    }
    osal_printk("[KY-TEST] uapi_adc_init SUCCESS! ret=0x%x\r\n", ret);

    // 3. 打开ADC通道（3863芯片ADC通道2需确保引脚复用正确）
    ret = uapi_adc_open_channel(KY_ADC_CHANNEL);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[KY-TEST] uapi_adc_open_channel FAILED! ret=0x%x\r\n", ret);
        return ret;
    }
    osal_printk("[KY-TEST] uapi_adc_open_channel SUCCESS! ret=0x%x\r\n", ret);

    // 4. 使能ADC电源（3863 SDK该函数返回void，移除赋值）
    uapi_adc_power_en(AFE_GADC_MODE, true);
    osal_printk("[KY-TEST] uapi_adc_power_en called (3863 AFE_GADC_MODE)\r\n");
    adc_scan_config_t config = {
        .type = 0,
        .freq = 1,
    };
    osal_printk("[KY-TEST] ADC init ALL DONE (3863 chip)!\r\n");
    return 0;
}

/**
 * @brief KY-039 ADC采样测试任务（3863芯片无限循环）
 */
void ky_test_task(void *arg) {
    osal_printk("[KY-TEST] Test task start (3863 chip)...\r\n");
    
    // 初始化ADC（复用适配qrswork的KY_init）
    int init_ret = KY_init();
    if (init_ret != 0) {
        osal_printk("[KY-TEST] ADC init FAILED! test abort!\r\n");
        osThreadTerminate(NULL);
        return;
    }

    // 3863芯片ADC采样循环（每500ms一次）
    while (1) {
        // 复用适配qrswork的KY_read_heart_adc
        int32_t raw_adc = KY_read_heart_adc();
        
        // 打印3863 ADC原始值（范围0~4095）
        osal_printk("[KY-TEST] Raw ADC val: %d | ", raw_adc);
        if (raw_adc < 0) {
            osal_printk("SAMPLING FAILED (3863 ADC error)\r\n");
        } else if (raw_adc == 0) {
            osal_printk("VALUE=0 (check 3863 KY-039 hardware/pin)\r\n");
        } else {
            osal_printk("VALID VALUE (3863 ADC normal)\r\n");
        }

        osal_msleep(500); // 3863芯片osal_msleep适配LiteOS
    }
}

/**
 * @brief 测试入口函数（3863芯片替换原有qrswork逻辑）
 */
void ky_test_entry(void) {
    osal_printk("[KY-TEST] App entry: KY-039 Sensor Test (3863 chip)\r\n");

    // 创建3863芯片测试任务（栈大小≥8K，避免栈溢出）
    osThreadAttr_t test_task_attr = {
        .name = "ky_test_task",
        .stack_size = 8192,  
        .priority = osPriorityHigh  // 避免被WiFi任务抢占
    };
    osThreadId_t task_id = osThreadNew(ky_test_task, NULL, &test_task_attr);
    if (task_id == NULL) {
        osal_printk("[KY-TEST] Create test task FAILED (3863)!\r\n");
        return;
    }
    osal_printk("[KY-TEST] Create test task SUCCESS (3863)!\r\n");

    // 启动3863芯片LiteOS内核（必须最后调用）
    osKernelStart();
}

// 3863芯片HiSpark SDK专属入口注册（小写app_run适配你的SDK版本）
app_run(ky_test_entry);