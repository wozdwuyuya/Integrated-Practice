/**
 * @file i2c_master.c
 * @brief I2C主设备初始化和外设配置
 * @note 初始化I2C总线、SSD1306显示屏、MPU6050
 */

#include "i2c_master.h"
#include "sensor/mpu6050.h"
#include "output/ssd1306.h"
#include "output/ssd1306_fonts.h"
#include "gpio.h"
#include "osal_debug.h"

static osMutexId_t g_i2c_bus_mutex = NULL;  // I2C总线互斥锁

// I2C锁超时：防止总线挂死导致线程永久阻塞
#define I2C_LOCK_TIMEOUT_MS  500

/**
 * I2C引脚配置说明：
 *   当前配置：SCL=GPIO15(Pin9), SDA=GPIO16(Pin10)
 *   依据：SDK Kconfig默认值 + H3863 Pinout表确认只有GPIO15/16有I2C1硬件功能
 *   ⚠️ 如果你的MPU6050/OLED实际接的是GPIO7/8，硬件接线有误，需要改线到GPIO15/16
 *   ⚠️ 如果确认要改引脚：修改下面两个宏的值（但GPIO7/8无I2C alternate function，改了也不工作）
 */
#define CONFIG_I2C_SCL_MASTER_PIN 15  // SCL时钟引脚（GPIO15 = I2C1_SCL）
#define CONFIG_I2C_SDA_MASTER_PIN 16  // SDA数据引脚（GPIO16 = I2C1_SDA）
#define I2C_MASTER_PIN_MODE 2         // 引脚复用模式2 = I2C功能

// I2C波特率：400KHz Fast Mode（SDK示例标准值）
// ⚠️ 如果总线不稳定可降至100KHz（Standard Mode），但会降低多设备共享总线效率
#define I2C_SET_BAUDRATE  400000
#define I2C_MASTER_ADDR 0x0       // 主设备地址

// I2C总线恢复：当SDA被从设备拉低时，通过SCL脉冲释放总线
static void i2c_bus_recovery(void){
    osal_printk("[I2C] Bus recovery: toggling SCL...\r\n");

    // 切换SCL为GPIO输出模式
    uapi_pin_set_mode(CONFIG_I2C_SCL_MASTER_PIN, 0);  // GPIO模式
    uapi_gpio_set_dir(CONFIG_I2C_SCL_MASTER_PIN, GPIO_DIRECTION_OUTPUT);

    // 发送最多9个SCL脉冲，释放SDA
    for(int i = 0; i < 9; i++) {
        uapi_gpio_set_val(CONFIG_I2C_SCL_MASTER_PIN, GPIO_LEVEL_LOW);
        osDelay(1);
        uapi_gpio_set_val(CONFIG_I2C_SCL_MASTER_PIN, GPIO_LEVEL_HIGH);
        osDelay(1);
    }

    // 生成STOP条件：SDA从低到高（SCL为高时）
    uapi_pin_set_mode(CONFIG_I2C_SDA_MASTER_PIN, 0);  // GPIO模式
    uapi_gpio_set_dir(CONFIG_I2C_SDA_MASTER_PIN, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(CONFIG_I2C_SDA_MASTER_PIN, GPIO_LEVEL_LOW);
    osDelay(1);
    uapi_gpio_set_val(CONFIG_I2C_SDA_MASTER_PIN, GPIO_LEVEL_HIGH);
    osDelay(1);

    // 恢复I2C引脚模式
    uapi_pin_set_mode(CONFIG_I2C_SCL_MASTER_PIN, I2C_MASTER_PIN_MODE);
    uapi_pin_set_mode(CONFIG_I2C_SDA_MASTER_PIN, I2C_MASTER_PIN_MODE);
    uapi_pin_set_pull(CONFIG_I2C_SCL_MASTER_PIN, PIN_PULL_TYPE_UP);
    uapi_pin_set_pull(CONFIG_I2C_SDA_MASTER_PIN, PIN_PULL_TYPE_UP);

    osal_printk("[I2C] Bus recovery done\r\n");
}

// I2C引脚初始化：配置SCL和SDA引脚模式和上拉
static void app_i2c_init_pin(void)
{
    osal_printk("[I2C] Pin init: SCL=GPIO%d, SDA=GPIO%d, mode=%d\r\n",
           CONFIG_I2C_SCL_MASTER_PIN, CONFIG_I2C_SDA_MASTER_PIN, I2C_MASTER_PIN_MODE);

    uapi_pin_set_mode(CONFIG_I2C_SCL_MASTER_PIN, I2C_MASTER_PIN_MODE);
    uapi_pin_set_mode(CONFIG_I2C_SDA_MASTER_PIN, I2C_MASTER_PIN_MODE);
    uapi_pin_set_pull(CONFIG_I2C_SCL_MASTER_PIN, PIN_PULL_TYPE_UP);
    uapi_pin_set_pull(CONFIG_I2C_SDA_MASTER_PIN, PIN_PULL_TYPE_UP);
}


// I2C总线初始化：初始化引脚、I2C控制器、SSD1306、MPU6050
void all_i2c_init(void){
    errcode_t ret;

    osal_printk("[I2C] === I2C Bus Init Start ===\r\n");

    // Step1: 引脚初始化
    osal_printk("[I2C] Step1: Pin config...\r\n");
    app_i2c_init_pin();

    // 创建I2C互斥锁（保护多设备共享总线）
    if(g_i2c_bus_mutex == NULL) {
        g_i2c_bus_mutex = osMutexNew(NULL);
    }

    // Step2: 尝试总线恢复（防止上次异常导致SDA卡死）
    i2c_bus_recovery();
    osDelay(10);

    // Step3: I2C控制器初始化
    osal_printk("[I2C] Step2: Controller init (bus=%d, baud=%d)...\r\n",
           CONFIG_I2C_MASTER_BUS_ID, I2C_SET_BAUDRATE);
    ret = uapi_i2c_master_init(CONFIG_I2C_MASTER_BUS_ID, I2C_SET_BAUDRATE, I2C_MASTER_ADDR);
    if(ret != ERRCODE_SUCC){
        osal_printk("[I2C] FAILED! ret=0x%x\r\n", ret);
        osal_printk("[I2C] Check: 1)Pin mode 2)Pull-up 3)GPIO15/16 wiring\r\n");
        return;
    }
    osal_printk("[I2C] Controller init OK\r\n");

    // Step4: 等待外设上电稳定
    osal_printk("[I2C] Step3: Waiting 200ms for devices power-up...\r\n");
    osDelay(200);

    // Step5: 逐个初始化I2C设备，每步间隔50ms，方便定位问题
    osal_printk("[I2C] Step4: Init SSD1306 (addr=0x3C)...\r\n");
    ssd1306_Init();
    osDelay(50);

    osal_printk("[I2C] Step5: Init MPU6050 (addr=0x68)...\r\n");
    if(mpu6050_init(MPU6050_ACCEL_RANGE_4G, MPU6050_GYRO_RANGE_500)){
        osal_printk("[I2C] MPU6050 OK\r\n");
    }else{
        osal_printk("[I2C] MPU6050 FAIL - check wiring and address\r\n");
    }

    osal_printk("[I2C] === I2C Init Complete ===\r\n");

    // 显示初始化结果到OLED
    ssd1306_Fill(Black);
    ssd1306_SetCursor(0, 0);
    ssd1306_DrawString("I2C Init Done", Font_7x10, White);
    ssd1306_UpdateScreen();
}

// 获取I2C总线互斥锁（供其他模块使用）
osMutexId_t i2c_master_get_mutex(void){
    return g_i2c_bus_mutex;
}

// 获取I2C总线锁（带超时和运行时总线恢复）
bool i2c_master_lock(void){
    if(g_i2c_bus_mutex == NULL) {
        return false;
    }
    osStatus_t status = osMutexAcquire(g_i2c_bus_mutex, I2C_LOCK_TIMEOUT_MS);
    if(status != osOK) {
        osal_printk("[I2C] Mutex acquire timeout (0x%X)! Attempting bus recovery...\r\n", status);
        i2c_bus_recovery();
        status = osMutexAcquire(g_i2c_bus_mutex, I2C_LOCK_TIMEOUT_MS);
        if(status != osOK) {
            osal_printk("[I2C] Bus recovery failed, lock still unavailable (0x%X)\r\n", status);
            return false;
        }
    }
    return true;
}

// 释放I2C总线锁
void i2c_master_unlock(void){
    if(g_i2c_bus_mutex != NULL) {
        osMutexRelease(g_i2c_bus_mutex);
    }
}
