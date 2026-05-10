# 传感器数据分析（lib 同步版）

本文件夹包含传感器数据格式参考、模拟器和处理模板，与 `lib/` 中的实际代码保持同步。

## 文件说明

| 文件 | 功能 | 对应 lib 模块 |
|------|------|--------------|
| `sensor_data_analysis.md` | 传感器数据格式、阈值、算法完整文档（含硬件引脚） | 全局参考 |
| `sensor_simulator.h/c` | 传感器数据模拟器，输出格式与 lib 一致 | `health_monitor_main.c` |
| `sensor_data_template.c` | 数据结构定义 + 检测逻辑 + 格式化输出 | `health_alert.c` / `fall_detection.c` |

## JSON 输出格式

模拟器和模板的 JSON 输出与 lib 的 `send_data_to_serial()` 完全一致：

```json
{"hr":72,"spo2":98,"temp":36.5,"accel":[0.10,0.05,1.00],
 "gyro":[0.10,0.05,0.02],"fall_conf":0,"status":"Status: Normal",
 "hr_source":"max30102",
 "valid":{"hr":true,"spo2":true,"temp":true,"imu":true}}
```

## 使用方法

### 模拟器

```c
#include "sensor_simulator.h"

sensor_data_t sim;
char json[512];

sensor_sim_init();

// 正常数据
sensor_sim_normal(72, &sim);
sensor_sim_to_json(&sim, json, sizeof(json));
printf("%s\n", json);

// 模拟跌倒
sensor_sim_fall(&sim);
sensor_sim_to_json(&sim, json, sizeof(json));
printf("%s\n", json);
```

### 模板程序

```bash
gcc sensor_data_template.c -o template -lm
./template
```

输出示例：
```
[正常静息] {"hr":72,"spo2":98,"temp":36.5,"accel":[0.10,0.05,1.00],...}
[心率偏高] {"hr":105,"spo2":96,"temp":36.5,"accel":[0.10,0.05,1.00],...}
[跌倒冲击] {"hr":72,"spo2":98,"temp":36.5,"accel":[0.10,0.05,3.50],...,"fall_conf":100,"status":"[!!!] FALL!",...}
```

## 模拟场景

模板内置 8 个典型场景用于验证：

1. 正常静息 × 2
2. 心率偏高 (105, 110 BPM)
3. 恢复正常
4. 血氧过低 (85%)
5. 恢复正常
6. 跌倒冲击 (accel=3.5g, fall_conf=100)

## 与 lib 的对应关系

| 本文件夹 | lib 对应文件 | 说明 |
|---------|-------------|------|
| `sensor_data_t` | `health_monitor_main.c` 全局变量 | 数据结构 |
| `check_health_status()` | `health_alert.c:health_alert_update()` | 健康检测（含滞后） |
| `determine_alert_level()` | `health_alert.c:determine_alert_level()` | 报警分级 |
| `check_fall_detection()` | `fall_detection.c:fall_detection_update()` | 跌倒检测（多特征融合） |
| `format_serial_json()` | `health_monitor_main.c:send_data_to_serial()` | JSON 输出 |
