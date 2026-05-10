import serial
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import collections
import os

# --- 配置参数 --- 
PORT = 'COM5'          # 已根据用户反馈更新为 COM5
BAUD = 115200          
MAX_POINTS = 500       # 屏幕上显示的采样点数
SAVE_FILE = "heart_raw_data.bin"

# 目标帧头 (51 51 16 E3 BE)
HEADER = bytes([0x51, 0x51, 0x16, 0xE3, 0xBE])

# 初始化数据容器
data_queue = collections.deque([128] * MAX_POINTS, maxlen=MAX_POINTS)
ser_buffer = bytearray()

try:
    ser = serial.Serial(PORT, BAUD, timeout=0.1)
    raw_log = open(SAVE_FILE, "wb")
    print(f"==========================================")
    print(f"  对齐波形探测器 (Aligned Sniffer) 已启动")
    print(f"  正在监听端口: {PORT} @ {BAUD}")
    print(f"  对齐特征头: 51 51 16 E3 BE")
    print(f"  原始数据将同步保存至: {os.path.abspath(SAVE_FILE)}")
    print(f"==========================================")
except Exception as e:
    print(f"!!! 串口连接错误: {e}")
    exit()

# 设置 Matplotlib 绘图
fig, ax = plt.subplots(figsize=(10, 5))
line, = ax.plot(list(data_queue), color='#00FF00', linewidth=1.5)
ax.set_facecolor('black')
ax.set_ylim(0, 255)       # 字节范围 0-255
ax.grid(True, color='#333333', linestyle='--')
plt.title("Heart Aligned Signal Sniffer (Raw Byte Wave)", color='black', fontsize=12)

def update(frame):
    global ser_buffer
    if ser.in_waiting > 0:
        # 1. 直接获取 bytes 原始字节
        new_bytes = ser.read(ser.in_waiting)
        
        # 原始记录 (老师要求的强化采集)
        raw_log.write(new_bytes)
        raw_log.flush()
        
        # 加入解析缓冲区
        ser_buffer.extend(new_bytes)
        
        # 2. 针对 51 51 16 E3 BE 做对齐并提取数值
        while len(ser_buffer) >= 6: # 头(5字节) + 数据(至少1字节)
            # 查找头的位置
            header_pos = ser_buffer.find(HEADER)
            
            if header_pos == 0:
                # 找到头且在起始位置，提取紧随其后的字节作为波形值
                # 注意：这里假设紧跟头的是数据，如果协议有变可微调
                val = ser_buffer[len(HEADER)] 
                data_queue.append(val)
                
                # 消费掉这部分数据 (头 + 1字节数据)
                del ser_buffer[:len(HEADER) + 1]
            elif header_pos > 0:
                # 头不在起始位置，丢弃前面的杂质
                del ser_buffer[:header_pos]
            else:
                # 没找到头，如果缓冲区过长则保留尾部可能的部分头
                if len(ser_buffer) > 100:
                    del ser_buffer[:-len(HEADER)]
                break
            
        # 更新波形图数据
        line.set_ydata(list(data_queue))
    return line,

# 使用 FuncAnimation 实现流畅的实时刷新
ani = FuncAnimation(fig, update, interval=20, blit=True)

try:
    plt.show()
except KeyboardInterrupt:
    print("\n用户手动停止采集。")
finally:
    if 'ser' in locals() and ser.is_open:
        ser.close()
    if 'raw_log' in locals() and not raw_log.closed:
        raw_log.close()
    print("采集结束。")
