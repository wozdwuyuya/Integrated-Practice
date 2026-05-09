# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/lang/zh-CN/).

---

## [v1.0.0-alpha] - 2026-05-10

### 架构升级：lib/ 目录迁移 + WiFi 北向传输修复

本次更新包含 4 个原子提交，覆盖代码缺陷修复、目录架构重构、构建系统联动和文档补全。

---

### 工程协作提醒

> 以下 3 条注意事项适用于所有组员，请在 `git pull` 后优先阅读。

| # | 类别 | 要点 | 操作 |
|---|------|------|------|
| 1 | 🔧 硬件模拟开关 | `QRSWORK_MOCK_HARDWARE` 默认开启，传感器输出假数据 | 实物调试前必须 `menuconfig` 关闭 |
| 2 | ⚠️ 引脚复用预检 | Hi3863 存在 Pinmux 复用，新增外设前需查表 | 当前已占用 GPIO3/4/5/6/7/8/15/16/17/18 |
| 3 | 🧹 清理编译 | lib/ 目录迁移后 CMake 缓存路径失效 | `git pull` 后必须先 `./build.py clean` 再编译 |

---

### Fixed

#### WiFi 北向传输 P0/P1 级别缺陷（commit `2f82fcd`）

**P0 级（必须修复）：**

| 缺陷 | 文件 | 修复方案 |
|------|------|---------|
| Socket 泄漏：`memset_s` 失败时未关闭已打开的 socket | `northbound_client.c` | 新增 `northbound_client_close_socket()` 防御性关闭函数，任何步骤失败都会回滚 |
| WiFi 断线后无法恢复：北向任务在旧 socket 上持续 `sendto` 失败 | `northbound_client.c` + `wifi_connect.c` | 新增 `MSG_RECONNECT` 消息类型 + `northbound_client_notify_wifi_disconnect()` 断线通知接口，WiFi 回调通过消息队列触发 socket 重建 |
| DHCP 状态检查返回类型错误：声明 `td_bool` 实际返回 `0/-1` | `wifi_connect.c` | 修正返回类型为 `int`，消除隐式类型转换 |

**P1 级（建议修复）：**

| 缺陷 | 文件 | 修复方案 |
|------|------|---------|
| 任务优先级 27 过高，会抢占 SLE 回调和系统任务 | `northbound_client.c` | 修正为 13，与 SDK 官方 `udp_client.c` 范例对齐 |
| 跨上下文共享变量缺少 `volatile` 修饰 | `northbound_client.c` + `wifi_connect.c` | `g_qrswork_wifi_state`、`g_qrswork_nb_queue_ready`、`g_qrswork_nb_task_started` 添加 `volatile` |
| `unused()` 小写宏未定义，SDK 使用大写 `UNUSED()` | `northbound_client.c` | 修正为 `UNUSED(arg)` |

#### 迁移后路径修复（commit `37003c9`）

| 缺陷 | 文件 | 修复方案 |
|------|------|---------|
| `health_monitor_main.c` 引用旧路径 `comm/sle_comm.h` | `lib/app/health_monitor_main.c` | 修正为 `sle_comm.h`（文件已从 `lib/comm/` 迁移至 `lib/`） |

---

### Changed

#### lib/ 目录架构重构（commit `d7bce05`）

将原项目根目录 `lib/` 下的全部模块迁移至 `application/samples/qrswork/lib/`，按功能分类存放：

```
qrswork/lib/
├── sensor/              # 传感器驱动
│   ├── max30102.c/h     #   心率+血氧传感器 (I2C 0x57)
│   ├── mpu6050.c/h      #   六轴加速度/陀螺仪 (I2C 0x68)
│   ├── ky039.c/h        #   心跳传感器 (ADC Channel 2)
│   ├── sw420.c/h        #   振动传感器 (GPIO4)
│   └── data_filter.c/h  #   卡尔曼滤波 + 滑动平均
├── output/              # 输出设备驱动
│   ├── ssd1306.c/h      #   OLED 显示屏 (I2C 0x3C)
│   ├── ssd1306_fonts.c/h#   字体数据
│   ├── rgb_led.c/h      #   RGB LED (GPIO6/7/8, 共阳)
│   ├── beep.c/h         #   蜂鸣器 (GPIO5, PWM)
│   └── vibration_motor.c/h # 振动马达 (GPIO3)
├── algorithm/           # 算法模块
│   ├── fall_detection.c/h   # 跌倒检测（多特征融合）
│   ├── breath_guide.c/h     # 呼吸引导（3轮自动停止）
│   ├── health_alert.c/h     # 分级健康告警（INFO/WARNING/DANGER）
│   └── attitude_estimation.c/h # 姿态估算（互补滤波）
├── system/              # 系统底层驱动
│   └── i2c_master.c/h   #   I2C 总线驱动 (SCL=GPIO15, SDA=GPIO16)
├── app/                 # 应用主循环
│   └── health_monitor_main.c/h # 健康监测主任务 + 状态机
├── sle_comm.c/h         # SLE 通信封装
└── app_main.c           # 系统入口 (app_run 模式)
```

**迁移原则：**
- 硬件引脚定义严格保持小熊派原始定义，不做任何修改
- 所有 `.h` 文件顶部标注：`// 协作提示：此文件已由 lib/ 迁移，硬件引脚严格保持小熊派原始定义。`
- 原 `lib/` 根目录保留作为备份，待团队确认后删除

#### CMake 构建系统联动（commit `37003c9`）

**CMakeLists.txt 变更：**
- 新增 7 条 `include_directories`：
  - `${CMAKE_CURRENT_SOURCE_DIR}/lib` — 支持 `#include "sensor/max30102.h"` 等相对路径
  - `${CMAKE_CURRENT_SOURCE_DIR}/lib/sensor` — 支持 `#include "max30102.h"` 直接引用
  - `${CMAKE_CURRENT_SOURCE_DIR}/lib/output` — 支持 `#include "ssd1306.h"` 直接引用
  - `${CMAKE_CURRENT_SOURCE_DIR}/lib/system` — 支持 `#include "i2c_master.h"` 直接引用
  - `${CMAKE_CURRENT_SOURCE_DIR}/lib/algorithm` — 支持 `#include "fall_detection.h"` 直接引用
  - `${CMAKE_CURRENT_SOURCE_DIR}/lib/app` — 支持 `#include "health_monitor_main.h"` 直接引用
  - `${CMAKE_CURRENT_SOURCE_DIR}/qrswork_server` — 支持 SLE Server 头文件
- `SOURCES_LIST` 新增 16 个迁移源文件（SERVER 和非 SERVER 分支同步更新）

**Kconfig 变更：**
- 新增 `QRSWORK_MOCK_HARDWARE` 配置项：
  ```
  config QRSWORK_MOCK_HARDWARE
      bool "Enable mock hardware mode (use fake sensor data for debugging)"
      default y
  ```
- `health_monitor_main.h` 更新为优先从 `CONFIG_QRSWORK_MOCK_HARDWARE` 读取，未配置时回退到 `#ifndef` 默认值

#### WiFi 连接模块代码注释（commit `1aca75b`）

- `northbound_client.c`：补充详细中文注释，标注 `[AI 优化]` 改动点和原教程逻辑位置
- `wifi_connect.c`：补充 WiFi 状态机流程说明、SDK `sta_sample.c` 对照、`volatile` 设计决策

---

### Added

#### 团队协作规范（commit `84832b2`）

- 新增 `CONTRIBUTING.md`：仓库上传纪律、分支命名规范（feature/fix/docs/test）、原子化提交要求、编译环境对齐
- 新增 `.gitignore` 规则：排除 `.claude/settings.local.json`（含个人路径）

#### README 目录结构更新（commit `1aca75b`）

- `qrswork/README.md` 目录结构树从 8 行扩展至 35 行，完整展示 lib/ 下 5 个子目录及所有文件
- 新增注意事项：硬件模拟模式说明、lib/ 迁移说明
- 同步更新客户端工程目录（northbound_client、wifi 模块）

#### 参考文档迁移（commit `d7bce05`）

- `docs/HARDWARE_REFERENCE.md` — 硬件接线参考
- `docs/SYSTEM_README.md` — 系统架构说明
- `docs/项目展望与开发方向.md` — 后续规划

---

### 组员同步指南

```bash
# 1. 拉取最新代码
git pull origin main

# 2. 确认目录结构
ls application/samples/qrswork/lib/

# 3. 编译验证（SDK 环境中）
./build.py menuconfig ws63-liteos-app   # 可选：配置 MOCK_HARDWARE_MODE
./build.py ws63-liteos-app

# 4. 如遇 include 报错，确认 CMakeLists.txt 已包含 include_directories
```

**注意事项：**
- `MOCK_HARDWARE_MODE` 默认开启（使用假数据调试），生产环境请在 menuconfig 中关闭 `QRSWORK_MOCK_HARDWARE`
- 原 `lib/` 根目录保留作为备份，确认无误后可删除
- 新增 `.c` 文件需同步更新 `CMakeLists.txt` 的 `SOURCES_LIST`

---

## [历史记录] - 2026-04-30 ~ 2026-05-07

> 以下记录迁移自 `lib/CHANGELOG.md`，保留完整历史。详见 [lib/CHANGELOG.md](lib/CHANGELOG.md)。

### 已完成的优化轮次

- **3 轮算法优化** + **1 次目录重组** + **2 次 SDK 适配** + **1 次硬件重接**
- 覆盖：分级报警、跌倒检测、呼吸引导、数据有效性、状态机、KY-039 备用心率、RGB 软件 PWM、SLE 条件编译、I2C 互斥、MAX30102 单线程采样、姿态解算等

### 已知未解决

- OLED 字符截断风险（行宽 18 字符）

---

[v1.0.0-alpha]: https://github.com/wozdwuyuya/Integrated-Practice/compare/v1.0.0-alpha...HEAD
