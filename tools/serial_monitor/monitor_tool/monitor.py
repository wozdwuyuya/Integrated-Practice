import sys
import os
import time
import threading
import struct
import warnings

# --- 静默警告信息 (Matplotlib 字体缺失警告) ---
warnings.filterwarnings("ignore", category=UserWarning)

# --- 修复 PyInstaller --noconsole 模式下的 stdout 问题 ---
class NullWriter:
    def write(self, text): pass
    def flush(self): pass
    def reconfigure(self, *args, **kwargs): pass

if sys.stdout is None:
    sys.stdout = NullWriter()
if sys.stderr is None:
    sys.stderr = NullWriter()

# 强制设置标准输出编码并立即刷新，确保控制台能看到日志
try:
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8')
except Exception:
    pass

print("==========================================", flush=True)
print("      星闪心率监测系统 - 启动程序", flush=True)
print("==========================================", flush=True)
print("[1/6] 核心环境初始化...", flush=True)

try:
    import tkinter as tk
    from tkinter import ttk, scrolledtext, messagebox
    print("[2/6] GUI 框架 (Tkinter) 加载成功", flush=True)
except ImportError as e:
    print(f"!!! 致命错误: Tkinter 加载失败: {e}", flush=True)
    input("按回车键退出...")
    sys.exit(1)

# 全局占位符，稍后动态加载
serial = None
np = None
deque = None
OpenAI = pd = joblib = None # 新增 pandas 和 joblib
plt = None
Figure = None
FigureCanvasTkAgg = None
list_ports = None

# 标志位
libs_loaded = False
load_error = None

def load_heavy_modules():
    global serial, np, deque, OpenAI, pd, joblib, plt, Figure, FigureCanvasTkAgg, list_ports, libs_loaded, load_error
    try:
        print("[3/6] 正在加载计算引擎 (NumPy/Pandas)...", flush=True)
        import numpy
        np = numpy
        import pandas
        pd = pandas
        import joblib as jl
        joblib = jl
        from collections import deque as dq
        deque = dq
        
        print("[4/6] 正在加载绘图引擎 (Matplotlib)... 这可能需要几秒钟", flush=True)
        import matplotlib
        matplotlib.use('TkAgg')
        import matplotlib.pyplot as pyplot
        plt = pyplot
        from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg as FCT
        FigureCanvasTkAgg = FCT
        from matplotlib.figure import Figure as Fig
        Figure = Fig
        
        # 配置字体 (增加对 SimHei 的兼容，若失败则回退 Arial)
        try:
            plt.rcParams['font.sans-serif'] = ['SimHei', 'Microsoft YaHei', 'Arial']
            plt.rcParams['axes.unicode_minus'] = False
        except:
            pass
        
        print("[5/6] 正在加载通信模块 (Serial/OpenAI)...", flush=True)
        import serial as ser
        import serial.tools.list_ports as lp
        serial = ser
        list_ports = lp
        from openai import OpenAI as AI
        OpenAI = AI
        
        # 导入专家给的解析模块
        global split_frames, parse_payload_to_samples, AdaptiveThreshold, detect_peaks, compute_rmssd
        from binary_frame_parser import split_frames, parse_payload_to_samples
        from adaptive_threshold import AdaptiveThreshold
        from robust_peak_and_rmssd import detect_peaks, compute_rmssd
        
        print("[6/6] 所有模块加载完成！准备进入主界面...", flush=True)
        libs_loaded = True
    except Exception as e:
        import traceback
        err = traceback.format_exc()
        print(f"!!! 模块加载失败: {err}", flush=True)
        load_error = str(e)

# --- 调试日志功能 (保留但简化) ---
def log_debug(msg):
    print(f"[LOG] {msg}", flush=True)

# 全局变量
is_running = True

# ================= 优化配置 ================= 
BAUD_RATE = 115200 
MAX_POINTS = 500 # 增加绘图点数，展示更多细节
REFRESH_INTERVAL = 50   
ANALYSIS_WINDOW = 10 # 匹配 v1.2 的 WINDOW_SIZE
SMOOTHING_WINDOW = 5 # 滑动平均窗口长度
MIN_VALID_BPM = 45      
MAX_VALID_BPM = 120     
# 峰值检测配置
PEAK_REFRACTORY_PERIOD = 0.3 # 300ms 最小间隔 (相当于最高 200 BPM)
RR_WINDOW = 20 # 用于 RMSSD 计算的 RR 间期数量
SAMPLE_RATE = 100 # 采样率 (增加宽容度优化)
HEADER_BYTES = b'\x51\x25\x54\x16\xe3\xbe' # 更新后的帧头

# ================= AI 信号评估模块 (SQI Module) =================
class HeartAI:
    def __init__(self, model_path='heart_model.pkl'):
        self.model = None
        self.model_path = model_path
        # 延迟加载模型，防止主线程卡顿
        threading.Thread(target=self._load_model, daemon=True).start()

    def _load_model(self):
        if os.path.exists(self.model_path):
            try:
                import joblib as jl
                self.model = jl.load(self.model_path)
                log_debug(f"AI 模型加载成功: {self.model_path}")
            except Exception as e:
                log_debug(f"AI 模型加载失败: {e}")

    def classify_heuristic(self, raw_val, std_val, bpm=0, rmssd=0, vmin=0, vmax=0):
        """硬逻辑拦截 (Priority Logic) v5.0"""
        # 1. 紧急状态判定 (心律失常)
        if 45 <= bpm <= 120 and rmssd > 25: # RMSSD > 25ms 判定为心律失常
            return "ARRHYTHMIA", 1

        # 2. 正常逻辑维持 (拓宽健康判定范围)
        if 45 <= bpm <= 120:
            # 增加信号幅值容错 (5mv - 400mv)
            if 5 <= (vmax - vmin) <= 400:
                return "HEALTHY", 0
            elif raw_val >= 100:
                return "HEALTHY", 0

        if raw_val < 80:
            return "DISCONNECTED", 1 # 脱落
        if std_val < 10.0 and raw_val > 120:
            return "HEALTHY", 0      # 更强的安静判定
        if std_val > 50:
            return "INTERFERENCE", 1 # 干扰 (增加 STD 容错)
        return "HEALTHY", 0

    def predict(self, raw_val, std_val, bpm=0, rmssd=0, vmin=0, vmax=0):
        """实时推理与分类"""
        # 1. 启发式拦截 (结合实时 BPM 和 RMSSD 以及 vmin/vmax)
        status, label = self.classify_heuristic(raw_val, std_val, bpm, rmssd, vmin, vmax)
        
        # 2. 如果启发式判定为健康且模型可用，进一步细分
        if status == "HEALTHY" and self.model:
            try:
                import pandas as pd
                X = pd.DataFrame([[raw_val, std_val]], columns=['Raw_Value', 'STD_10'])
                pred_label = self.model.predict(X)[0]
                if pred_label == 1:
                    # 即使模型说是干扰，如果有稳定的 BPM，依然保持 HEALTHY
                    if not (40 <= bpm <= 180):
                        status = "INTERFERENCE"
                        label = 1
            except:
                pass
        return status, label

# ================= 全局变量 ================= 
heart_ai = HeartAI() # 初始化 AI 引擎
current_status = "INITIALIZING..."
last_logged_status = None # 新增：用于防止重复日志打印
is_ignoring_artifact = False # 新增：用于用户手动忽略误报
current_std = 0.0
current_rmssd = 0.0 # 新增：实时 RMSSD
healthy_count = 0
total_data_points = 0
deepseek_btn_ready = False
current_user_context = "Resting" # 新增：用户场景

# 心动过速/过缓报警相关
alert_timer_start = None
is_alerting = False

# --- 核心变量 ---
filtered_buffer = None # 稍后初始化 (用于绘图)
bpm_analysis_cache = None # 稍后初始化 (用于计算 STD)
current_mode = "静坐" 
ai_advice = "等待数据接入..." 
ai_is_thinking = False
last_ai_query_time = 0
AI_QUERY_INTERVAL = 5  
current_bpm = 0 # 原始波形值 (或解析出的 BPM)
serial_thread = None
current_app = None # 全局引用，用于回调 GUI

# ================= DeepSeek API 配置 ================= 
DEEPSEEK_KEY = "sk-ad9f1a09a337403ebe347c755a8733b1" 
client = None # 稍后初始化

# ================= 串口读取 (二进制专家版 v5.0) ================= 
def read_serial(port): 
    global current_bpm, current_status, current_std, current_rmssd, healthy_count, total_data_points
    global deepseek_btn_ready, is_running, alert_timer_start, is_alerting, last_logged_status
    
    # --- 信号处理与算法变量 ---
    serial_buffer = bytearray()
    adaptive_th = AdaptiveThreshold(timeout_seconds=5.0)
    
    # RR 间期分析
    last_peak_time_global = 0
    rr_intervals = deque(maxlen=RR_WINDOW)
    bpm_history = deque(maxlen=5)
    real_bpm = 0
    frame_count = 0
    status_counter = 0 # 状态平滑计数器 (用于抗震荡升级)
    
    try: 
        ser = serial.Serial(port, BAUD_RATE, timeout=0.01) 
        ser.reset_input_buffer()
        log_debug(f"串口 {port} 已成功打开，进入二进制解析模式。")
    except Exception as e: 
        log_debug(f"串口占用或错误: {e}")
        if current_app:
            current_app.chat_history.insert(tk.END, f"System Error: 串口 {port} 无法打开。\n")
        return 

    timestamp_str = time.strftime("%Y%m%d_%H%M%S")
    filename = f"ai_factory_log_{timestamp_str}.csv"
    
    try:
        with open(filename, 'w', newline='') as csvfile:
            writer = csv.writer(csvfile)
            writer.writerow(['Time', 'Raw_Voltage', 'Smoothed_Voltage', 'STD_10', 'RMSSD', 'Status', 'Label', 'BPM'])

            while is_running: 
                try: 
                    if ser.in_waiting: 
                        # 1. 串口读取与 buffer 追加
                        new_bytes = ser.read(ser.in_waiting)
                        serial_buffer += new_bytes
                        
                        # 2. 帧对齐与拆分
                        frames = split_frames(serial_buffer, header=HEADER_BYTES)
                        if not frames:
                            continue
                        
                        # 处理解析得到的每一帧
                        for i, frame in enumerate(frames):
                            frame_count += 1
                            try:
                                # 3. 调用专家解析器
                                info = parse_payload_to_samples(frame)
                                samples = info['samples']
                                dtype = info['dtype']
                                vmin, vmax = info['vmin'], info['vmax']
                                
                                # 调试日志 (专家要求：Frame ID, len, dtype, vmin, vmax, samples[:10])
                                if frame_count % 100 == 1: # 降低打印频率，每 100 帧打印一次
                                    print(f"[DEBUG] Frame {frame_count}, len={len(frame)}, dtype={dtype}, vmin={vmin}, vmax={vmax}, samples={samples[:10]}", flush=True)
                                
                                if len(samples) == 0:
                                    continue
                                
                                # 4. 核心算法：对整帧进行峰值检测
                                current_th = adaptive_th.get_threshold()
                                peak_indices = detect_peaks(
                                    samples, 
                                    sample_rate=SAMPLE_RATE, 
                                    threshold=current_th,
                                    min_distance_s=0.4
                                )
                                
                                # 处理检测到的峰
                                for p_idx in peak_indices:
                                    peak_amp = samples[p_idx]
                                    now = time.time()
                                    adaptive_th.note_peak(peak_amp, now)
                                    if last_peak_time_global > 0:
                                        rr_ms = (now - last_peak_time_global) * 1000
                                        if 300 <= rr_ms <= 1500: # 40-200 BPM
                                            rr_intervals.append(rr_ms)
                                            instant_bpm = 60000.0 / rr_ms
                                            bpm_history.append(instant_bpm)
                                    last_peak_time_global = now

                                # 5. 信号处理流水线：处理每个数据点 (用于绘图与 AI)
                                for raw_val in samples:
                                    current_bpm = int(raw_val)
                                    filtered_buffer.append(raw_val)
                                    bpm_analysis_cache.append(raw_val)
                                    
                                    # 6. 计算实时指标
                                    real_bpm = int(np.mean(list(bpm_history))) if bpm_history else 0
                                    current_rmssd, _ = compute_rmssd(list(rr_intervals))
                                    if np.isnan(current_rmssd): current_rmssd = 0.0
                                    
                                    # 7. AI 判定与状态更新
                                    if len(bpm_analysis_cache) < ANALYSIS_WINDOW:
                                        new_status = "INITIALIZING..."
                                        label = 0
                                        current_std = 0.0
                                    else:
                                        current_std = np.std(list(bpm_analysis_cache))
                                        # 增加 vmin/vmax 的输入
                                        raw_status, label = heart_ai.predict(raw_val, current_std, real_bpm, current_rmssd, vmin, vmax)
                                        
                                        # 状态平滑计数器 (抗震荡升级)
                                        if raw_status == "DISCONNECTED":
                                            status_counter -= 1 # 连续 15 帧解析失败才切 DISCONNECTED
                                            if status_counter < -15:
                                                new_status = "DISCONNECTED"
                                            else:
                                                new_status = current_status if current_status != "INITIALIZING..." else "HEALTHY"
                                        elif raw_status == "HEALTHY":
                                            status_counter += 1 # 连续 3 帧解析成功就立刻切 HEALTHY
                                            if status_counter >= 3:
                                                new_status = "HEALTHY"
                                                status_counter = 3 # 防止无限累加
                                            else:
                                                new_status = current_status
                                        else:
                                            new_status = raw_status
                                            status_counter = 0 # 干扰或心律不齐重置计数器
                                        
                                        # 8. 紧急警报判定 (拓宽判定范围)
                                        if (real_bpm > 115 or (real_bpm < 50 and real_bpm > 0)) and new_status != "DISCONNECTED":
                                            if alert_timer_start is None:
                                                alert_timer_start = time.time()
                                            elif time.time() - alert_timer_start > 5.0:
                                                is_alerting = True
                                        else:
                                            alert_timer_start = None
                                            is_alerting = False

                                        # 9. 统计与日志
                                        total_data_points += 1
                                        if new_status == "HEALTHY":
                                            healthy_count += 1
                                        deepseek_btn_ready = (new_status != "DISCONNECTED")

                                    # 状态切换 UI 提示
                                    final_status = "HEALTHY (Ignored)" if is_ignoring_artifact and new_status in ["INTERFERENCE", "ARRHYTHMIA"] else new_status
                                    current_status = final_status
                                    
                                    if current_status != last_logged_status:
                                        if current_app:
                                            current_app.chat_history.insert(tk.END, f"[{time.strftime('%H:%M:%S')}] Status: {last_logged_status} -> {current_status}\n")
                                            current_app.chat_history.see(tk.END)
                                        last_logged_status = current_status

                                    # 10. 记录数据
                                    writer.writerow([time.strftime("%H:%M:%S"), raw_val, raw_val, f"{current_std:.2f}", f"{current_rmssd:.1f}", current_status, label, real_bpm])
                                
                                global ai_advice
                                ai_advice = f"BPM: {real_bpm} | RMSSD: {current_rmssd:.1f}ms"
                                csvfile.flush()
                                
                                # 专家要求：每处理完一帧，pop 掉已处理的 buffer
                                last_header_pos = serial_buffer.rfind(HEADER_BYTES)
                                if len(frames) > 0:
                                    serial_buffer = serial_buffer[last_header_pos + len(frames[-1]):]
                                
                            except Exception as parse_err:
                                # 容错处理：尝试回退到旧的正则模式
                                try:
                                    line = frame.decode('utf-8', errors='ignore').strip()
                                    v_match = re.search(r"voltage:\s*(\d+)mv", line)
                                    if v_match:
                                        raw_val = int(v_match.group(1))
                                        # 这里可以执行简化的旧逻辑
                                        current_bpm = raw_val
                                        filtered_buffer.append(raw_val)
                                except: pass
                                continue
                except Exception as e:
                    # log_debug(f"Loop Error: {e}")
                    continue
            ser.close()
    except Exception as e:
        log_debug(f"文件保存错误: {e}")

# ================= GUI 主程序 ================= 
class MonitorApp:
    def __init__(self, root, port):
        log_debug("开始初始化 MonitorApp 界面")
        self.root = root
        self.root.configure(bg="#f5f6fa") 
        self.main_paned = ttk.PanedWindow(root, orient=tk.HORIZONTAL)
        self.main_paned.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)

        # 图表区 - 左侧容器
        self.left_frame = tk.Frame(self.main_paned, bg="white")
        self.main_paned.add(self.left_frame, weight=3)
        
        # 1. 顶部状态显示栏 (AI 增强版)
        self.status_bar = tk.Frame(self.left_frame, bg="#2f3640", height=80)
        self.status_bar.pack(side=tk.TOP, fill=tk.X, padx=5, pady=5)
        self.status_bar.pack_propagate(False)

        self.lbl_status_title = tk.Label(self.status_bar, text="AI 信号质量状态:", bg="#2f3640", fg="white", font=("Microsoft YaHei", 12))
        self.lbl_status_title.pack(side=tk.LEFT, padx=15)

        self.lbl_ai_status = tk.Label(self.status_bar, text="INITIALIZING...", bg="#2f3640", fg="#f1c40f", font=("Microsoft YaHei", 24, "bold"))
        self.lbl_ai_status.pack(side=tk.LEFT, padx=10)
        
        # v4.1 新增：右键菜单（忽略误报）
        self.status_menu = tk.Menu(self.root, tearoff=0)
        self.status_menu.add_command(label="标记为运动伪影 (忽略)", command=self.ignore_artifact)
        self.lbl_ai_status.bind("<Button-3>", self.show_status_menu)

        # 2. 核心指标面板
        self.metrics_frame = tk.Frame(self.left_frame, bg="white")
        self.metrics_frame.pack(side=tk.TOP, fill=tk.X, padx=5, pady=5)

        # 场景选择下拉菜单 (Context-Awareness)
        tk.Label(self.metrics_frame, text="监测场景:", bg="white", font=("Microsoft YaHei", 10)).pack(side=tk.LEFT, padx=10)
        self.combo_context = ttk.Combobox(self.metrics_frame, values=["Resting", "Light Exercise", "High Intensity"], 
                                         state="readonly", width=15)
        self.combo_context.current(0)
        self.combo_context.pack(side=tk.LEFT, padx=5)
        self.combo_context.bind("<<ComboboxSelected>>", self.on_context_change)

        # 数据质量进度条
        tk.Label(self.metrics_frame, text="健康占比:", bg="white", font=("Microsoft YaHei", 10)).pack(side=tk.LEFT, padx=10)
        self.quality_bar = ttk.Progressbar(self.metrics_frame, length=200, mode='determinate')
        self.quality_bar.pack(side=tk.LEFT, padx=10, pady=10)
        self.lbl_quality_val = tk.Label(self.metrics_frame, text="0%", bg="white", font=("Microsoft YaHei", 10, "bold"))
        self.lbl_quality_val.pack(side=tk.LEFT)

        # 3. 绘图区
        self.plot_frame = tk.Frame(self.left_frame, bg="white")
        self.plot_frame.pack(side=tk.TOP, fill=tk.BOTH, expand=True, padx=5, pady=5)

        self.fig = Figure(figsize=(8, 5), dpi=100, facecolor='white') 
        self.ax = self.fig.add_subplot(111)
        self.canvas = FigureCanvasTkAgg(self.fig, master=self.plot_frame)
        self.canvas.get_tk_widget().pack(side=tk.TOP, fill=tk.BOTH, expand=True)

        # 4. 底部控制栏
        self.bottom_control = tk.Frame(self.left_frame, bg="#f0f0f0")
        self.bottom_control.pack(side=tk.BOTTOM, fill=tk.X, padx=5, pady=5)

        self.btn_deepseek = tk.Button(self.bottom_control, text="生成 DeepSeek 专家诊断报告", 
                                     command=self.generate_report, state="disabled",
                                     bg="#bdc3c7", fg="white", font=("Microsoft YaHei", 12, "bold"), padx=20)
        self.btn_deepseek.pack(pady=10)

        # 聊天区
        self.right_frame = tk.Frame(self.main_paned, bg="#353b48")
        self.main_paned.add(self.right_frame, weight=1)
        
        self.chat_entry = tk.Entry(self.right_frame, font=("Microsoft YaHei", 10))
        self.chat_entry.pack(side=tk.BOTTOM, fill=tk.X, padx=5, pady=5)
        self.chat_entry.bind("<Return>", self.send_message)
        
        self.chat_history = scrolledtext.ScrolledText(self.right_frame, bg="#2f3640", fg="white", font=("Microsoft YaHei", 10))
        self.chat_history.pack(side=tk.TOP, fill=tk.BOTH, expand=True, padx=5, pady=5)
        self.chat_history.insert(tk.END, "=== 星闪健康助手 v2.0 ===\n[AI 驱动] 信号质量实时监测已启动。\n[专家系统] 健康状态满10秒即可解锁诊断。\n\n")

        if port:
            threading.Thread(target=read_serial, args=(port,), daemon=True).start()
        
        global current_app
        current_app = self
        
        self.animate_counter = 0 # 用于降低 UI 刷新率
        self.animate()
        log_debug("MonitorApp 界面初始化完成")

    def on_context_change(self, event):
        global current_user_context
        current_user_context = self.combo_context.get()
        self.chat_history.insert(tk.END, f"System: 场景已切换为【{current_user_context}】\n")

    def show_status_menu(self, event):
        self.status_menu.post(event.x_root, event.y_root)

    def ignore_artifact(self):
        global is_ignoring_artifact
        is_ignoring_artifact = True
        self.chat_history.insert(tk.END, "System: 已标记为运动伪影，异常检测灵敏度已降低。\n")
        self.root.after(10000, self.reset_ignore) # 10秒后自动恢复检测

    def reset_ignore(self):
        global is_ignoring_artifact
        is_ignoring_artifact = False
        self.chat_history.insert(tk.END, "System: 已恢复正常异常监测。\n")

    def generate_report(self):
        """生成 DeepSeek 诊断报告逻辑 v4.1 (增加准备进度)"""
        self.btn_deepseek.config(state="disabled", text="正在整理数据...")
        
        def _task():
            time.sleep(1.5) # 模拟数据整理过程
            # 提取统计特征 (Payload 准备)
            recent_raw = list(filtered_buffer)[-300:] 
            avg_raw = np.mean(recent_raw) if recent_raw else 0
            
            # 动态 Prompt 封装
            real_bpm = ai_advice.split('BPM: ')[1].split(' |')[0] if 'BPM:' in ai_advice else "0"
            bpm_val = int(real_bpm)
            
            custom_msg = ""
            if current_user_context == "Resting" and bpm_val > 100:
                custom_msg = f"警告：用户处于静息状态，但心率高达 {bpm_val}，疑似心动过速。"
            elif current_user_context == "High Intensity" and bpm_val < 80:
                custom_msg = f"注意：用户处于高强度运动，但心率仅为 {bpm_val}，请检查运动状态或传感器贴合度。"
                
            payload = {
                "bpm": bpm_val,
                "rmssd": f"{current_rmssd:.1f}ms",
                "status": current_status,
                "user_context": current_user_context,
                "analysis": custom_msg if custom_msg else "常规健康分析",
                "recent_avg_voltage": f"{avg_raw:.1f}mv"
            }
            
            report_prompt = (f"请基于以下 JSON 数据生成一份专业的心血管健康评估报告：\n{payload}\n"
                             f"如果是 ARRHYTHMIA 状态，请特别关注 RMSSD 指标。如果是紧急心率，请给出行动指南。")
            
            self.root.after(0, lambda: self.chat_history.insert(tk.END, f"我: [场景:{current_user_context}] 生成智能诊断报告。\n"))
            self.ask_ai(report_prompt)
            self.root.after(0, lambda: self.btn_deepseek.config(state="normal", text="生成 DeepSeek 专家诊断报告"))

        threading.Thread(target=_task, daemon=True).start()

    def update_chat_with_ai(self, text):
        def _update():
            self.chat_history.insert(tk.END, f"AI 专家: {text}\n\n")
            self.chat_history.see(tk.END)
        self.root.after(0, _update)

    def send_message(self, event=None):
        msg = self.chat_entry.get()
        if msg:
            self.chat_history.insert(tk.END, f"我: {msg}\n")
            self.chat_entry.delete(0, tk.END)
            threading.Thread(target=self.ask_ai, args=(msg,), daemon=True).start()
            
    def ask_ai(self, user_msg):
        try:
            context = f"当前状态: {current_status}, 信号波动 STD: {current_std:.2f}。"
            system_prompt = "你是一位专业的心血管内科医生。请基于数据给出严谨的分析建议。"
            
            if client:
                response = client.chat.completions.create(
                    model="deepseek-chat",
                    messages=[
                        {"role": "system", "content": system_prompt},
                        {"role": "user", "content": f"【实时数据: {context}】\n用户问题: {user_msg}"},
                    ],
                    stream=False
                )
                reply = response.choices[0].message.content
                self.root.after(0, lambda: self.chat_history.insert(tk.END, f"AI: {reply}\n\n"))
        except Exception as e:
            self.root.after(0, lambda: self.chat_history.insert(tk.END, f"System: AI 服务繁忙，请稍后再试。\n"))

    def animate(self):
        if is_running:
            try:
                self.update_ui_elements()
                self.animate_counter += 1
                if self.animate_counter >= 10: # 每收到 10 帧数据才更新一次画布 (降低 UI 刷新率)
                    self.update_plot()
                    self.animate_counter = 0
            except: pass
            self.root.after(50, self.animate)

    def update_ui_elements(self):
        """更新状态文本、进度条和按钮 v4.1"""
        # 1. 状态大文本和颜色
        status_colors = {
            "HEALTHY": "#00b894",      
            "INTERFERENCE": "#f1c40f", 
            "DISCONNECTED": "#d63031", 
            "INITIALIZING...": "#3498db",
            "ARRHYTHMIA": "#9b59b6",
            "HEALTHY (Ignored)": "#00b894"
        }
        color = status_colors.get(current_status, "white")
        
        # 处理忽略状态的显示
        display_status = current_status
        if "Ignored" in current_status:
            display_status = "HEALTHY 🛡️"
            
        self.lbl_ai_status.config(text=display_status, fg=color)

        # 2. 紧急警报静默可视化 (Urgent Alert)
        if is_alerting:
            # 呼吸灯效果：基于时间计算 alpha
            alpha = (np.sin(time.time() * 5) + 1) / 2 # 0 to 1
            # 背景在深灰色和深橙色之间过渡
            self.status_bar.config(bg="#e67e22" if alpha > 0.5 else "#2f3640")
        else:
            self.status_bar.config(bg="#2f3640")

        # 3. 数据质量进度条
        if total_data_points > 0:
            quality = (healthy_count / total_data_points) * 100
            self.quality_bar['value'] = quality
            self.lbl_quality_val.config(text=f"{quality:.1f}%")

        # 4. DeepSeek 按钮呼吸闪烁逻辑 (Smart Trigger)
        if deepseek_btn_ready:
            # 只要不是 DISCONNECTED，按钮就可用
            self.btn_deepseek.config(state="normal")
            
            # 如果是异常状态，增加呼吸闪烁吸引注意
            if current_status in ["ARRHYTHMIA", "INTERFERENCE"]:
                alpha = (np.sin(time.time() * 8) + 1) / 2
                blink_color = "#0984e3" if alpha > 0.5 else "#3498db"
                self.btn_deepseek.config(bg=blink_color)
            else:
                self.btn_deepseek.config(bg="#0984e3")
        else:
            self.btn_deepseek.config(state="disabled", bg="#bdc3c7")

    def update_plot(self):
        if len(filtered_buffer) < 2: return
        self.ax.clear()
        
        status_colors = {
            "HEALTHY": "#00b894",
            "INTERFERENCE": "#f1c40f",
            "DISCONNECTED": "#d63031",
            "INITIALIZING...": "#3498db",
            "ARRHYTHMIA": "#9b59b6"
        }
        plot_color = status_colors.get(current_status, "#00b894")
        
        self.ax.plot(list(filtered_buffer), color=plot_color, linewidth=1.5) 
        
        # 统一使用英文标签，彻底解决乱码问题
        calc_bpm = ai_advice.split('BPM: ')[1].split(' |')[0] if 'BPM:' in ai_advice else 'Detecting'
        rmssd_val = f"{current_rmssd:.1f}ms"
        title_text = f"Volt: {current_bpm}mv | BPM: {calc_bpm} | RMSSD: {rmssd_val} | SQI: {current_status}"
        
        if current_status == "DISCONNECTED":
            title_text = "[!] DISCONNECTED - " + title_text
        elif current_status == "ARRHYTHMIA":
            title_text = "[V] ARRHYTHMIA - " + title_text
            
        self.ax.set_title(title_text, color=plot_color, fontsize=10, fontweight='bold')
        self.ax.set_ylim(0, 300) 
        self.ax.set_xlim(0, 200) # 显示 2 秒波形 (100Hz 采样率下)
        self.ax.set_xlabel("Samples (Smoothed)")
        self.ax.set_ylabel("Voltage (mv)")
        self.ax.grid(True, linestyle='--', alpha=0.3) 
        self.canvas.draw()

def main():
    # 创建加载窗口
    root = tk.Tk()
    root.title("星闪心率监测系统 Pro - 启动中")
    root.geometry("400x150")
    root.configure(bg="#2b2b2b")
    
    # 居中
    screen_width = root.winfo_screenwidth()
    screen_height = root.winfo_screenheight()
    x = (screen_width - 400) // 2
    y = (screen_height - 150) // 2
    root.geometry(f"+{x}+{y}")
    
    lbl_status = tk.Label(root, text="正在初始化运行环境...", bg="#2b2b2b", fg="white", font=("Microsoft YaHei", 12))
    lbl_status.pack(expand=True)
    
    progress = ttk.Progressbar(root, length=300, mode='indeterminate')
    progress.pack(pady=20)
    progress.start(10)
    
    def check_load_status():
        if libs_loaded:
            try:
                progress.stop()
                root.destroy()
            except:
                pass
            start_main_app()
        elif load_error:
            try:
                progress.stop()
                messagebox.showerror("启动失败", f"模块加载发生错误:\n{load_error}")
                root.destroy()
            except:
                pass
            sys.exit(1)
        else:
            try:
                if root.winfo_exists():
                    root.after(100, check_load_status)
            except:
                pass
            
    # 在后台线程加载重型库
    threading.Thread(target=load_heavy_modules, daemon=True).start()
    
    # 开始轮询加载状态
    root.after(100, check_load_status)
    root.mainloop()

def start_main_app():
    # 初始化全局对象
    global filtered_buffer, bpm_analysis_cache, client
    filtered_buffer = deque(maxlen=MAX_POINTS)
    bpm_analysis_cache = deque(maxlen=ANALYSIS_WINDOW)
    client = OpenAI(api_key=DEEPSEEK_KEY, base_url="https://api.deepseek.com")
    
    # 导入 re 和 csv (这些很快，可以在这里导入)
    global re, csv
    import re
    import csv

    _main_logic()

def _main_logic():
    log_debug("初始化 Tkinter root 窗口")
    root = tk.Tk()
    root.title("星闪心率监测系统 Pro - 启动配置")
    root.geometry("500x300")
    root.configure(bg="#2b2b2b")
    
    # 居中显示
    screen_width = root.winfo_screenwidth()
    screen_height = root.winfo_screenheight()
    x = (screen_width - 500) // 2
    y = (screen_height - 300) // 2
    root.geometry(f"+{x}+{y}")

    ports = list(list_ports.comports())
    
    # --- 界面构建函数 ---
    def setup_selection_ui():
        for widget in root.winfo_children():
            widget.destroy()
            
        root.title("星闪心率监测系统 Pro v1.5 - 选择设备")
        
        tk.Label(root, text="欢迎使用星闪心率监测系统 v1.5", bg="#2b2b2b", fg="#00d2d3", font=("Microsoft YaHei", 16, "bold")).pack(pady=20)
        tk.Label(root, text="请选择连接传感器的串口：", bg="#2b2b2b", fg="white", font=("Microsoft YaHei", 12)).pack(pady=10)
        
        # 串口下拉框
        port_values = [f"{p.device} - {p.description}" for p in ports]
        combo = ttk.Combobox(root, values=port_values, font=("Microsoft YaHei", 10), width=40)
        combo.pack(pady=10, padx=20)
        if port_values:
            combo.current(0)
        else:
            combo.set("未检测到设备 (将进入演示模式)")
            
        def on_confirm():
            selection = combo.get()
            selected_port = None
            if selection and "未检测到设备" not in selection:
                selected_port = selection.split(" - ")[0]
            
            # 清空界面，进入主功能
            for widget in root.winfo_children():
                widget.destroy()
            
            # 调整窗口大小适应主程序
            root.geometry("1400x800")
            x = (screen_width - 1400) // 2
            y = (screen_height - 800) // 2
            root.geometry(f"+{x}+{y}")
            
            # 初始化主程序
            app = MonitorApp(root, selected_port)

        btn_confirm = tk.Button(root, text="启动系统", command=on_confirm, bg="#0984e3", fg="white", font=("Microsoft YaHei", 12, "bold"), width=15)
        btn_confirm.pack(pady=30)

    # 启动流程
    setup_selection_ui()
    
    # 处理窗口关闭
    def on_closing():
        global is_running
        is_running = False
        try:
            root.destroy()
        except:
            pass
        os._exit(0)

    root.protocol("WM_DELETE_WINDOW", on_closing)
    root.mainloop()

if __name__ == "__main__":
    # Windows 平台下的多进程支持修复 (PyInstaller 打包必备)
    import multiprocessing
    multiprocessing.freeze_support()
    main()