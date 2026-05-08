# 硬件底层交接文档

> 给负责硬件底层驱动的同学
> 更新时间：2026-05-07

---

## 一、系统架构：MOCK_HARDWARE_MODE

项目支持两种运行模式，通过 `health_monitor_main.h` 中的宏切换：

```c
// health_monitor_main.h
#define MOCK_HARDWARE_MODE  1   // 1=模拟模式（当前）, 0=真实硬件
```

**Mock 模式（当前）：** 跳过所有 I2C/GPIO 初始化，用正弦波假数据跑通算法链路，通过串口输出 JSON。

**真实模式：** 需要你补全硬件初始化和驱动。设置 `MOCK_HARDWARE_MODE = 0` 后编译。

### 代码中的条件编译位置

所有 `#if MOCK_HARDWARE_MODE` 分支都在以下文件中：

| 文件 | 位置 | Mock 行为 | 你需要补全的 |
|------|------|-----------|-------------|
| `app_main.c:66-78` | main_task | 跳过 MAX30102 采样和 RGB 状态 | 无（已由 SDK 定时器驱动） |
| `health_monitor_main.c:107-192` | health_monitor_init() | 跳过所有硬件 init | 无（init 函数已写好，在 #else 中） |
| `health_monitor_main.c:196-260` | update_sensor_data() | 调用 mock_generate_data() | 无（真实读取代码已在 #else 中） |
| `health_monitor_main.c:494-516` | health_monitor_loop() | 跳过 OLED/动画/SLE | 无（已在 #else 中） |

**结论：** 真实模式的代码已经写好在 `#else` 分支中，你不需要额外编写驱动代码。只需要：

1. 设置 `MOCK_HARDWARE_MODE` 为 `0`
2. 确认硬件接线正确
3. 编译烧录

---

## 二、硬件引脚总表

| 外设 | GPIO | 模式 | SDK 配置 | 备注 |
|------|------|------|----------|------|
| I2C1 SCL | **GPIO15** | 复用模式2 | `CONFIG_I2C_SCL_MASTER_PIN=15` | H3863 唯一 I2C1 引脚 |
| I2C1 SDA | **GPIO16** | 复用模式2 | `CONFIG_I2C_SDA_MASTER_PIN=16` | 挂载 OLED+MAX30102+MPU6050 |
| RGB Red | **GPIO6** | 输出（共阳） | `RGB_LED_RED_PIN=6` | LOW=亮 |
| RGB Green | **GPIO7** | 输出（共阳） | `RGB_LED_GREEN_PIN=7` | LOW=亮 |
| RGB Blue | **GPIO8** | 输出（共阳） | `RGB_LED_BLUE_PIN=8` | LOW=亮 |
| 蜂鸣器 | **GPIO5** | PWM 模式1 | `beep_channel=5` | 无源蜂鸣器 |
| SW-420 | **GPIO4** | 输入+中断 | `SW420_GPIO_PIN=4` | 低电平=检测到震动 |
| 震动马达 | **GPIO3** | 输出 | `VIBRATION_MOTOR_PIN=3` | 需 MOSFET 驱动 |
| KY-039 | ADC_CH2 | ADC | `KY039_ADC_CHANNEL=ADC_CHANNEL_2` | 备用心率（见注） |

> **KY-039 说明：** ADC_CHANNEL_2 来自旧项目（`嵌入式传感器/1.9/qrswork/lib/ky.c`），注释明确写 "3863芯片KY-039硬件对应通道2"。同学接线表未提到 KY-039，如实际接在其他 GPIO 需修改 `sensor/ky039.c` 中的 `KY039_ADC_CHANNEL` 宏。

### I2C 设备地址（芯片固定，无需修改）

| 设备 | 地址 | 驱动文件 |
|------|------|----------|
| SSD1315 OLED | 0x3C | output/ssd1306.c |
| MAX30102 | 0x57 | sensor/max30102.c |
| MPU6050 | 0x68 | sensor/mpu6050.c |

---

## 三、需要你确认/检查的事项

### 1. I2C 总线（优先级最高）

所有 I2C 操作都在 `system/i2c_master.c` 中，已实现：
- 引脚初始化（GPIO15/16，复用模式2，上拉）
- 总线恢复（9 个 SCL 脉冲 + STOP）
- 互斥锁保护（`i2c_master_lock()`/`unlock()`）
- 400KHz Fast Mode

**你需要确认：**
- [ ] OLED/MPU6050/MAX30102 的 SDA/SCL 是否确实接到 GPIO15/16
- [ ] I2C 上拉电阻是否焊接（一般开发板自带）

### 2. MAX30102 心率血氧

驱动在 `sensor/max30102.c`，关键流程：
1. 读取 Part ID (0xFF) 验证通信（期望 0x15）
2. 软件复位 (写 0x40 到 MODE_CONFIG)
3. 配置 FIFO/模式/LED 电流
4. 启动 10ms 定时器采样（TIMER_INDEX_0）

**你需要确认：**
- [ ] MAX30102 模块是否已焊接/连接
- [ ] I2C 地址是否为 0x57（默认值）

### 3. MPU6050 六轴 IMU

驱动在 `sensor/mpu6050.c`，关键流程：
1. 读取 WHO_AM_I (0x75) 验证（期望 0x68）
2. 配置加速度量程 ±4g，陀螺仪量程 ±500°/s
3. 启用卡尔曼滤波（q=0.01, r=0.5）

**你需要确认：**
- [ ] MPU6050 AD0 引脚是否悬空或接地（地址 0x68）
- [ ] 模块供电是否 3.3V

### 4. RGB LED（共阳接法）

驱动在 `output/rgb_led.c`，注意：
- **共阳接法：** GPIO LOW = 点亮，GPIO HIGH = 熄灭
- 软件 PWM：10ms 粒度，100ms 周期，TIMER_INDEX_1
- JTAG 不再冲突（GPIO13/14 已空出）

**你需要确认：**
- [ ] RGB 灯是共阳还是共阴（代码按共阳写，共阴需反转极性）
- [ ] R/G/B 分别接的 GPIO6/7/8

### 5. SW-420 震动传感器

驱动在 `sensor/sw420.c`，配置：
- GPIO4 输入模式 + 下降沿中断
- 低电平有效（检测到震动=LOW）
- GPIO4 是启动限制引脚，上电时不可被强拉高（输入模式安全）

### 6. 蜂鸣器

驱动在 `output/beep.c`：
- GPIO5，PWM 模式1
- 无源蜂鸣器（需 PWM 驱动发出声音）
- 有源蜂鸣器只需 GPIO 高低电平（如用有源需改代码）

### 7. 震动马达

驱动在 `output/vibration_motor.c`：
- GPIO3 输出，高电平开启
- **5V 供电**，需 MOSFET/三极管驱动 + 续流二极管保护
- 软件控制脉冲震动（200ms 开 / 2s 关）

---

## 四、初始化调用链

```
app_main.c: main_task()
  └─ health_monitor_init()
       ├─ [Mock跳过] all_i2c_init()           ← I2C总线 + OLED/MAX30102/MPU6050
       ├─ [Mock跳过] sw420_init()              ← GPIO4 输入+中断
       ├─ [Mock跳过] ky039_init()              ← ADC Channel 2
       ├─ [Mock跳过] rgb_led_init()            ← GPIO6/7/8 输出
       ├─ [Mock跳过] vibration_motor_init()    ← GPIO3 输出
       ├─ [Mock跳过] beep_init()               ← GPIO5 PWM
       ├─ fall_detection_init()                ← 纯算法，无硬件依赖
       ├─ attitude_init(0.96f)                 ← 纯算法，无硬件依赖
       ├─ breath_guide_init()                  ← 纯算法
       ├─ health_alert_init()                  ← 纯算法
       ├─ [Mock跳过] sle_comm_init()           ← SLE 星闪通信
       └─ [Mock跳过] ssd1306_*()               ← OLED 显示
```

---

## 五、编译与烧录

```
工具：HiSpark Studio VSCode 插件
SDK：E:\bearpi\bearpi-pico_h3863
目标：ws63-liteos-app
串口：CH340E USB，115200 波特率
日志：osal_printk() 输出
```

---

## 六、常见问题排查

| 现象 | 可能原因 | 排查方法 |
|------|----------|----------|
| I2C init 失败 | 引脚接错 / 无上拉 | 检查 GPIO15/16 接线，量上拉电压 |
| MAX30102 读 ID 失败 | 地址错 / 未焊接 | I2C 扫描 0x57 地址 |
| MPU6050 读 ID 失败 | AD0 高→地址 0x69 | 确认 AD0 悬空/接地 |
| OLED 花屏 | I2C 时序问题 | 降低波特率到 100KHz 测试 |
| RGB 灯不亮 | 共阴/共阳搞反 | 改 rgb_led.h 注释中的极性说明 |
| 蜂鸣器无声 | 有源/无源搞错 | 有源只需 GPIO 高低电平 |
