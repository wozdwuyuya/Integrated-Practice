# 北端上位机通信协议文档

> 给负责上位机/北端开发的同学
> 更新时间：2026-05-07

---

## 一、通信接口参数

| 参数 | 值 |
|------|-----|
| 接口类型 | USB 串口（CH340E USB 转 UART） |
| 波特率 | **115200** |
| 数据位 | 8 |
| 停止位 | 1 |
| 校验位 | 无 |
| 流控 | 无 |
| 行结束符 | `\r\n`（每条 JSON 以换行结尾） |
| 数据方向 | 下位机 → 上位机（单向推送，1秒/条） |
| 数据格式 | JSON 字符串 |

---

## 二、JSON 数据格式

下位机每秒通过串口发送一条 JSON 数据，格式如下：

```json
{
  "hr": 72,
  "spo2": 98,
  "temp": 36.5,
  "accel": [0.10, 0.05, 1.00],
  "gyro": [2.00, 1.50, 0.10],
  "pitch": 1.2,
  "roll": 0.8,
  "fall_conf": 0,
  "fall_alert": false,
  "status": "Status: Normal",
  "hr_source": "max30102",
  "valid": {
    "hr": true,
    "spo2": true,
    "temp": true,
    "imu": true
  }
}
```

---

## 三、字段说明

### 3.1 生命体征数据

| 字段 | 类型 | 单位 | 取值范围 | 说明 |
|------|------|------|----------|------|
| `hr` | uint32 | BPM | 0, 40~200 | 心率。0 表示无数据 |
| `spo2` | uint32 | % | 0, 70~100 | 血氧饱和度。0 表示无数据 |
| `temp` | float | ℃ | 35.0~42.0 | 体温（MPU6050 内置温度传感器） |
| `hr_source` | string | - | `"max30102"` / `"ky039"` | 心率数据来源。MAX30102 为主，无效时自动切换 KY-039 |

### 3.2 运动数据

| 字段 | 类型 | 单位 | 取值范围 | 说明 |
|------|------|------|----------|------|
| `accel` | float[3] | g | ±16 | 加速度 [x, y, z]。静止时 z≈1.0（重力） |
| `gyro` | float[3] | °/s | ±500 | 角速度 [x, y, z]。静止时≈0 |
| `pitch` | float | ° | -180~180 | 俯仰角（互补滤波输出）。站立≈0°，前倾增大 |
| `roll` | float | ° | -90~90 | 横滚角（互补滤波输出）。站立≈0°，侧倾增大 |

### 3.3 跌倒检测

| 字段 | 类型 | 取值范围 | 说明 |
|------|------|----------|------|
| `fall_conf` | uint8 | 0~100 | 跌倒置信度。≥60% 持续 3 秒触发报警 |
| `fall_alert` | bool | true/false | 跌倒报警状态。true=已判定跌倒，等待确认 |

### 3.4 系统状态

| 字段 | 类型 | 说明 |
|------|------|------|
| `status` | string | 当前健康状态文本（见下表） |

**status 取值规则：**

| status 值 | 含义 | 报警等级 |
|-----------|------|---------|
| `"Status: Normal"` | 所有指标正常 | 无 |
| `"[!] High HR"` | 心率 > 100 BPM | WARNING |
| `"[!] Low HR"` | 心率 < 60 BPM | WARNING |
| `"[!!] Low SpO2!"` | 血氧 < 90% | DANGER |
| `"[!!!] FALL!"` | 跌倒置信度 ≥ 60% | DANGER |
| `"[i] Fever"` | 体温 > 37.5℃ | INFO |

### 3.5 数据有效性

| 字段 | 类型 | 说明 |
|------|------|------|
| `valid.hr` | bool | 心率数据是否有效（false=传感器离线或超时） |
| `valid.spo2` | bool | 血氧数据是否有效 |
| `valid.temp` | bool | 温度数据是否有效 |
| `valid.imu` | bool | IMU 数据是否有效（加速度+陀螺仪） |

**有效性规则：**
- 传感器初始化成功 → 对应 valid = true
- 心率/血氧超过 5 秒无更新 → `valid.hr` 和 `valid.spo2` 自动变 false
- 无效时下位机 OLED 显示 "---"，JSON 中值为 0

---

## 四、数据发送时序

```
下位机启动
  │
  ├─ 初始化（约 500ms）
  │
  └─ 进入主循环
       │
       ├─ 每 100ms：更新传感器数据 + 算法计算
       │
       └─ 每 1000ms：发送一条 JSON 到串口
            │
            └─ 你的上位机解析这条 JSON
```

**发送频率：** 1 条/秒（`DATA_SEND_INTERVAL_MS = 1000`）

---

## 五、Mock 模式输出样例

当前下位机运行在 Mock 模式（无硬件），输出的 JSON 如下：

### 样例 1：正常状态

```json
{"hr":72,"spo2":98,"temp":36.5,"accel":[0.08,0.04,1.01],"gyro":[1.85,1.32,0.08],"pitch":1.2,"roll":0.8,"fall_conf":0,"fall_alert":false,"status":"Status: Normal","hr_source":"max30102","valid":{"hr":true,"spo2":true,"temp":true,"imu":true}}
```

### 样例 2：心率偏高

```json
{"hr":105,"spo2":96,"temp":36.8,"accel":[0.12,0.06,0.99],"gyro":[2.10,1.60,0.12],"pitch":2.1,"roll":1.5,"fall_conf":0,"fall_alert":false,"status":"[!] High HR","hr_source":"max30102","valid":{"hr":true,"spo2":true,"temp":true,"imu":true}}
```

### 样例 3：跌倒报警

```json
{"hr":88,"spo2":95,"temp":36.6,"accel":[2.50,0.30,0.50],"gyro":[15.00,8.00,2.00],"pitch":45.2,"roll":12.3,"fall_conf":100,"fall_alert":true,"status":"[!!!] FALL!","hr_source":"max30102","valid":{"hr":true,"spo2":true,"temp":true,"imu":true}}
```

---

## 六、解析代码参考（Python）

```python
import serial
import json

ser = serial.Serial('COM3', 115200, timeout=1)

while True:
    line = ser.readline().decode('utf-8', errors='ignore').strip()
    if not line or not line.startswith('{'):
        continue

    try:
        data = json.loads(line)
    except json.JSONDecodeError:
        continue

    # 生命体征
    hr      = data['hr']          # int, BPM
    spo2    = data['spo2']        # int, %
    temp    = data['temp']        # float, ℃
    source  = data['hr_source']   # str, "max30102" or "ky039"

    # 运动数据
    accel   = data['accel']       # [x, y, z], 单位 g
    gyro    = data['gyro']        # [x, y, z], 单位 °/s
    pitch   = data['pitch']       # float, °
    roll    = data['roll']        # float, °

    # 跌倒检测
    fall_conf   = data['fall_conf']    # int, 0-100
    fall_alert  = data['fall_alert']   # bool

    # 状态
    status  = data['status']      # str, 状态文本
    valid   = data['valid']       # dict, 有效性标记

    # 处理数据...
    print(f"HR={hr} SpO2={spo2}% Temp={temp}°C Pitch={pitch}° Roll={roll}° Fall={fall_alert}")
```

---

## 七、下位机控制命令（SLE 通道，预留）

下位机支持通过 SLE（星闪）接收以下命令（当前 Mock 模式下 SLE 未启用）：

| 命令字符串 | 功能 |
|-----------|------|
| `"breath_start"` | 启动呼吸引导 |
| `"breath_stop"` | 停止呼吸引导 |
| `"mute"` | 静音报警 30 秒 |
| `"confirm"` | 确认跌倒报警 |

---

## 八、注意事项

1. **JSON 缓冲区：** 下位机发送缓冲区为 400 字节，JSON 总长度不超过 350 字节
2. **浮点精度：** 温度保留 1 位小数，加速度/陀螺仪保留 2 位小数，pitch/roll 保留 1 位小数
3. **无效数据：** 当 `valid.hr=false` 时，`hr` 字段值为 0，上位机应显示 "---" 或忽略
4. **状态文本：** status 字段包含特殊字符 `[!]` `[!!]` `[!!!]` `[i]`，解析时注意
5. **编码：** UTF-8，无 BOM
