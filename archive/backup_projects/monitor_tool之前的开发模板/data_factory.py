import serial
import csv
import time
import os
import numpy as np
from collections import deque
from ai_engine import HeartAI # 新增：引入 AI 推理引擎

# --- 配置参数 ---
PORT = 'COM5'          # 你的串口号
BAUD = 115200          # 波特率
HEADER = bytes([0x51, 0x51, 0x16, 0xE3, 0xBE]) # 目标帧头
WINDOW_SIZE = 10       # 用于计算标准差的滑动窗口大小
CSV_FILE = f"ai_training_data_{time.strftime('%Y%m%d_%H%M%S')}.csv"

# --- AI & 信号质量监测配置 ---
heart_ai = HeartAI()   # 初始化 AI 引擎
std_baseline_deque = deque(maxlen=50) # 存储 5 秒(假设10Hz)的 STD 均值，用于动态阈值
disconnected_start_time = None # 用于连续 3 秒脱落报警
last_status = None # 记录上一个状态，用于状态切换强制打印
last_print_time = 0 # 记录上次打印时间，用于频率控制 (2次/秒)

# 统计信息
healthy_count = 0
anomaly_count = 0
total_processed = 0

# 初始化
ser_buffer = bytearray()
data_history = deque(maxlen=WINDOW_SIZE)

print(f"==========================================")
print(f"  AI 数据采集神器 (Data Factory) v1.2")
print(f"  正在监听: {PORT} @ {BAUD}")
print(f"  目标文件: {CSV_FILE}")
print(f"  AI 状态监测: 已引入【最强安静判定】")
print(f"  界面配色: \033[92mHEALTHY\033[0m | \033[93mINTERFERENCE\033[0m | \033[91mDISCONNECTED\033[0m")
print(f"  按 Ctrl+C 停止采集并保存数据")
print(f"==========================================")

try:
    ser = serial.Serial(PORT, BAUD, timeout=0.1)
    
    with open(CSV_FILE, 'w', newline='') as f:
        writer = csv.writer(f)
        # 写入更丰富的特征头
        writer.writerow(['Timestamp', 'Raw_Value', 'STD_10', 'Status', 'Label'])
        
        count = 0
        while True:
            if ser.in_waiting > 0:
                chunk = ser.read(ser.in_waiting)
                ser_buffer.extend(chunk)
                
                # 帧对齐解析逻辑
                while len(ser_buffer) >= 6:
                    header_pos = ser_buffer.find(HEADER)
                    
                    if header_pos == 0:
                        # 提取数据位
                        raw_val = ser_buffer[len(HEADER)]
                        data_history.append(raw_val)
                        
                        # 1. 计算特征：最近10个点的标准差
                        if len(data_history) < WINDOW_SIZE:
                            std_val = 0.0
                            status, label = "INITIALIZING...", 0
                        else:
                            std_val = np.std(list(data_history))
                            std_baseline_deque.append(std_val)
                            # 3. AI 在线推理与自动标注 (含硬阈值拦截)
                            status, label = heart_ai.predict(raw_val, std_val)
                            
                            # 严格标注逻辑：仅 HEALTHY 为 0，其他均为 1
                            label = 0 if status == "HEALTHY" else 1
                            
                            # 更新统计信息
                            total_processed += 1
                            if status == "HEALTHY":
                                healthy_count += 1
                            else:
                                anomaly_count += 1
                        
                        # 4. 连续脱落报警逻辑
                        if status == "DISCONNECTED":
                            if disconnected_start_time is None:
                                disconnected_start_time = time.time()
                            elif time.time() - disconnected_start_time > 3.0:
                                # 触发报警（控制台红色提示）
                                print(f"\n\033[91;1m[!!! CRITICAL !!!] 传感器已脱落持续 3 秒! (Raw: {raw_val})\033[0m", flush=True)
                        else:
                            disconnected_start_time = None

                        # 5. 记录数据 (Timestamp, Raw_Value, STD_10, Status, Label)
                        timestamp = time.time()
                        writer.writerow([f"{timestamp:.3f}", raw_val, f"{std_val:.2f}", status, label])
                        
                        count += 1
                        
                        # 6. 智能打印频率控制
                        now = time.time()
                        status_changed = (status != last_status)
                        
                        # 规则：状态切换立即打印，或者每 0.5 秒打印一次摘要
                        if status_changed or (now - last_print_time >= 0.5):
                            # 界面配色
                            if status == "HEALTHY":
                                status_color = "\033[92m" # 绿
                            elif status == "INTERFERENCE":
                                status_color = "\033[93m" # 黄
                            elif status == "DISCONNECTED":
                                status_color = "\033[91m" # 红
                            elif status == "INITIALIZING...":
                                status_color = "\033[94m" # 蓝
                            else:
                                status_color = "\033[0m"
                            
                            if status_changed:
                                print(f"\n\033[96m[Status Change] {last_status} -> {status_color}{status}\033[0m")
                            
                            # 计算数据有效率
                            healthy_rate = (healthy_count / total_processed * 100) if total_processed > 0 else 0
                            anomaly_rate = (anomaly_count / total_processed * 100) if total_processed > 0 else 0
                            
                            stats_str = f"[\033[94m统计\033[0m] 正常率: {healthy_rate:>5.1f}% | 异常率: {anomaly_rate:>5.1f}%"
                            print(f"[{time.strftime('%H:%M:%S')}] 点数: {count:<6} | 状态: {status_color}{status:<15}\033[0m | Raw: {raw_val:<3} | STD: {std_val:>5.2f} | {stats_str}    ", end='\r')
                            
                            last_status = status
                            last_print_time = now
                        
                        # 消费掉这帧
                        del ser_buffer[:len(HEADER) + 1]
                    elif header_pos > 0:
                        del ser_buffer[:header_pos]
                    else:
                        if len(ser_buffer) > 100:
                            del ser_buffer[:-len(HEADER)]
                        break
                        
except serial.SerialException as e:
    print(f"\n!!! 串口错误: {e}")
except KeyboardInterrupt:
    print(f"\n采集停止。共保存 {count} 条数据到 {CSV_FILE}")
finally:
    if 'ser' in locals() and ser.is_open:
        ser.close()
