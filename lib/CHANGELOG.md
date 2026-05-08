# 更新日志 (Changelog)

---

## 当前状态总结（上次重写：2026-05-07，共 ~46 次修改）

**目录结构（05-06 重组）：**
```
lib/
├── app_main.c                     # 系统入口
├── system/    (i2c_master, system_utils)
├── sensor/    (max30102, mpu6050, ky039, sw420, data_filter)
├── output/    (ssd1306, rgb_led, beep, vibration_motor)
├── algorithm/ (fall_detection, breath_guide, health_alert)
├── comm/      (sle_comm)
└── app/       (health_monitor_main)
```

**硬件接线（05-07 按同学接线表更新）：**
```
I2C:  SCL=GPIO15, SDA=GPIO16（挂载 OLED SSD1315 + MAX30102 + MPU6050）
RGB:  R=GPIO6, G=GPIO7, B=GPIO8（共阳，低电平点亮）
蜂鸣器: GPIO5（无源，PWM）
震动传感器: GPIO4（低电平触发）
震动马达: GPIO3（高电平开启，需MOSFET驱动）
KY-039: ADC_CHANNEL_2（备用，MAX30102无效时自动切换）
```

**系统架构：**
```
app_main.c → main_task 主循环（单线程采样 MAX30102）
  ├── health_monitor_init() → all_i2c_init() + 各模块 init
  └── health_monitor_loop() → 状态机驱动
        ├── NORMAL: update_sensor_data() → 自动检测异常
        ├── ALERT: 持续监测，恢复自动退出
        └── BREATH_GUIDE: breath_guide_update()，完成自动停止
```

**JSON 输出：**
`{"hr":72,"spo2":98,"temp":36.5,"accel":[x,y,z],"gyro":[x,y,z],"fall_conf":0,"status":"Status: Normal","hr_source":"max30102","valid":{"hr":true,"spo2":true,"temp":true,"imu":true}}`

**已完成 3 轮优化 + 1 次重组 + 2 次 SDK 适配 + 1 次硬件重接，覆盖：**
- 分级报警 INFO/WARNING/DANGER + 滞后防抖 + 脉冲震动
- 跌倒检测多特征融合 + 冷却期 + 运动门槛
- 呼吸引导 3 轮自动停止 + HR 变化反馈
- 数据有效性 4 标记 + 5s 过期 + OLED "---"
- 状态机 NORMAL/ALERT/BREATH_GUIDE
- KY-039 备用心率 + hr_source 字段
- RGB LED 软件 PWM（0-100% 占空比，共阳极性）
- SLE 条件编译 #ifdef FEATURE_SLE
- I2C 总线互斥 + 恢复机制
- MAX30102 单线程采样 + 心率加权平均修复
- 目录分类重组（6 子目录）+ include 路径规范化
- SDK 适配：app_run() 入口 + osal_printk() + KY-039 ADC 修正
- 硬件注释：每个文件标注引脚假设和修改指引
- Bug 修复：snosal_printk→snprintf、timer_init 缺失、死代码清理

**已知未解决：** OLED 字符截断风险（行宽 18 字符）

---
> **使用规则：** 每次修改前只读上面「当前状态总结」，不用读下面的详细日志。每次修复后在下面对应类别追加条目。每累积约 10 次修改，重写总结。
---

## Bug 修复

### 05-07 硬件接线适配 + Bug 修复（6项）
| # | 文件 | 问题 | 修复 |
|---|------|------|------|
| 1 | 多个文件 | 引脚与同学实际接线不符（马达GPIO12→3, 传感器GPIO5→4, 蜂鸣器GPIO10→5, RGB GPIO13/14/2→6/7/8） | 按同学接线表更新所有引脚宏定义 |
| 2 | output/rgb_led.c + rgb_led.h | RGB灯实际为**共阳**接法（低电平点亮），代码按共阴编写 | 全文件反转 GPIO 电平（HIGH↔LOW），更新注释 |
| 3 | output/rgb_led.c | 缺少 `uapi_timer_init()`，直接调 adapter/create 会失败（参考 SDK timer_demo.c） | 添加 `uapi_timer_init()` + 防重复初始化标记 |
| 4 | output/rgb_led.c | `rgb_led_set_bright()` 中有死代码（else if 永远不执行）；`rgb_led_off()` 用 index 而非 handle 操作定时器 | 删除死代码；改用 `uapi_timer_stop(g_pwm_timer)` |
| 5 | app/health_monitor_main.c + algorithm/breath_guide.c | `snosal_printk` 函数不存在（SDK 中无定义），会导致**链接失败** | 全部替换为 `snprintf`，补充 `#include <stdio.h>` |
| 6 | 所有硬件文件 | 缺少硬件配置说明注释 | 在每个硬件相关文件添加引脚配置说明、修改指引 |

### 05-07 SDK 适配修复（11项）
| # | 文件 | 问题 | 修复 |
|---|------|------|------|
| 1 | app_main.c | 入口用 `void app_main()` + 手动 `osKernelInitialize/Start`，不符合 SDK 规范 | 改为 `app_run(entry)` 模式，任务签名改为 `void *fn(const char *arg)` |
| 2 | 12 个 .c 文件 | 全局用 `printf()`，嵌入式平台无 stdout 重定向时无输出 | 全部替换为 `osal_printk()`，补充 `#include "osal_debug.h"` |
| 3 | sensor/ky039.c | ADC 通道号错误；`uapi_adc_init()` 参数错误（传通道号应传时钟频率）；缺 `open_channel`/`power_en` | 重写：参考 qrswork 模板，init 用 `ADC_CLOCK_500KHZ`，补充 open/power 步骤 |
| 4 | system/i2c_master.c | SCL/SDA 引脚号写反（SCL=16/SDA=15，SDK 默认 SCL=15/SDA=16） | 交换引脚号，与 SDK Kconfig 默认值一致 |
| 5 | sensor/max30102.h | 重复声明 `get_time_ms()`（已移入 `system_utils.h`） | 删除重复声明 |
| 6 | sensor/max30102.c | `timer_init()!=1` 与 bool 比较不规范 | 改为 `!timer_init()` |
| 7 | app/health_monitor_main.c | 缺 `<string.h>`（用了 `strstr`/`memcpy`）；缺 `system_utils.h` | 补充 include |
| 8 | system/i2c_master.h | `CONFIG_I2C_MASTER_BUS_ID` 在 i2c_master.c 和 ssd1306.c 重复定义 | 统一到 i2c_master.h |
| 9 | sensor/ky039.h | `KY039_ADC_PIN` 定义多余且不准确 | 删除，ADC 通道定义统一在 .c 中 |
| 10 | output/rgb_led.c | **共阴 RGB 灯电平逻辑全部反了**（LOW=ON 应为 HIGH=ON） | 全文件反转 GPIO 电平：HIGH=亮，LOW=灭 |
| 11 | system/i2c_master.c | I2C 波特率 100KHz 偏低 | 改为 400KHz（Fast Mode），与 SDK 示例一致 |

### 05-06 缺陷修复（6项）
| # | 文件 | 问题 | 修复 |
|---|------|------|------|
| 1 | app/health_monitor_main.c | `sle_data_callback()` 使用前未声明 | 添加前向声明 |
| 2 | algorithm/breath_guide.c + app/ | `breath_guide_stop()` 的 `g_end_hr` 永远为 0 | 签名改为接收 `current_hr` 参数 |
| 3 | algorithm/fall_detection.c | `fall_detection_cancel_alert()` 不清除冷却期 | 增加 `g_in_cooldown = false` |
| 4 | app/health_monitor_main.c | 体温有效性 `!= 0` 判断无效（公式始终非零） | 改用 `mpu6050_read_processed()` 返回值 |
| 5 | output/rgb_led.c | `rgb_led_set_bright(0,0,0)` 不关 GPIO | 全零时立即置 HIGH + 停定时器 |
| 6 | app/health_monitor_main.c | `health_monitor_init()` SLE 失败阻塞启动 | SLE 失败不设 `init_ok = false` |

### 05-01 并发优化（9项）
| # | 问题 | 修复 |
|---|------|------|
| 1 | MAX30102 双线程重复读 FIFO | 改为 main_task 单线程采样，health_monitor 只消费 `return_ac[]` |
| 2 | I2C 互斥锁只创建不使用 | `i2c_master_lock()`/`unlock()` + 设备驱动加锁 |
| 3 | 跌倒回调签名 `fall_state_t` vs `bool` | 新增 `fall_state_callback()` 适配函数 |
| 4 | 震动脉冲不自动关断 | 主循环补充 `vibration_motor_update()` |
| 5 | 初始化失败仍返回成功 | `init_ok` 聚合状态，关键模块失败返回 false |
| 6 | `ALERT_MODE_ALL` 缺蜂鸣器 | 增加 `beep_time(80)` |
| 7 | `ssd1306_FillBuffer` 拷贝越界 | 目的长度改为 `SSD1306_BUFFER_SIZE` |
| 8 | `beep.h` 缺 `<stdbool.h>` | 补充 include |
| 9 | 跌倒检测运动场景误报 | 运动中置信度 <80 不进入异常判定 |

### 04-30 早期 Bug 修复
| # | 问题 | 修复 |
|---|------|------|
| 1 | I2C 总线无互斥保护 → 花屏/数据错乱 | `osMutexNew()` 创建 + 提供 lock/unlock API |
| 2 | 缺少系统入口 `app_main` | 新建 `app_main.c`，创建任务 + `osKernelStart()` |
| 3 | `beep.h`/`max30102.h` 冗余 include | 依赖下沉到 .c 文件 |
| 4 | I2C 总线挂死无恢复 | 新增 `i2c_bus_recovery()`（9 个 SCL 脉冲 + STOP） |
| 5 | sensor_task 在 I2C 初始化前运行 | 改为初始化成功后才启动 |
| 6 | `return_ac` 静态变量链接错误 | `static` → `extern` + 源文件定义 |
| 7 | `fall_detection.h` 缺函数声明 | 补充 `fall_detection_is_moving()` 声明 |
| 8 | CMakeLists.txt 缺源文件 | 每次新增 .c 同步更新 SOURCES_LIST |

---

## 算法优化

### MAX30102 心率检测（04-30 + 05-01）
- 自适应阈值：每 100 采样点用信号均值 60% 更新（范围 20-100）
- 加权平均：按时间距离加权，最新样本权重最高（`newest_idx`）
- 变化验证：单次变化 >20% 视为噪声忽略
- 滤波代码去重：删除本地 `moving_average_filter()`，复用 `data_filter.h`

### 跌倒检测融合算法（04-30 + 05-01）
- 多特征加权：跌倒特征 50% + 加速度 25% + 倾斜角 15% + 震动 10%
- 运动状态检测：加速度方差判断，运动中提高门槛
- 持续时间过滤：置信度 ≥60% 且持续 3 秒才判定
- 冷却期 10s + 30s 超时自动取消

### 数据滤波算法库（04-30）
- `sensor/data_filter.c/.h`：卡尔曼滤波 + 滑动平均
- MPU6050：6 轴卡尔曼（q=0.01, r=0.5）
- MAX30102：PPG 4 点滑动平均

---

## 功能新增

### 分级报警系统（05-01）
- `algorithm/health_alert.c` 完全重写
- INFO（视觉）→ WARNING（视觉+脉冲震动）→ DANGER（全模式+蜂鸣）
- 滞后防抖：HR ±5BPM、SpO2 ±2%
- 静音 30s 自动恢复，新异常自动解除

### 呼吸引导智能控制（05-01）
- `algorithm/breath_guide.c`：3 轮自动停止 + HR 变化反馈
- `breath_guide_stop(current_hr)` 接收当前心率计算 delta

### 数据质量评估（05-01）
- 4 个有效性标记：`g_hr_valid`, `g_spo2_valid`, `g_temp_valid`, `g_imu_valid`
- 心率 5s 过期自动标记无效，OLED 显示 "---"
- JSON 新增 `valid` 字段（缓冲区 256→320 字节）

### 系统状态机（05-01）
- NORMAL / ALERT / BREATH_GUIDE 独立行为分支
- `state_transition()` 自动打印状态变化日志
- 状态驱动的 OLED 显示和按键处理

### KY-039 备用心率（05-01）
- MAX30102 无效时自动切换到 KY-039
- JSON 新增 `"hr_source":"max30102"/"ky039"` 字段

### RGB LED 软件 PWM（05-01）
- `output/rgb_led.c`：`rgb_led_set_bright(r, g, b)` 占空比 0-100%
- 10ms 粒度，100ms 周期，TIMER_INDEX_1 驱动

### SLE 条件编译（05-01）
- `comm/sle_comm.c`：`#ifdef FEATURE_SLE` 包裹，无 SDK 空实现退化

### 姿态解算模块（05-07）
- 新建 `algorithm/attitude_estimation.c/.h`：互补滤波融合加速度计+陀螺仪
- 输出 Pitch/Roll 欧拉角，用于跌倒检测姿态判定
- α=0.96 默认值，响应快且适合瞬时姿态突变场景

### 姿态解算接入跌倒检测（05-07）
- `health_monitor_main.c`：init 中调用 `attitude_init(0.96f)`，update 中调用 `attitude_update()`
- `fall_detection.c`：`fall_detection_update()` 新增 pitch/roll 参数

### MOCK_HARDWARE_MODE 硬件模拟 + JSON 扩展（05-07）
- `health_monitor_main.h`：新增 `MOCK_HARDWARE_MODE` 宏（默认1=模拟模式）
- `health_monitor_main.c`：init/update/loop 三处加 `#if` 分支，Mock 模式跳过 I2C/GPIO/OLED/SLE
- `health_monitor_main.c`：新增 `mock_generate_data()` — 心率/血氧/温度/加速度/陀螺仪正弦波动
- `app_main.c`：MAX30102 采样和 RGB LED 状态指示加 Mock 分支
- `send_data_to_serial()` + `health_monitor_send_data()`：JSON 新增 `pitch`、`roll`、`fall_alert` 字段
- 算法层（attitude/fall_detection/health_alert）零改动，Mock 数据经过完整算法链路
- 倾斜角计算从纯加速度 atan2 改为取 Pitch/Roll 绝对值较大者
- 优势：融合陀螺仪后抗振动噪声更强，瞬时响应更快

### 北端模板前后端对齐（05-07）
- `sensor_simulator.h`：`sensor_data_t` 新增 `pitch`、`roll`、`fall_alert` 字段
- `sensor_simulator.c`：`sensor_sim_generate()`/`to_json()`/`to_csv()`/`fall()` 等全部补齐新字段
- `sensor_data_template.c`：`sensor_all_data_t` 新增 `pitch`、`roll`、`fall_alert`；`format_serial_json()` 对齐 lib JSON 格式；`check_fall_detection()` 改用 pitch/roll 计算倾斜角；测试场景补充 pitch/roll 值
- `sensor_data_analysis.md`：JSON 示例和字段说明表新增 pitch/roll/fall_alert
- 前后端 JSON 字段顺序、类型、含义完全一致

---

## 构建系统 & 代码规范化

### 05-06 VSCode IntelliSense 配置
- 创建 `.vscode/c_cpp_properties.json`，配置 SDK 路径 `E:/bearpi/bearpi-pico_h3863/**`
- 添加 `FEATURE_SLE` 宏定义
- 修复子目录自引用路径重复（14 个文件）：子目录内 `.c` 自引用 `.h` 不加前缀，跨目录加前缀

### 05-06 目录分类重组
- 17 个文件按功能分入 6 个子目录（system/sensor/output/algorithm/comm/app）
- 所有项目头文件 `#include` 加子目录前缀，SDK 头文件不变
- `i2c_master.h` 清理：移除不属于 I2C 模块的 include
- `CMakeLists.txt` SOURCES_LIST 路径同步更新

### 05-06 CMakeLists + get_time_ms 规范化
- `CMakeLists.txt`：修复 `${SOURCES}` 空引用 → `set(SOURCES ${SOURCES_LIST} PARENT_SCOPE)`
- `system/system_utils.h`：统一声明 `get_time_ms()`，6 个文件删除重复 `extern`

### 04-30 I2C 初始化改进
- 每步加 `[I2C] StepN: ...` 诊断打印
- 引脚复用模式 `I2C_MASTER_PIN_MODE = 2`（已确认正确，总线恢复临时切 mode 0）

---

## 修改注意事项

1. **新增 .c 文件** → 同步更新 `CMakeLists.txt` SOURCES_LIST
2. **新增 .h 文件** → 检查调用方 `#include` 路径（项目头文件需加子目录前缀）
3. **修改 I2C 设备** → 注意互斥锁保护（`i2c_master_lock()`/`unlock()`）
4. **修改传感器阈值** → 对应头文件 `#define` 常量
5. **修改 OLED 显示** → 坐标范围 128x64，Font_7x10 每行约 18 字符
6. **修改 JSON 格式** → 字段名需与上位机 `serial_plot.py` 的 STATUS_MAP 对齐
7. **MAX30102 采样** → 必须先 `is_time()` 再 `main_max30102_data()`
8. **SLE 编译** → 未定义 `FEATURE_SLE` 时退化为空实现
9. **SDK 入口** → 必须用 `app_run(entry_fn)` 宏，不能用 `void app_main()`
10. **日志输出** → 统一用 `osal_printk()`，不用 `printf()`
11. **KY-039 ADC** → 通道 2（`ADC_CHANNEL_2`），init 参数是时钟频率不是通道号
12. **I2C 引脚** → SCL=GPIO15, SDA=GPIO16（SDK Kconfig 默认值，不可随意改）
13. **RGB 灯** → 共阳接法（LOW=亮），引脚 R=GPIO6, G=GPIO7, B=GPIO8
14. **定时器使用** → 首次使用必须先调 `uapi_timer_init()`（参考 SDK timer_demo.c）
15. **字符串格式化** → 用 `snprintf()` + `<stdio.h>`，不用 `snosal_printk`（SDK 无此函数）
16. **硬件引脚修改** → 每个文件顶部注释标注了引脚假设，改接线时按注释修改对应宏
