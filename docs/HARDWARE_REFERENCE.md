# BearPi-Pico H3863 硬件参考与底层驱动开发指南

> 生成时间：2026-05-07 | 基于 SDK `E:\bearpi\bearpi-pico_h3863` + 用户硬件文档

---

## 1. 开发板概况

| 项目 | 参数 |
|------|------|
| 主控芯片 | WS63V / Hi3863V100（QFN-40） |
| 无线 | 2.4GHz Wi-Fi 6 + BLE 5.2 + NearLink SLE |
| 供电 | USB 5V → TPS562200 降压 → 3.3V（最大 2A） |
| 调试串口 | CH340E USB 转 UART |
| 布局 | 兼容 Raspberry Pi Pico 40pin |
| SDK 路径 | `E:\bearpi\bearpi-pico_h3863` |
| 工具链 | `E:\vscode_tools\HiSpark_Toolchain` |

---

## 2. 完整引脚分配表

### 2.1 本项目使用的引脚

| 外设 | 功能 | GPIO | 信号类型 | SDK 配置 |
|------|------|------|----------|----------|
| **I2C1 SCL** | I2C 时钟 | **GPIO15** | I2C 开漏 | `CONFIG_I2C_SCL_MASTER_PIN=15` |
| **I2C1 SDA** | I2C 数据 | **GPIO16** | I2C 开漏 | `CONFIG_I2C_SDA_MASTER_PIN=16` |
| **KY-039** | 心率模拟输入 | ADC_CH2 | ADC 模拟 | `KY039_ADC_CHANNEL=ADC_CHANNEL_2` |
| **RGB Red** | 红色指示 | **GPIO6** | 数字输出（共阳） | `RGB_LED_RED_PIN=6` |
| **RGB Green** | 绿色指示 | **GPIO7** | 数字输出（共阳） | `RGB_LED_GREEN_PIN=7` |
| **RGB Blue** | 蓝色指示 | **GPIO8** | 数字输出（共阳） | `RGB_LED_BLUE_PIN=8` |
| **蜂鸣器** | PWM 报警 | **GPIO5** | PWM 输出 | `beep_channel=5` |
| **SW-420** | 震动检测 | **GPIO4** | 数字输入 | `SW420_GPIO_PIN=4` |
| **震动马达** | 触觉反馈 | **GPIO3** | 数字输出 | `VIBRATION_MOTOR_PIN=3` |

> **共阳 RGB 灯：GPIO LOW = 点亮，GPIO HIGH = 熄灭**

### 2.2 I2C 总线设备（GPIO15/16 共享）

| 设备 | I2C 地址 | 驱动文件 | 说明 |
|------|----------|----------|------|
| SSD1315 OLED | 0x3C | `output/ssd1306.c` | 128x64，兼容 SSD1306 驱动 |
| MPU6050 | 0x68 | `sensor/mpu6050.c` | 六轴 IMU，AD0 悬空/接地=0x68 |
| MAX30102 | 0x57 | `sensor/max30102.c` | 心率血氧传感器，共享 I2C1 总线 (GPIO15, GPIO16) |

### 2.3 I2C 引脚确认

H3863 只有 GPIO15/16 有 I2C1 硬件功能。同学接线确认使用 GPIO15(SCL)/GPIO16(SDA)。

### 2.4 启动限制引脚（上电时不可被强拉高）

| GPIO | 状态 | 说明 |
|------|------|------|
| GPIO1 | 未使用 | 安全 |
| GPIO3 | **震动马达** | 输出模式，上电默认低电平，安全 |
| GPIO4 | **SW-420 传感器** | 输入模式 + 下拉，安全 |
| GPIO6 | **RGB Red** | 输出模式，共阳默认 HIGH(灭)，安全 |
| GPIO9 | 未使用 | 安全 |
| GPIO11 | 未使用 | 安全 |

### 2.5 JTAG 复用冲突

**已解决：** RGB 灯已改用 GPIO6/7/8，GPIO13/14 不再占用，**JTAG 调试可正常使用**。

---

## 3. 供电方案

| 电源轨 | 来源 | 供电外设 |
|--------|------|----------|
| V3.3 (Pin 1) | TPS562200 降压 | KY-039、RGB 灯、蜂鸣器、SW-420、MPU6050、OLED |
| V5.0 (Pin 39) | USB 5V 直连 | 震动马达 |
| GND | 多个引脚 | 所有外设共地 |

**注意：** 震动马达是感性负载，建议加续流二极管（1N4148）和 100μF 滤波电容。

---

## 4. 软件配置对照表

### 4.1 I2C 配置（i2c_master.c）

```c
// SDK Kconfig 默认值（不可随意改）
#define CONFIG_I2C_SCL_MASTER_PIN  15   // GPIO15 = I2C1_SCL
#define CONFIG_I2C_SDA_MASTER_PIN  16   // GPIO16 = I2C1_SDA
#define I2C_MASTER_PIN_MODE         2   // 引脚复用模式2 = I2C功能
#define CONFIG_I2C_MASTER_BUS_ID    1   // I2C 控制器 1
#define I2C_SET_BAUDRATE       400000   // 400KHz Fast Mode
```

### 4.2 I2C 设备地址

```c
// SSD1306 OLED（ssd1306.c）
#define I2C_SLAVE2_ADDR  0x3C

// MPU6050 六轴 IMU（mpu6050.h）
#define MPU6050_ADDRESS  0x68   // AD0 接地/悬空
```

### 4.3 KY-039 ADC 配置（ky039.c）

```c
// ⚠️ 需要根据实际硬件确认通道号
#define KY039_ADC_CHANNEL  ADC_CHANNEL_2  // 对应 KY-039 模拟输入（参考 ky039.c）

// ADC 初始化流程（参考 qrswork 模板）
uapi_adc_init(ADC_CLOCK_500KHZ);         // 参数是时钟频率，不是通道号！
uapi_adc_open_channel(KY039_ADC_CHANNEL); // 打开通道
uapi_adc_power_en(AFE_GADC_MODE, true);   // 使能电源
int32_t raw = uapi_adc_manual_sample(KY039_ADC_CHANNEL);  // 手动采样
```

### 4.4 RGB LED 电平逻辑（rgb_led.c）

```c
// 共阳 RGB 灯：低电平点亮，高电平熄灭
#define RGB_LEVEL_ON   GPIO_LEVEL_LOW
#define RGB_LEVEL_OFF  GPIO_LEVEL_HIGH

// 引脚定义
#define RGB_LED_RED_PIN    6  // GPIO6
#define RGB_LED_GREEN_PIN  7  // GPIO7
#define RGB_LED_BLUE_PIN   8  // GPIO8
```

### 4.5 蜂鸣器（beep.c）

```c
// 无源蜂鸣器：GPIO5 PWM 驱动
// PWM通道5（GPIO5 → PWM通道5）
#define beep_channel  5
```

### 4.6 SW-420 震动传感器（sw420.c）

```c
// 数字输入：低电平 = 检测到震动
#define SW420_GPIO_PIN  4  // GPIO4（启动限制引脚，上电不可强拉高）
// 配置：GPIO 输入 + 下降沿中断
uapi_pin_set_mode(SW420_GPIO_PIN, 0);  // GPIO 模式
uapi_gpio_set_dir(SW420_GPIO_PIN, GPIO_DIRECTION_INPUT);
uapi_pin_set_irq(SW420_GPIO_PIN, GPIO_IRQ_TYPE_EDGE_FALLING, handler, 0);
```

### 4.7 震动马达（vibration_motor.c）

```c
// 高电平开启震动
#define VIBRATION_MOTOR_PIN  3  // GPIO3（启动限制引脚，输出模式安全）
// ⚠️ 5V 供电，需三极管/MOSFET 驱动 + 续流二极管保护
```

---

## 5. SDK 编码规范

### 5.1 入口函数

```c
// ✅ 正确：使用 app_run() 宏
static void *main_task(const char *arg) {
    unused(arg);
    while(1) { ... return NULL; }
}
static void main_entry(void) { ... }
app_run(main_entry);

// ❌ 错误：不要用 void app_main() + osKernelStart()
```

### 5.2 日志输出

```c
// ✅ 正确：osal_printk()（SDK 标准）
#include "osal_debug.h"
osal_printk("[MODULE] message %d\r\n", value);

// ❌ 错误：printf()（嵌入式无 stdout 重定向时无输出）
```

### 5.3 任务函数签名

```c
// ✅ 正确：返回 void*，参数 const char*
static void *task_fn(const char *arg) {
    unused(arg);
    // ...
    return NULL;
}

// ❌ 错误：void task_fn(void *arg)
```

### 5.4 延时函数

```c
osDelay(ms);        // CMSIS-RTOS2 标准
osal_msleep(ms);    // SDK 封装（推荐）
```

### 5.5 互斥锁

```c
osMutexId_t mutex = osMutexNew(NULL);
osMutexAcquire(mutex, osWaitForever);
// ... 临界区 ...
osMutexRelease(mutex);
```

---

## 6. 复用信号速查

| 功能 | GPIO | 说明 |
|------|------|------|
| I2C1 | SDA=GPIO15, SCL=GPIO16 | **唯一 I2C1 引脚** |
| UART1 | TX=GPIO15, RX=GPIO16 | 与 I2C1 共享引脚 |
| UART2 | TX=GPIO8, RX=GPIO7 | |
| SPI0 | SCK=GPIO7, CS1=GPIO8 | |
| SPI1 | SCK=GPIO6, CS=GPIO0 | |
| JTAG | TDI=GPIO0, TMS=GPIO13, TCK=GPIO14, TDO=GPIO9 | |
| PWM | 几乎所有 GPIO 均有 PWM0-7 | |
| I2S | MCLK=GPIO7, SCLK=GPIO10, LRCLK=GPIO11 | |

---

## 7. 未使用引脚（可扩展）

| GPIO | 状态 | 说明 |
|------|------|------|
| GPIO1 | 未使用 | 启动限制引脚 |
| GPIO9 | 未使用 | 启动限制引脚 |
| GPIO11 | 未使用 | 启动限制引脚 |
| GPIO13 | 未使用 | JTAG_TMS（可调试用） |
| GPIO14 | 未使用 | JTAG_TCK（可调试用） |

---

## 8. 硬件驱动参考代码

### 8.1 I2C 总线初始化

```c
#include "pinctrl.h"
#include "i2c.h"
#include "soc_osal.h"

#define I2C_SCL_PIN     15
#define I2C_SDA_PIN     16
#define I2C_PIN_MODE     2
#define I2C_BUS_ID       1
#define I2C_BAUDRATE    400000

void app_i2c_init_pin(void) {
    uapi_pin_set_mode(I2C_SCL_PIN, I2C_PIN_MODE);
    uapi_pin_set_mode(I2C_SDA_PIN, I2C_PIN_MODE);
    uapi_pin_set_pull(I2C_SCL_PIN, PIN_PULL_TYPE_UP);
    uapi_pin_set_pull(I2C_SDA_PIN, PIN_PULL_TYPE_UP);
}

void i2c_bus_init(void) {
    app_i2c_init_pin();
    uapi_i2c_master_init(I2C_BUS_ID, I2C_BAUDRATE, 0x0);
}
```

### 8.2 I2C 读写（MPU6050/OLED 通用模式）

```c
#include "i2c.h"

// 写单字节
errcode_t i2c_write_reg(uint8_t bus, uint8_t dev_addr, uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    i2c_data_t data = {0};
    data.send_buf = buf;
    data.send_len = 2;
    return uapi_i2c_master_write(bus, dev_addr, &data);
}

// 读单字节
errcode_t i2c_read_reg(uint8_t bus, uint8_t dev_addr, uint8_t reg, uint8_t *val) {
    i2c_data_t data = {0};
    data.send_buf = &reg;
    data.send_len = 1;
    data.receive_buf = val;
    data.receive_len = 1;
    return uapi_i2c_master_read(bus, dev_addr, &data);
}

// 读多字节（连续读取）
errcode_t i2c_read_buf(uint8_t bus, uint8_t dev_addr, uint8_t reg, uint8_t *buf, uint8_t len) {
    i2c_data_t data = {0};
    data.send_buf = &reg;
    data.send_len = 1;
    data.receive_buf = buf;
    data.receive_len = len;
    return uapi_i2c_master_writeread(bus, dev_addr, &data);
}
```

### 8.3 GPIO 控制（RGB/蜂鸣器/震动马达）

```c
#include "gpio.h"
#include "pinctrl.h"

// GPIO 初始化为输出
void gpio_output_init(uint8_t pin) {
    uapi_pin_set_mode(pin, 0);  // 模式0 = GPIO
    uapi_gpio_set_dir(pin, GPIO_DIRECTION_OUTPUT);
}

// GPIO 初始化为输入
void gpio_input_init(uint8_t pin) {
    uapi_pin_set_mode(pin, 0);
    uapi_gpio_set_dir(pin, GPIO_DIRECTION_INPUT);
}

// 设置输出电平
void gpio_set(uint8_t pin, bool high) {
    uapi_gpio_set_val(pin, high ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW);
}
```

### 8.4 ADC 采样（KY-039）

```c
#include "adc.h"

void adc_init(void) {
    uapi_adc_init(ADC_CLOCK_500KHZ);        // 时钟频率，非通道号
    uapi_adc_open_channel(ADC_CHANNEL_2);    // KY-039 → ADC 通道 2
    uapi_adc_power_en(AFE_GADC_MODE, true);
}

uint16_t adc_read(void) {
    int32_t raw = uapi_adc_manual_sample(ADC_CHANNEL_2);
    return (raw < 0) ? 0 : (uint16_t)raw;
}
```

### 8.5 GPIO 中断（SW-420 震动检测）

```c
#include "gpio.h"

static void vibration_irq(uint8_t pin, uint32_t data) {
    // 震动事件处理
}

void sw420_init(void) {
    uapi_pin_set_mode(5, 0);
    uapi_gpio_set_dir(5, GPIO_DIRECTION_INPUT);
    uapi_pin_set_irq(5, GPIO_IRQ_TYPE_EDGE_FALLING, vibration_irq, 0);
}
```

---

## 9. 编译与烧录

### 9.1 编译方式

```
HiSpark Studio VSCode 插件 → 打开 SDK 工作区 → 选择 target ws63-liteos-app → 编译
```

### 9.2 项目注册

用户代码放在 `E:\bearpi\bearpi-pico_h3863\application\samples\custom\demo\`，SDK 自动扫描编译。

### 9.3 串口调试

- CH340E USB 转串口
- 波特率：115200
- 使用 `osal_printk()` 输出调试信息

---

## 10. 已知问题与待确认项

| # | 问题 | 状态 | 说明 |
|---|------|------|------|
| 1 | **I2C 引脚** | ✅ 已确认 | 同学接线确认 GPIO15(SCL)/GPIO16(SDA) |
| 2 | **KY-039 ADC 通道** | ✅ 已确认 | 所有模板用 ADC_CHANNEL_2，代码正确 |
| 3 | **震动马达缺保护电路** | ⚠️ 硬件 | 感性负载需续流二极管 + MOSFET 驱动 |
| 4 | **SSD1315 兼容性** | ℹ️ 已验证 | SSD1315 与 SSD1306 驱动兼容，无需修改 |
| 5 | **GPIO3/4/6 启动限制** | ✅ 已处理 | 输出模式 + 默认电平安全，不影响启动 |
