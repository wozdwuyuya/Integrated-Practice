/**
 * @file mpu6050_test.c
 * @brief MPU6050 集成联调测试任务
 * @note  验证 I2C 总线通信 + MPU6050 WHO_AM_I + 卡尔曼滤波数据输出
 *
 * 使用方法：
 *   1. 在 app_main.c 中 #include "test/mpu6050_test.h"
 *   2. 在 main_task 的 init 区域调用 mpu6050_test_run()（会阻塞）
 *   3. 或者用 osThreadNew 创建独立线程运行
 *   4. 打开串口（115200），观察 [MPU6050_TEST] 输出
 */

#include "test/mpu6050_test.h"
#include "sensor/mpu6050.h"
#include "system/i2c_master.h"
#include "pinctrl.h"
#include "i2c.h"
#include "osal_debug.h"
#include "cmsis_os2.h"

// I2C 引脚配置（与 i2c_master.c 保持一致）
#define TEST_I2C_SCL_PIN    15
#define TEST_I2C_SDA_PIN    16
#define TEST_I2C_PIN_MODE   2       // 复用模式2 = I2C功能
#define TEST_I2C_BUS_ID     1
#define TEST_I2C_BAUDRATE   400000

// 测试循环间隔
#define TEST_READ_INTERVAL_MS  1000  // 1秒

// I2C 引脚初始化
static void test_i2c_pin_init(void)
{
    uapi_pin_set_mode(TEST_I2C_SCL_PIN, TEST_I2C_PIN_MODE);
    uapi_pin_set_mode(TEST_I2C_SDA_PIN, TEST_I2C_PIN_MODE);
    uapi_pin_set_pull(TEST_I2C_SCL_PIN, PIN_PULL_TYPE_UP);
    uapi_pin_set_pull(TEST_I2C_SDA_PIN, PIN_PULL_TYPE_UP);

    osal_printk("[MPU6050_TEST] I2C pin init: SCL=GPIO%d, SDA=GPIO%d, mode=%d\r\n",
                TEST_I2C_SCL_PIN, TEST_I2C_SDA_PIN, TEST_I2C_PIN_MODE);
}

void mpu6050_test_run(void)
{
    osal_printk("========================================\r\n");
    osal_printk("[MPU6050_TEST] MPU6050 Integration Test\r\n");
    osal_printk("========================================\r\n");

    // Step 1: I2C 引脚复用配置
    osal_printk("[MPU6050_TEST] Step 1: I2C pin init...\r\n");
    test_i2c_pin_init();

    // Step 2: I2C 控制器初始化
    osal_printk("[MPU6050_TEST] Step 2: I2C master init (bus=%d, baud=%d)...\r\n",
                TEST_I2C_BUS_ID, TEST_I2C_BAUDRATE);
    errcode_t ret = uapi_i2c_master_init(TEST_I2C_BUS_ID, TEST_I2C_BAUDRATE, 0x0);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[MPU6050_TEST] FAIL: I2C init error, ret=0x%X\r\n", ret);
        osal_printk("[MPU6050_TEST] Check: GPIO15/16 wiring, pull-up resistors\r\n");
        return;
    }
    osal_printk("[MPU6050_TEST] I2C init OK\r\n");

    // Step 3: 等待外设上电稳定
    osDelay(200);

    // Step 4: MPU6050 初始化（WHO_AM_I 检查 + 软复位 + 量程配置）
    osal_printk("[MPU6050_TEST] Step 3: MPU6050 init (accel=4G, gyro=500dps)...\r\n");
    if (!mpu6050_init(MPU6050_ACCEL_RANGE_4G, MPU6050_GYRO_RANGE_500)) {
        osal_printk("[MPU6050_TEST] FAIL: MPU6050 not found at addr 0x%02X\r\n", MPU6050_ADDRESS);
        osal_printk("[MPU6050_TEST] Check: 1)SDA/SCL wiring 2)AD0 pin level 3)3.3V power\r\n");
        return;
    }
    osal_printk("[MPU6050_TEST] MPU6050 init OK\r\n");

    // Step 5: 持续读取并打印数据
    osal_printk("[MPU6050_TEST] Step 4: Reading data (1Hz)...\r\n");
    osal_printk("[MPU6050_TEST] Format: AX,AY,AZ (g) | GX,GY,GZ (dps) | Temp (C)\r\n");
    osal_printk("========================================\r\n");

    uint32_t read_count = 0;
    while (1) {
        mpu6050_data_t data;

        if (mpu6050_read_processed(&data)) {
            read_count++;
            osal_printk("[MPU6050_TEST] #%lu  A:%.3f,%.3f,%.3f  G:%.2f,%.2f,%.2f  T:%.1f\r\n",
                        read_count,
                        data.accel_g[0], data.accel_g[1], data.accel_g[2],
                        data.gyro_dps[0], data.gyro_dps[1], data.gyro_dps[2],
                        data.temperature);
        } else {
            osal_printk("[MPU6050_TEST] Read FAILED (I2C error)\r\n");
        }

        osDelay(TEST_READ_INTERVAL_MS);
    }
}
