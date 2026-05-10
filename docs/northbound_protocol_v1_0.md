# Northbound Communication Protocol v1.0

> Smart Health Monitor System - Hi3863 Embedded Platform
> Protocol version: 1.0 | Last updated: 2026-05-11

---

## 1. Overview

This document defines the northbound communication protocol between the Hi3863 embedded health monitor device and the Android companion App. The device operates as a WiFi hotspot and TCP server, pushing real-time health data in JSON format and receiving control commands from the App.

```
┌──────────────┐    WiFi AP     ┌──────────────┐
│   Hi3863     │◄──────────────►│  Android App │
│  TCP Server  │   TCP :5000    │  TCP Client  │
│  192.168.4.1 │                │  192.168.4.x │
└──────────────┘                └──────────────┘
```

---

## 2. Network Layer Configuration

### 2.1 WiFi Access Point (SoftAP)

| Parameter | Value | Description |
|-----------|-------|-------------|
| SSID | `HealthMonitor` | Device hotspot name |
| Password | `12345678` | WPA2-PSK passphrase |
| Security | WPA2-PSK | Encryption method |
| Channel | 6 | 2.4GHz WiFi channel |
| Device IP | `192.168.4.1` | Gateway address, App connection target |
| Subnet Mask | `255.255.255.0` | /24 subnet |
| DHCP Range | `192.168.4.2` ~ `192.168.4.254` | Auto-assigned to connected clients |

### 2.2 TCP Server

| Parameter | Value | Description |
|-----------|-------|-------------|
| Protocol | TCP (SOCK_STREAM) | Persistent connection |
| Listen Port | `5000` | Fixed port number |
| Bind Address | `0.0.0.0` (INADDR_ANY) | Accepts connections on any interface |
| Backlog | 1 | Single client design |
| Message Format | Newline-delimited JSON | Each message ends with `\n` |

### 2.3 Connection Sequence

1. App connects to device WiFi `HealthMonitor` with password `12345678`
2. App receives IP address via DHCP (e.g., `192.168.4.100`)
3. App opens TCP socket to `192.168.4.1:5000`
4. Device accepts connection and begins pushing data every 1 second
5. App can send control commands at any time over the same TCP connection
6. If connection drops, device automatically returns to listening for new connections

---

## 3. Uplink Data Flow (Device → App)

The device pushes a JSON message every **1000ms** (configurable via `TCP_SEND_INTERVAL_MS`).

### 3.1 JSON Structure

```json
{
  "hr": 72,
  "spo2": 98,
  "temp": 36.5,
  "accel": [0.05, -0.02, 1.01],
  "gyro": [1.23, -0.45, 0.12],
  "pitch": 2.3,
  "roll": -1.1,
  "fall_conf": 0,
  "fall_alert": false,
  "status": "Normal",
  "hr_source": "ky039",
  "valid": {
    "hr": true,
    "spo2": true,
    "temp": true,
    "imu": true
  }
}
```

### 3.2 Field Definitions

#### Core Vital Signs

| Field | Type | Unit | Description |
|-------|------|------|-------------|
| `hr` | int | BPM | Heart rate. Value range: 40~200. Source: KY-039 ADC sensor |
| `spo2` | int | % | Blood oxygen saturation. Value range: 70~100 |
| `temp` | float | °C | Body temperature from MPU6050 on-chip sensor. Precision: 0.1°C |

#### IMU Data (MPU6050 6-axis)

| Field | Type | Unit | Description |
|-------|------|------|-------------|
| `accel` | float[3] | g | 3-axis acceleration [X, Y, Z]. Z≈1.0g when stationary upright |
| `gyro` | float[3] | dps | 3-axis angular velocity [X, Y, Z]. Unit: degrees per second |

#### Attitude Estimation

| Field | Type | Unit | Description |
|-------|------|------|-------------|
| `pitch` | float | degree | Pitch angle (forward/backward tilt). Complementary filter output (α=0.96) |
| `roll` | float | degree | Roll angle (left/right tilt). Complementary filter output |

#### Fall Detection

| Field | Type | Description |
|-------|------|-------------|
| `fall_conf` | int | Fall confidence percentage (0~100). 0 = normal, 100 = certain fall |
| `fall_alert` | bool | `true` when fall state is confirmed (`FALL_STATE_FALLEN`) |

#### System Status

| Field | Type | Description |
|-------|------|-------------|
| `status` | string | Health status text. Possible values: `"Normal"`, `"High HR"`, `"Low SpO2"`, `"Fall Alert"`, etc. |
| `hr_source` | string | Heart rate sensor identifier. Always `"ky039"` in current version |

#### Data Validity Flags

| Field | Type | Description |
|-------|------|-------------|
| `valid.hr` | bool | `true` if heart rate data is fresh (updated within 5 seconds) |
| `valid.spo2` | bool | `true` if SpO2 data is valid |
| `valid.temp` | bool | `true` if temperature data is valid |
| `valid.imu` | bool | `true` if IMU (accel + gyro) data is valid |

> **Note**: When a validity flag is `false`, the corresponding data field may contain stale or default values. The App should display `"---"` or equivalent placeholder.

---

## 4. Downlink Control Flow (App → Device)

The App sends control commands as JSON strings over the same TCP connection. Each command must end with `\n`.

### 4.1 Command Format

```json
{"command":"<command_name>"}\n
```

### 4.2 Supported Commands

| Command | Description | Device Behavior |
|---------|-------------|-----------------|
| `breath_start` | Start breathing guide | Enters `SYS_STATE_BREATH_GUIDE`, displays inhale/exhale animation on OLED |
| `breath_stop` | Stop breathing guide | Returns to `SYS_STATE_NORMAL`, shows breathing effect feedback |
| `mute` | Mute current alert | Silences buzzer and vibration motor, keeps alert state |
| `confirm` | Confirm alert / fall | Acknowledges alert or fall detection, returns to `SYS_STATE_NORMAL` |

### 4.3 Command Examples

```json
{"command":"breath_start"}
{"command":"breath_stop"}
{"command":"mute"}
{"command":"confirm"}
```

### 4.4 Command Processing Notes

- Commands are case-sensitive and matched via substring search (`strstr`)
- Unknown commands are silently ignored (logged to serial console)
- Commands can be sent at any time, regardless of current system state
- Some commands may have no effect in certain states (e.g., `breath_stop` when not in breathing guide mode)

---

## 5. System States

The device operates as a state machine. Commands and sensor data trigger state transitions.

```
              ┌─────────────┐
              │    INIT     │
              └──────┬──────┘
                     │ init complete
                     ▼
              ┌─────────────┐
    ┌────────►│   NORMAL    │◄────────────┐
    │         └──────┬──────┘             │
    │                │ anomaly detected   │ recovered
    │                ▼                    │
    │         ┌─────────────┐             │
    │         │   ALERT     │─────────────┘
    │         └──────┬──────┘
    │                │ breath_start cmd
    │                ▼
    │         ┌─────────────┐
    └─────────│BREATH_GUIDE │
   finished   └─────────────┘
```

| State | Description | OLED Display |
|-------|-------------|--------------|
| `NORMAL` | Standard monitoring mode | Heart rate, SpO2, temperature, accel data |
| `ALERT` | Health anomaly detected | `!!! ALERT !!!` with alert details and vital signs |
| `BREATH_GUIDE` | Guided breathing exercise | Breathing animation (inhale/exhale cycle) |
| `SLE_CONNECTED` | SLE (NearLink) device connected | Same as NORMAL, SLE data forwarding active |

---

## 6. Error Handling

### 6.1 Connection Recovery

- If `send()` returns error (client disconnected), device closes the client socket and returns to `accept()` to wait for reconnection
- If `recv()` returns 0 (graceful disconnect) or negative (error), same recovery behavior
- The listen socket remains open; only the client socket is recycled

### 6.2 Data Freshness

- Heart rate data expires after 5 seconds without update (`DATA_STALE_TIMEOUT_MS`)
- Expired data has `valid.hr = false`; App should display placeholder
- IMU data validity is checked per read cycle; `valid.imu = false` if MPU6050 read fails

### 6.3 WiFi AP Stability

- WiFi AP initialization failure does not block system startup
- Device continues to operate via serial console and SLE communication
- TCP Server task creation failure is logged but does not block the main health monitoring loop

---

## 7. Serial Console Output (Debug)

For debugging purposes, the device also outputs the same JSON data to the serial console (UART) every second. The serial output uses `snprintf` formatting (not cJSON) but produces identical field structure.

Serial baud rate: `115200`

Example serial output:
```
{"hr":72,"spo2":98,"temp":36.5,"accel":[0.05,-0.02,1.01],"gyro":[1.23,-0.45,0.12],"pitch":2.3,"roll":-1.1,"fall_conf":0,"fall_alert":false,"status":"Normal","hr_source":"ky039","valid":{"hr":true,"spo2":true,"temp":true,"imu":true}}
```

---

## 8. Implementation References

| Component | Source File | Description |
|-----------|-------------|-------------|
| WiFi AP Init | `lib/comm/tcp_server.c` | SoftAP configuration and startup |
| TCP Server Task | `lib/comm/tcp_server.c` | Socket lifecycle, data pump, command handler |
| Data Fusion | `lib/app/health_monitor_main.c` | `data_fusion_build_json()` - cJSON builder |
| Command Handler | `lib/app/health_monitor_main.c` | `health_monitor_process_command()` - command parser |
| System Entry | `lib/app_main.c` | Initialization sequence and task creation |
