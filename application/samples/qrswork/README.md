# Qrswork 心跳数据传输系统

## 项目概述

本项目实现了基于 SLE (Star Flash Low Energy) 的心跳数据无线传输系统，包含服务端和客户端两部分：

- **服务端 (qrswork)**: 读取心跳传感器数据，通过 SLE 发送给客户端
- **客户端 (qrswork_client)**: 接收服务端发送的心跳数据，并通过串口输出

## 目录结构

```
application/samples/
├── qrswork/                           # 服务端工程
│   ├── qrswork.c                      # 主程序文件
│   ├── qrswork.h                      # 头文件
│   ├── Kconfig                        # 配置文件
│   ├── CMakeLists.txt                 # CMake构建文件
│   ├── lib/                           # 硬件驱动库
│   │   ├── led.c/h                    # LED驱动
│   │   ├── rgb.c/h                    # RGB灯驱动
│   │   └── ky.c/h                     # 心跳传感器驱动
│   └── qrswork_server/                # SLE服务端实现
│       ├── qrswork_server.c           # 服务端核心逻辑
│       ├── qrswork_server.h           # 服务端头文件
│       ├── qrswork_server_adv.c       # 广播实现
│       └── qrswork_server_adv.h       # 广播头文件
│
└── qrswork_client/                    # 客户端工程
    ├── qrswork_client_main.c          # 客户端主程序
    ├── qrswork_client.c               # 客户端实现
    ├── qrswork_client.h               # 客户端头文件
    ├── Kconfig                        # 配置文件
    ├── CMakeLists.txt                 # CMake构建文件
    └── board.json                     # 板级配置
```

## 编译和烧录

### 编译服务端 (qrswork)

```bash
cd /home/bearpi/Documents/bearpi-pico_h3863-master
./build.py menuconfig ws63-liteos-app

# 在 menuconfig 中选择:
# Application  --->
#   [*] Enable Sample
#     Sample  --->
#       [*] Enable qrswork Sample
#         (X) Enable qrswork Server (Heart rate sender)

# 保存并退出

./build.py ws63-liteos-app
```

### 编译客户端 (qrswork_client)

```bash
cd /home/bearpi/Documents/bearpi-pico_h3863-master
./build.py menuconfig ws63-liteos-app

# 在 menuconfig 中选择:
# Application  --->
#   [*] Enable Sample
#     Sample  --->
#       [*] Enable qrswork_client Sample

# 配置UART引脚 (可选):
# QRSWORK_UART_TXD_PIN = 17
# QRSWORK_UART_RXD_PIN = 18

# 保存并退出

./build.py ws63-liteos-app
```

### 烧录固件

服务端和客户端需要分别烧录到两个设备上。

## 功能说明

### 服务端功能

1. **硬件初始化**: 初始化 LED、RGB 灯和心跳传感器
2. **SLE 服务初始化**: 创建 SLE 服务端，开启广播
3. **心跳检测任务**: 
   - 每 100ms 读取一次心跳 ADC 值
   - 根据心跳状态控制 RGB 灯显示
   - 通过 SLE 连接向客户端发送心跳数据
4. **数据格式**: 发送 4 字节数据 (uint32_t 小端序)，包含心跳 ADC 原始值

### 客户端功能

1. **SLE 初始化**: 启动 SLE 扫描，查找服务端设备
2. **自动连接**: 扫描到服务端后自动连接
3. **接收数据**: 接收服务端发送的心跳数据
4. **串口输出**: 将接收到的心跳数据通过串口输出
   - 原始 ADC 值
   - 计算后的心率 BPM 值 (60000 / ADC)

### 输出示例

**服务端输出:**
```
Init task start...
Led init successful
RGB init successful
KY init successful
Initializing SLE server...
SLE server init successful
All hardware init success!
Heart detect task create success!
Heart detect task start running...
Heart ADC raw val: 500 heart val: 120
Heart data sent via SLE
```

**客户端输出:**
```
qrswork client task start
waiting for connection and heart data...
sle enable: 0.
scan data: qrswork_heart_server
SLE connected
Heart rate data received: ADC=500, Heart Rate=120 BPM
```

## 配置说明

### SLE 参数

- **服务名称**: `qrswork_heart_server`
- **Service UUID**: 0x3333
- **Property UUID**: 0x3434
- **广播间隔**: 25ms
- **连接间隔**: 12.5ms

### UART 配置 (客户端)

- **波特率**: 115200
- **数据位**: 8
- **停止位**: 1
- **校验位**: 无
- **默认引脚**: TXD=17, RXD=18

## 注意事项

1. 服务端和客户端必须烧录到不同的设备上
2. 确保两个设备都支持 SLE 功能
3. 服务端启动后会自动开启广播
4. 客户端会自动扫描并连接到服务端
5. 断线后客户端会自动重新扫描连接

## 故障排查

### 客户端无法连接

- 检查服务端是否正常启动并开启广播
- 确认 SLE 功能已启用
- 查看日志中的扫描结果

### 无法接收数据

- 确认 SLE 连接状态
- 检查服务端心跳传感器是否正常工作
- 查看服务端是否成功发送数据

### 串口无输出

- 检查 UART 配置是否正确
- 确认串口工具连接参数匹配 (115200, 8N1)
- 检查引脚配置是否正确

## 开发参考

本项目参考了 `sle_uart` 示例的架构和实现方式，主要修改点：

1. 服务端集成了硬件传感器读取功能
2. 客户端专注于接收和显示心跳数据
3. 使用独立的 UUID 避免与其他服务冲突
4. 优化了数据传输格式和显示方式
