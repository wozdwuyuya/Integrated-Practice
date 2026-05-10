/**
 * @file max30102_test.c
 * @brief MAX30102 心率血氧传感器集成联调测试任务
 * @note  验证 I2C 总线通信 + MAX30102 器件ID + 心率血氧数据输出
 *
 * 使用方法：
 *   1. 在 app_main.c 中 #include "test/max30102_test.h"
 *   2. 在 main_task 的 init 区域调用 max30102_test_run()（会阻塞）
 *   3. 或者用 osThreadNew 创建独立线程运行
 *   4. 打开串口（115200），观察 [MAX30102_TEST] 输出
 *   5. 将手指轻放在传感器上，等待 10-20 秒获取稳定读数
 *
 * 总线说明：
 *   MAX30102 与 MPU6050、OLED 共享 I2C1 总线 (GPIO15, GPIO16)
 *   I2C 控制器已由 i2c_master.c 初始化，本测试不会重复初始化以避免冲突
 */

#include "test/max30102_test.h"
#include "sensor/max30102.h"
#include "system/i2c_master.h"
#include "osal_debug.h"
#include "cmsis_os2.h"

// 测试打印间隔（500ms）
#define TEST_PRINT_INTERVAL_MS  500

// I2C 引脚配置（与 i2c_master.c 保持一致，仅用于日志输出）
#define TEST_I2C_SCL_PIN    15
#define TEST_I2C_SDA_PIN    16

void max30102_test_run(void)
{
    osal_printk("========================================\r\n");
    osal_printk("[MAX30102_TEST] MAX30102 Integration Test\r\n");
    osal_printk("[MAX30102_TEST] I2C1 Bus: SCL=GPIO%d, SDA=GPIO%d\r\n",
                TEST_I2C_SCL_PIN, TEST_I2C_SDA_PIN);
    osal_printk("[MAX30102_TEST] Device Addr: 0x%02X\r\n", MAX30102_ADDRESS);
    osal_printk("========================================\r\n");

    // Step 1: I2C 总线由 i2c_master.c 统一初始化，此处不重复初始化
    osal_printk("[MAX30102_TEST] Step 1: I2C bus assumed already initialized\r\n");
    osal_printk("[MAX30102_TEST] (shared with MPU6050 & OLED on I2C1)\r\n");

    // Step 2: MAX30102 初始化（器件ID验证 + 软复位 + 寄存器配置 + 定时器启动）
    osal_printk("[MAX30102_TEST] Step 2: MAX30102 init...\r\n");
    if (!max30102_init()) {
        osal_printk("[MAX30102_TEST] FAIL: MAX30102 init failed\r\n");
        osal_printk("[MAX30102_TEST] Check: 1)SDA/SCL wiring 2)ADDR pin 3)3.3V power\r\n");
        clean_timer();
        return;
    }
    osal_printk("[MAX30102_TEST] MAX30102 init OK\r\n");

    // Step 3: 持续采集并打印心率血氧数据
    osal_printk("[MAX30102_TEST] Step 3: Reading data...\r\n");
    osal_printk("[MAX30102_TEST] Place finger on sensor, wait 10-20s for stable reading\r\n");
    osal_printk("[MAX30102_TEST] Format: Count | HR (BPM) | SpO2 (%%)\r\n");
    osal_printk("========================================\r\n");

    uint32_t print_count = 0;
    uint32_t data_count = 0;

    while (1) {
        // is_time() 内部判断 10ms 采样间隔，由定时器驱动
        if (is_time()) {
            if (main_max30102_data()) {
                data_count++;
                print_count++;
                osal_printk("[MAX30102_TEST] #%lu  HR:%lu BPM  SpO2:%lu%%\r\n",
                            print_count, return_ac[0], return_ac[1]);
            }
        }

        osDelay(TEST_PRINT_INTERVAL_MS);
    }
}
