# 传感器数据分析（lib 同步版）

## 一、传感器数据总览

| 传感器 | 接口 | 原始数据类型 | 处理后单位 | JSON 字段 |
|--------|------|-------------|-----------|-----------|
| MAX30102 | I2C (0x57) | uint32_t (18位) | BPM, % | `hr`, `spo2` |
| MPU6050 | I2C (0x68) | int16_t (16位) | g, °/s, ℃ | `accel`, `gyro`, `temp` |
| KY-039 | ADC | uint16_t (12位) | 心率BPM | `hr_source`（备用） |
| SW-420 | GPIO | bool (0/1) | 震动检测 | *(辅助跌倒检测)* |
| SSD1315 | I2C (0x3C) | - | 显示输出 | - |
| RGB灯 | GPIO | - | 颜色控制 | - |
| 蜂鸣器 | PWM | - | 频率/时长 | - |
| 震动马达 | GPIO | - | 开关控制 | - |

---

## 二、JSON 数据格式（串口输出）

与 `health_monitor_main.c` 的 `send_data_to_serial()` 完全一致：

```json
{
  "hr": 72,
  "spo2": 98,
  "temp": 36.5,
  "accel": [0.10, 0.05, 1.00],
  "gyro": [0.10, 0.05, 0.02],
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

### 字段说明

| 字段 | 类型 | 来源 | 说明 |
|------|------|------|------|
| `hr` | uint32 | MAX30102/KY-039 | 心率 BPM，0 表示无效 |
| `spo2` | uint32 | MAX30102 | 血氧 %，0 表示无效 |
| `temp` | float | MPU6050 | 体温 ℃ |
| `accel` | float[3] | MPU6050 | 加速度 [x,y,z] 单位 g |
| `gyro` | float[3] | MPU6050 | 角速度 [x,y,z] 单位 °/s |
| `pitch` | float | 互补滤波 | 俯仰角 °，站立≈0°，前倾增大 |
| `roll` | float | 互补滤波 | 横滚角 °，站立≈0°，侧倾增大 |
| `fall_conf` | uint8 | 融合算法 | 跌倒置信度 0-100 |
| `fall_alert` | bool | 跌倒检测 | true=已判定跌倒，等待确认 |
| `status` | string | 状态机 | `"Status: Normal"` / `"[!] High HR"` / `"[!!!] FALL!"` 等（带等级前缀） |
| `hr_source` | string | 心率源 | max30102（主）/ ky039（备用） |
| `valid` | object | 质量评估 | 各传感器数据有效性标志 |

### status 取值规则

| status | 触发条件 | 报警等级 |
|--------|---------|---------|
| `Status: Normal` | 所有指标正常 | 无 |
| `[!] High HR` | 心率 > 100 BPM | WARNING |
| `[!] Low HR` | 心率 < 60 BPM | WARNING |
| `[!!] Low SpO2!` | 血氧 < 90% | DANGER |
| `[!!!] FALL!` | 跌倒置信度 ≥ 60% | DANGER |
| `[i] Fever` | 体温 > 37.5℃ | INFO |

---

## 三、各传感器数据详解

### 1. MAX30102 心率血氧

**原始数据：** FIFO 每次读取 6 字节（红光 3 字节 + 红外 3 字节），右移 6 位后为 18 位有效数据。

**处理流程：**
```
FIFO → 滑动平均滤波(4点) → 自适应阈值峰值检测 → RR间期 → 心率
                                                      ↓
                                               SpO2 = 110 - 25*R  (R=AC_red/AC_ir)
```

**有效性判断：**
- `hr_valid`: `main_max30102_data()` 返回 true 且 `heart_rate > 0`
- 超过 5 秒无更新自动标记无效（`DATA_STALE_TIMEOUT_MS`）

### 2. MPU6050 六轴 IMU

**原始数据：** 连续读取 14 字节，含加速度(6B) + 温度(2B) + 陀螺仪(6B)。

**转换公式：**
```c
accel_g = raw_value / sensitivity;   // ±2g:16384, ±4g:8192, ±8g:4096, ±16g:2048
gyro_dps = raw_value / sensitivity;  // ±250:131, ±500:65.5, ±1000:32.8, ±2000:16.4
temperature = raw_value / 340.0 + 36.53;
```

**滤波：** 加速度和陀螺仪各 3 轴独立卡尔曼滤波（q=0.01, r=0.5）。

### 3. SW-420 震动传感器

**引脚：** GPIO4（启动限制引脚，输入模式安全）
**数据：** GPIO 低电平有效，`0` = 检测到震动，`1` = 无震动。

**用途：** 辅助跌倒检测，震动 + 高加速度 → 跌倒置信度 +10%。

### 4. KY-039 心率传感器

**状态：** 已整合为备用心率源。当 MAX30102 心率无效时（`hr_valid=false` 且超过 5 秒无更新），自动切换到 KY-039 数据。JSON 中 `hr_source` 字段标识当前来源（`"max30102"` 或 `"ky039"`）。

---

## 四、跌倒检测算法

采用多特征加权融合：

| 特征 | 权重 | 判定条件 |
|------|------|---------|
| 跌倒特征 | 50% | (高加速度 + 高倾斜角) 或 (高加速度 + 低角速度) |
| 加速度异常 | 25% | `accel_magnitude > 2.5g` |
| 倾斜角度 | 15% | `tilt_angle > 45°` |
| 震动辅助 | 10% | `vibration && accel > 2.0g` |

**状态机：** NORMAL → FALLING(置信度≥60%持续3秒) → FALLEN → ALERT(用户确认) → NORMAL

**冷却期：** 确认跌倒后 10 秒内不重新触发。

---

## 五、健康提醒分级

| 等级 | 触发条件 | 提醒方式 |
|------|---------|---------|
| INFO | 发烧 | OLED文字 + LED黄灯(1Hz闪) |
| WARNING | 心率异常 | LED黄灯闪烁(2Hz) + 脉冲震动(200ms/2s) |
| DANGER | 血氧低/跌倒 | LED红灯快闪(4Hz) + 蜂鸣器 + 持续震动 |

**滞后防抖：** 心率阈值 ±5 BPM，血氧阈值 ±2%。

---

## 六、硬件连接

| 传感器 | I2C地址 | 引脚 | 模式 |
|--------|---------|------|------|
| SSD1315 | 0x3C | GPIO15(SCL)/GPIO16(SDA) | mode=2, I2C1 |
| MAX30102 | 0x57 | 共用 I2C 总线 | - |
| MPU6050 | 0x68 | 共用 I2C 总线 | - |
| KY-039 | ADC | ADC引脚 | 12位采样 |
| SW-420 | GPIO | GPIO输入 | 低电平有效 |
| RGB灯 | GPIO | R=GPIO6, G=GPIO7, B=GPIO8 | 共阳（LOW=亮） |
| 蜂鸣器 | PWM | PWM输出 | 100-5000Hz |
| 震动马达 | GPIO | GPIO3输出 | 高电平有效，需MOSFET驱动 |
