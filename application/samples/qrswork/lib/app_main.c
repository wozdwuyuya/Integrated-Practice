/**
 * @file app_main.c
 * @brief 系统入口函数（BearPi-H3863平台）
 * @note 使用SDK标准 app_run() 入口模式
 *
 * ======================================================================
 * 硬件引脚配置说明（基于同学接线表，如实际不同请修改对应文件的宏定义）
 * ======================================================================
 *
 * I2C 总线（GPIO15/16）—— system/i2c_master.c
 *   SCL = GPIO15 (复用信号2)  ← H3863 唯一有 I2C1_SCL 功能的引脚
 *   SDA = GPIO16 (复用信号2)  ← H3863 唯一有 I2C1_SDA 功能的引脚
 *   挂载设备：
 *     1. SSD1315 OLED（I2C地址 0x3C，128x64，兼容SSD1306驱动）
 *     2. MAX30102 心率血氧（I2C地址 0x57）
 *     3. MPU6050 三轴陀螺仪+三轴加速度（I2C地址 0x68）
 *
 * KY-039 心率传感器 —— sensor/ky039.c
 *   当前配置：ADC_CHANNEL_2（参考qrswork模板）
 *   ⚠️ 请确认实际接的GPIO引脚，修改 ky039.c 中 KY039_ADC_CHANNEL
 *
 * 震动马达 —— output/vibration_motor.c
 *   GPIO3 数字输出，高电平=开启震动
 *   ⚠️ 5V 供电，需三极管/MOSFET 驱动 + 续流二极管保护
 *
 * 震动传感器 SW-420 —— sensor/sw420.c
 *   GPIO4 数字输入，低电平=检测到震动
 *   ⚠️ GPIO4 是启动限制引脚，上电时不可被强拉高
 *
 * 蜂鸣器（无源） —— output/beep.c
 *   GPIO5 PWM 输出
 *
 * RGB 三色灯（共阳，低电平点亮）—— output/rgb_led.c
 *   Red   = GPIO6
 *   Green = GPIO7
 *   Blue  = GPIO8
 *   共阳接法：GPIO LOW = 点亮，GPIO HIGH = 熄灭
 *   ⚠️ 如果改接共阴灯珠，需反转 rgb_led.c 中所有 GPIO_LEVEL（HIGH↔LOW）
 *
 * MAX30102 心率血氧 —— sensor/max30102.c
 *   I2C 地址 0x57，与 OLED/MPU6050 共享 I2C 总线
 *
 * ======================================================================
 */

#include "common_def.h"
#include "osal_debug.h"
#include "cmsis_os2.h"
#include "app_init.h"
#include "app/health_monitor_main.h"
#include "sensor/max30102.h"
#include "output/ssd1306.h"
#include "output/rgb_led.h"

#define MAIN_TASK_STACK_SIZE    0x2000
#define MAIN_TASK_PRIO          (osPriority_t)(17)

static volatile bool g_system_running = true;

static void *main_task(const char *arg)
{
    unused(arg);

    osal_printk("[MAIN] Main task started\r\n");

    if(!health_monitor_init()) {
        osal_printk("[MAIN] System init FAILED!\r\n");
#if !MOCK_HARDWARE_MODE
        rgb_led_status_error();
#endif
        return NULL;
    }

#if !MOCK_HARDWARE_MODE
    rgb_led_status_normal();
#endif
    osal_printk("[MAIN] System running!\r\n");

    while(g_system_running) {
#if !MOCK_HARDWARE_MODE
        if(is_time()) {
            main_max30102_data();
        }
#endif
        health_monitor_loop();
        osDelay(10);
    }

    return NULL;
}

static void main_entry(void)
{
    osThreadAttr_t attr;

    attr.name = "MainTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = MAIN_TASK_STACK_SIZE;
    attr.priority = MAIN_TASK_PRIO;

    if(osThreadNew((osThreadFunc_t)main_task, NULL, &attr) == NULL) {
        osal_printk("[MAIN] Main task create FAILED!\r\n");
    }
}

app_run(main_entry);
