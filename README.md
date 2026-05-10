# 智能健康监测系统 (Smart Health Monitor)

基于 **Hi3863 / 星闪 (NearLink)** 的便携式智能健康监测系统，通过指尖传感器实时采集心率、血氧、运动姿态等生理数据，经 SLE 星闪无线传输至客户端，再通过 WiFi 北向上报至上位机进行分析与展示。

---

## 硬件平台

| 组件 | 型号 | 接口 | 功能 |
|------|------|------|------|
| 主控 | BearPi-Pico H3863 (WS63/Hi3863V100) | — | 2.4GHz Wi-Fi 6 + BLE 5.2 + NearLink SLE |
| 心率血氧 | MAX30102 | I2C (GPIO7/8) | 指尖光电容积脉搏波采集 |
| 六轴 IMU | MPU6050 | I2C (GPIO7/8) | 加速度 + 陀螺仪，跌倒检测 |
| 心率波形 | KY-039 | ADC (GPIO0) | 模拟心率信号采集 |
| 震动检测 | SW-420 | GPIO | 振动传感器 |
| 显示 | SSD1306 | I2C | 0.96" OLED 状态显示 |
| 输出 | RGB LED / 蜂鸣器 / 振动马达 | GPIO | 多模态健康预警 |

---

## 目录结构

```
├── application/          # 主应用程序（传感器采集、SLE 通信、WiFi 北向）
├── lib/                  # 自定义库与硬件抽象层（驱动、算法、输出模块）
├── docs/                 # 项目文档（硬件参考、系统架构、发展方向）
├── tools/                # 辅助工具（串口调试、固件烧录脚本）
├── references/           # 参考与第三方资源（SDK、北向协议文档、硬件手册）
├── archive/              # 历史备份（旧版本项目、AI 修改记录）
├── CMakeLists.txt        # 构建系统入口
├── CHANGELOG.md          # 版本变更日志
├── CONTRIBUTING.md       # 团队协作规范
└── CLAUDE.md             # AI Agent 执行协议
```

---

## Quick Start / 快速编译与烧录

### 前置依赖

本工程依赖 **HiSpark Toolchain**（RISC-V 交叉编译工具链）。请确保：
1. 已将 `references/bearpi_3863_sdk/HiSpark_Toolchain/` 解压至本地（该目录已被 `.gitignore` 忽略，需手动获取）
2. 已安装 Python 3.8+（SDK 构建脚本 `build.py` 依赖 Python）

### 编译

```bash
# 进入 SDK 根目录
cd references/bearpi_3863_sdk/bearpi-pico_h3863

# 清理并编译（首次编译或 pull 后必须先 clean）
python build.py -c ws63

# 仅编译特定组件（例如只编译 application）
python build.py ws63 -component=application

# 多线程编译（指定线程数）
python build.py -j8 ws63
```

编译产物输出至 `output/` 目录。

### 烧录

<待补充：请使用 HiFlash 或 J-Link 烧录 `output/` 目录下的 `.bin` 文件至开发板>

### 验证

烧录完成后，通过串口工具（波特率 115200）连接开发板，观察启动日志确认固件运行正常。

---

## 开发与查阅指引

| 你要做什么 | 去哪里看 |
|-----------|---------|
| 了解项目背景与整体规划 | `docs/项目展望与开发方向.md` |
| 查硬件引脚与外设接口 | `docs/HARDWARE_REFERENCE.md` |
| 查系统架构与模块职责 | `docs/SYSTEM_README.md` |
| 开发硬件驱动 / SLE / 北向功能 | `references/bearpi_3863_sdk/` — 先读头文件和范例，禁止凭空捏造 API |
| 查北向通信协议与数据帧格式 | `references/northbound_docs_v5_7/NORTHBOUND_PROTOCOL.md` |
| 了解已有的传感器驱动与算法 | `application/samples/qrswork/` 及其 `README.md` |
| 了解 WiFi 北向上报实现 | `application/samples/qrswork_client/` 及其 `README.md` |
| 提交代码前的规范检查 | `CONTRIBUTING.md` |
| 查看版本变更历史 | `CHANGELOG.md` |
