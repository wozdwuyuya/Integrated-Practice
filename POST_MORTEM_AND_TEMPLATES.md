# 综合实训技术复盘报告：BearPi-Hi3863 嵌入式健康监测系统

> 课程名称：综合实训 Grade 3
> 报告日期：2026-05-12
> 项目仓库：https://github.com/wozdwuyuya/Integrated-Practice.git

---

## 一、项目概述

本项目基于 BearPi-Hi3863 开发板，设计并实现了一套智能健康监测系统。系统集成
多种传感器（MPU6050 六轴 IMU、KY-039 心率、SW-420 振动检测）、OLED 显示、
RGB/蜂鸣器/振动马达输出、星闪（SLE）通信及 WiFi TCP 数据传输等功能模块，
采用 CMSIS-RTOS2 实时操作系统进行多任务调度。

---

## 二、总线安全与 RTOS 线程安全技术总结

### 2.1 问题背景

系统中 OLED 显示屏（SSD1306，I2C 地址 0x3C）与六轴惯性测量单元（MPU6050，
I2C 地址 0x68）共用同一条 I2C 总线（GPIO15/16，I2C1，400KHz Fast Mode）。

RTOS 线程架构如下：

| 线程名称 | 优先级 | 职责 | I2C 访问 |
|----------|--------|------|----------|
| MainTask | 17 | 传感器采集、算法处理、OLED 刷新 | MPU6050 读取 + SSD1306 写入 |
| TCPServerTask | 12 | WiFi AP 管理、TCP 数据推送 | 无直接 I2C 访问 |

由于两个 I2C 设备共享总线且分属不同的调用路径，若缺乏有效的总线仲裁机制，
将产生以下风险：总线冲突、数据撕裂、线程死锁。

### 2.2 改造方案与实施

#### 2.2.1 全局互斥锁基础架构

在 `lib/system/i2c_master.c` 中引入全局互斥锁 `g_i2c_bus_mutex`，
通过 `i2c_master_lock()` / `i2c_master_unlock()` 接口封装，
SSD1306 驱动（`lib/output/ssd1306.c`）和 MPU6050 驱动（`lib/sensor/mpu6050.c`）
均在每次 I2C 事务前后调用锁接口。

#### 2.2.2 超时机制与运行时总线恢复

原始实现中 `osMutexAcquire()` 使用 `osWaitForever` 参数，
当 I2C 总线物理层异常（如 SDA 被从设备持续拉低）时，持有锁的线程将永久阻塞，
其他等待锁的线程随之死锁。

改造方案：将超时参数从 `osWaitForever` 改为 500ms（`I2C_LOCK_TIMEOUT_MS`）。
超时触发后，通过 GPIO 模拟 SCL 时钟脉冲（最多 9 个脉冲）尝试释放总线，
随后生成 STOP 条件并恢复 I2C 引脚复用模式，最后重试一次锁获取。

改动文件：`lib/system/i2c_master.c`，提交记录：`a58a40c`。

#### 2.2.3 OLED 刷新操作级互斥

原始设计中，`ssd1306_UpdateScreen()` 每次底层 I2C 传输独立加锁/解锁。
一次完整的 OLED 刷新（8 页 x 4 次 I2C 事务 = 32 次锁操作）期间，
MPU6050 的读取请求可在任意两次锁操作之间插入，导致 OLED 刷新时间被拉长。

改造方案：在 SSD1306 驱动中新增 `ssd1306_UpdateScreen_locked()` 变体函数，
内部使用不经由互斥锁的底层写入接口（`ssd1306_i2c_write_raw()`）。
业务层 `update_oled_display()` 函数在缓冲区操作完成后，统一持锁调用
`ssd1306_UpdateScreen_locked()` 完成整屏刷新。

实施过程中发现了一个关键设计问题：若在调用方直接加锁包裹
`update_oled_display()`，而函数内部的 `ssd1306_SendData()` 又会尝试获取同一把
非递归互斥锁，将导致同线程死锁。解决方案是提供 `_locked` 后缀的底层变体函数，
绕过驱动内部的锁机制，由外层统一管理总线互斥。

改动文件：`lib/output/ssd1306.c`、`lib/output/ssd1306.h`、`lib/app/health_monitor_main.c`，
提交记录：`8aa7fb2`。

#### 2.2.4 跨线程数据快照同步

TCPServerTask 通过 `data_fusion_build_json()` 函数读取主线程维护的全局传感器数据
（心率、血氧、加速度、陀螺仪等）并构建 JSON 字符串推送至客户端。
由于读取和写入操作分属不同线程且无同步保护，JSON 输出可能包含不一致的数据快照。

改造方案：引入独立的数据快照互斥锁 `g_data_mutex`（与 I2C 总线锁分离，避免锁竞争）。
主线程在数据发送路径中持锁，TCP 线程在 `data_fusion_build_json()` 内部持锁，
确保任一时刻只有一个线程访问传感器数据。

改动文件：`lib/app/health_monitor_main.c`、`lib/app/health_monitor_main.h`，
提交记录：`d2effd6`。

### 2.3 改造前后对比

| 指标 | 改造前 | 改造后 |
|------|--------|--------|
| I2C 总线保护 | 无 | 全局互斥锁 + 超时恢复 |
| 锁超时策略 | `osWaitForever`（永久阻塞） | 500ms 超时 + GPIO 总线恢复 + 重试 |
| OLED 刷新原子性 | 单事务锁（可被穿插） | 操作级锁（整屏持锁） |
| 同线程死锁风险 | 无（无锁） → 引入锁后存在 | `_locked` 变体规避 |
| 跨线程数据一致性 | 无保护 | 独立数据快照互斥锁 |

### 2.4 关键源文件索引

| 文件路径 | 职责 |
|----------|------|
| `lib/system/i2c_master.c` | I2C 总线初始化、互斥锁管理、总线恢复 |
| `lib/system/i2c_master.h` | I2C 锁接口声明 |
| `lib/output/ssd1306.c` | SSD1306 OLED 驱动（含 `_locked` 变体） |
| `lib/output/ssd1306.h` | SSD1306 公共接口声明 |
| `lib/sensor/mpu6050.c` | MPU6050 IMU 驱动 |
| `lib/app/health_monitor_main.c` | 系统主循环、数据融合、状态机 |
| `lib/app/health_monitor_main.h` | 系统接口声明（含数据锁接口） |

---

## 三、项目开发阶段总结

### 3.1 Phase 1：传感器驱动与算法集成

完成 MPU6050（六轴 IMU）、KY-039（心率 ADC）、SW-420（振动检测）、
MAX30102（血氧）等传感器驱动开发。集成跌倒检测、呼吸引导、健康告警、
姿态估计（互补滤波）等算法模块。实现 SSD1306 OLED 显示、RGB LED、
蜂鸣器、振动马达等输出设备驱动。

### 3.2 Phase 2：WiFi AP 模式与网络通信

实现 WiFi SoftAP 热点初始化（SSID: HealthMonitor），配置 DHCP 服务器，
为 Android 客户端提供无线接入能力。

### 3.3 Phase 3：TCP 数据传输与命令处理

实现 TCP Server 后台任务，支持 JSON 格式传感器数据定时推送（1s 周期）
和客户端命令接收处理。集成 cJSON 库实现结构化数据序列化。

### 3.4 安全加固阶段

针对 I2C 总线冲突和跨线程数据竞争问题，实施三轮安全改造（详见第二章）。

---

## 四、项目统计

| 指标 | 数据 |
|------|------|
| 代码总量 | ~3000 行 C（lib/ 目录） |
| 传感器驱动 | MPU6050, KY-039, SW-420, MAX30102 |
| 输出设备 | SSD1306 OLED, RGB LED, 蜂鸣器, 振动马达 |
| 通信协议 | 星闪（SLE）+ WiFi TCP（SoftAP） |
| RTOS 线程数 | 2（MainTask + TCPServerTask） |
| Android 客户端 | Java/Gradle，6 个 Activity |
| 测试工具 | Python 串口监控 + 数据工厂 |
