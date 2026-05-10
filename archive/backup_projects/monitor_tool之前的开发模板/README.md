# 星闪智能健康监测系统 v2.0 上位机

接收 BearPi-H3863 开发板通过串口发送的 JSON 数据，实时显示多维度健康指标，支持 AI 辅助诊断。

## 支持的传感器数据

| 指标 | 数据来源 | JSON 字段 | 说明 |
|------|---------|-----------|------|
| 心率 (HR) | MAX30102/KY-039 | `hr` | 单位 BPM，MAX30102优先，无效时自动切换KY-039 |
| 血氧 (SpO2) | MAX30102 | `spo2` | 单位 % |
| 体温 (Temp) | MPU6050 | `temp` | 单位 ℃ |
| 加速度 (Accel) | MPU6050 | `accel` | [x, y, z] 单位 g |
| 角速度 (Gyro) | MPU6050 | `gyro` | [x, y, z] 单位 °/s |
| 跌倒置信度 | 融合算法 | `fall_conf` | 0-100 |
| 系统状态 | 状态机 | `status` | Normal/High HR/Low SpO2/FALL!/Fever |
| 心率源 | 双传感器 | `hr_source` | max30102（主）/ ky039（备用） |
| 数据有效性 | 质量评估 | `valid` | {hr, spo2, temp, imu} |

## 数据格式

开发板每秒发送一行 JSON：

```json
{"hr":72,"spo2":98,"temp":36.5,"accel":[0.12,-0.03,9.78],"gyro":[0.5,-0.2,0.1],"fall_conf":0,"status":"Normal","valid":{"hr":true,"spo2":true,"temp":true,"imu":true}}
```

## 功能

- 实时显示心率、血氧、体温、加速度、跌倒置信度
- 双图表：HR+SpO2趋势图（双Y轴）、加速度幅值图
- 按钮下发命令：开始/停止呼吸引导、静音、确认报警
- CSV 自动记录所有数据
- AI 健康助手（DeepSeek API）：实时问答 + 诊断报告

## 环境要求

- Python 3.x
- pyserial、matplotlib、numpy、openai

## 安装与运行

```bash
pip install pyserial matplotlib numpy openai
python monitor.py
```

## 文件说明

```
monitor.py              主程序（GUI + 串口 + AI）
binary_frame_parser.py  旧版二进制帧解析器（备用）
adaptive_threshold.py   自适应阈值算法（备用）
robust_peak_and_rmssd.py 峰值检测与RMSSD计算（备用）
ai_engine.py            AI信号质量评估引擎（备用）
```
