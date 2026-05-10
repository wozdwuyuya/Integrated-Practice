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
