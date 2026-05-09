# qrswork_client WiFi 北向传输说明

## 目标

`qrswork_client` 作为 SLE Client 接收下位心率节点的数据，并通过 WiFi 连接到电脑或上位服务器，为后续北向应用开发预留统一入口。

当前实现采用“板子连接 WiFi 热点/路由器”的 STA 模式，板子和电脑处于同一局域网后，板子将 SLE 收到的数据封装成 UDP JSON 行发送到电脑。相比板子开启热点，STA 模式更适合当前开发阶段：电脑无需切换网络，调试工具、代码仓库和上位机程序可以继续使用原有网络，后续接入路由器、手机热点或实验室网络也更自然。

## 文件结构

```text
application/samples/qrswork_client/
├── qrswork_client.c          # SLE 通知回调，收到数据后发布到北向队列
├── qrswork_client_main.c     # client 任务入口，启动北向任务和 SLE client
├── northbound_client.c       # UDP 北向传输任务与对外发布接口
├── northbound_client.h       # 北向接口声明
├── wifi/
│   ├── wifi_connect.c        # WiFi STA 扫描、连接、DHCP 获取 IP
│   └── wifi_connect.h
├── CMakeLists.txt            # 按 Kconfig 开关加入 WiFi/北向源码
└── Kconfig                   # WiFi 与北向服务器配置项
```

## 组网方式

推荐连接方式：

```text
qrswork server 节点 --SLE--> qrswork_client 板子 --WiFi/UDP--> 电脑或北向服务器
```

要求：

- 板子连接的 WiFi 与电脑处在同一局域网。
- `CONFIG_QRSWORK_SERVER_IP` 配置为电脑 WiFi 网卡的 IPv4 地址。
- 电脑防火墙允许 UDP 端口收包，默认端口为 `8888`。

## Kconfig 配置项

启用 `SAMPLE_SUPPORT_QRSWORK_CLIENT` 后，会出现以下配置：

| 配置项 | 默认值 | 说明 |
| --- | --- | --- |
| `CONFIG_QRSWORK_NORTHBOUND_ENABLE` | `y` | 启用 WiFi 北向 UDP 传输 |
| `CONFIG_QRSWORK_WIFI_SSID` | `"test"` | 板子要连接的 WiFi 名称 |
| `CONFIG_QRSWORK_WIFI_PSK` | `"0987654321"` | WiFi 密码 |
| `CONFIG_QRSWORK_SERVER_IP` | `"192.168.1.175"` | 电脑或服务器 IPv4 地址 |
| `CONFIG_QRSWORK_SERVER_PORT` | `8888` | 电脑或服务器 UDP 监听端口 |

本地 SDK 中可通过 menuconfig 修改：

```bash
cd /home/bearpi/project/bearpi-pico_h3863
./build.py menuconfig ws63-liteos-app
```

也可以直接确认目标配置文件：

```text
/home/bearpi/project/bearpi-pico_h3863/build/config/target_config/ws63/menuconfig/acore/ws63_liteos_app.config
```

## 编译与产物

本仓库代码同步到本地 SDK 后编译：

```bash
cd /home/bearpi/project/bearpi-pico_h3863
rsync -a /home/bearpi/project/Integrated-Practice/application/samples/qrswork_client/ \
    application/samples/qrswork_client/
./build.py ws63-liteos-app -j2
```

成功后主要产物：

```text
output/ws63/acore/ws63-liteos-app/ws63-liteos-app-sign.bin
output/ws63/fwpkg/ws63-liteos-app/ws63-liteos-app_all.fwpkg
output/ws63/fwpkg/ws63-liteos-app/ws63-liteos-app_load_only.fwpkg
```

## 电脑端接收测试

Linux 环境可以直接监听 UDP：

```bash
nc -klu 8888
```

如果当前 `nc` 版本参数不兼容，可以使用 Python：

```bash
python3 - <<'PY'
import socket

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(("0.0.0.0", 8888))
while True:
    data, addr = s.recvfrom(2048)
    print(addr, data.decode(errors="replace").strip())
PY
```

板子烧录运行后，串口日志中应能看到 WiFi 扫描、连接、DHCP 成功和 UDP 发送日志。电脑端应收到 JSON 行数据。

## 北向数据格式

心率数据：

```json
{"type":"heart","adc":1234,"bpm":72,"source":"qrswork_client"}
```

原始数据：

```json
{"type":"raw","len":4,"hex":"01 02 03 04","source":"qrswork_client"}
```

字段说明：

| 字段 | 说明 |
| --- | --- |
| `type` | `heart` 表示心率数据，`raw` 表示未识别原始数据 |
| `adc` | SLE 通知中解析出的 ADC 周期值，无法解析时为 `0` |
| `bpm` | 由 ADC 周期换算出的心率值，未检测到心率时为 `0` |
| `len` | 原始数据长度 |
| `hex` | 原始数据的十六进制摘要，最多保留前 32 字节 |
| `source` | 固定为 `qrswork_client`，便于上位机区分来源 |

## 预留接口

北向模块对 qrswork client 暴露以下接口：

```c
int northbound_client_start(void);
void northbound_client_publish_heart(uint32_t adc, uint32_t bpm);
void northbound_client_publish_raw(const uint8_t *data, uint16_t len);
```

设计上 SLE 回调只负责解析和入队，实际 UDP 发送由 `QrsworkNorthbound` 任务完成，避免在 SLE 通知回调里执行耗时网络操作。后续北向开发可以继续复用 `northbound_client_publish_*` 接口，也可以在 `northbound_client_poll_command()` 中扩展电脑到板子的控制命令。

## 当前边界

- 当前传输协议为 UDP，适合低成本实时调试；需要可靠传输时可以在北向模块内替换为 TCP 或 MQTT。
- 当前未做 WiFi 断线后的完整重连状态机，主要覆盖启动连接和发送路径。
- 当前命令下行仅打印收到的 UDP 内容，控制协议还未定义。
