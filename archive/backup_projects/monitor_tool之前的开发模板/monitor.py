"""
星闪智能健康监测系统 - 上位机监控工具 v2.0
支持解析 lib 发送的 JSON 格式数据：
  {"hr":72,"spo2":98,"temp":36.5,"accel":[x,y,z],"gyro":[x,y,z],
   "fall_conf":0,"status":"Status: Normal","hr_source":"max30102",
   "valid":{"hr":true,"spo2":true,"temp":true,"imu":true}}
"""

import sys
import os
import time
import threading
import json
import warnings

warnings.filterwarnings("ignore", category=UserWarning)

class NullWriter:
    def write(self, text): pass
    def flush(self): pass
    def reconfigure(self, *args, **kwargs): pass

if sys.stdout is None:
    sys.stdout = NullWriter()
if sys.stderr is None:
    sys.stderr = NullWriter()

try:
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8')
except Exception:
    pass

print("==========================================", flush=True)
print("  星闪智能健康监测系统 v2.0", flush=True)
print("==========================================", flush=True)
print("[1/5] 核心环境初始化...", flush=True)

try:
    import tkinter as tk
    from tkinter import ttk, scrolledtext, messagebox
    print("[2/5] GUI 框架加载成功", flush=True)
except ImportError as e:
    print(f"!!! 致命错误: Tkinter 加载失败: {e}", flush=True)
    input("按回车键退出...")
    sys.exit(1)

serial = None
np = None
deque = None
plt = None
Figure = None
FigureCanvasTkAgg = None
list_ports = None
libs_loaded = False
load_error = None

def load_heavy_modules():
    global serial, np, deque, plt, Figure, FigureCanvasTkAgg, list_ports, libs_loaded, load_error
    try:
        print("[3/5] 正在加载计算引擎 (NumPy)...", flush=True)
        import numpy
        np = numpy
        from collections import deque as dq
        deque = dq

        print("[4/5] 正在加载绘图引擎 (Matplotlib)...", flush=True)
        import matplotlib
        matplotlib.use('TkAgg')
        import matplotlib.pyplot as pyplot
        plt = pyplot
        from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg as FCT
        FigureCanvasTkAgg = FCT
        from matplotlib.figure import Figure as Fig
        Figure = Fig
        try:
            plt.rcParams['font.sans-serif'] = ['SimHei', 'Microsoft YaHei', 'Arial']
            plt.rcParams['axes.unicode_minus'] = False
        except:
            pass

        print("[5/5] 正在加载通信模块 (Serial)...", flush=True)
        import serial as ser
        import serial.tools.list_ports as lp
        serial = ser
        list_ports = lp

        libs_loaded = True
        print("所有模块加载完成！", flush=True)
    except Exception as e:
        import traceback
        load_error = str(e)
        print(f"!!! 模块加载失败: {traceback.format_exc()}", flush=True)

def log_debug(msg):
    print(f"[LOG] {msg}", flush=True)

# ================= 配置 =================
BAUD_RATE = 115200
MAX_POINTS = 300
REFRESH_INTERVAL = 200

# ================= 全局变量 =================
is_running = True
current_app = None

# 传感器数据
g_hr = 0
g_spo2 = 0
g_temp = 0.0
g_accel = [0.0, 0.0, 0.0]
g_gyro = [0.0, 0.0, 0.0]
g_fall_conf = 0
g_status_text = "Normal"
g_hr_source = "max30102"
g_valid = {"hr": False, "spo2": False, "temp": False, "imu": False}

# 历史数据（用于绘图）
hr_history = None
spo2_history = None
temp_history = None
accel_mag_history = None

# AI 相关
ai_advice = "等待数据..."
ai_is_thinking = False
DEEPSEEK_KEY = "sk-ad9f1a09a337403ebe347c755a8733b1"
client = None

# ================= JSON 数据解析 =================
def parse_json_line(line):
    """解析 lib 发送的 JSON 行数据"""
    global g_hr, g_spo2, g_temp, g_accel, g_gyro, g_fall_conf, g_status_text, g_hr_source, g_valid
    try:
        data = json.loads(line.strip())
        g_hr = int(data.get("hr", 0))
        g_spo2 = int(data.get("spo2", 0))
        g_temp = float(data.get("temp", 0))
        g_accel = list(data.get("accel", [0, 0, 0]))
        g_gyro = list(data.get("gyro", [0, 0, 0]))
        g_fall_conf = int(data.get("fall_conf", 0))
        g_status_text = str(data.get("status", "Unknown"))
        g_hr_source = str(data.get("hr_source", "max30102"))
        g_valid = data.get("valid", {"hr": False, "spo2": False, "temp": False, "imu": False})
        return True
    except (json.JSONDecodeError, ValueError, KeyError):
        return False

# ================= 串口读取 =================
# 全局串口对象（用于发送命令）
serial_port_obj = None

def read_serial(port):
    global g_hr, g_spo2, g_temp, ai_advice, is_running, serial_port_obj

    try:
        ser = serial.Serial(port, BAUD_RATE, timeout=0.1)
        ser.reset_input_buffer()
        serial_port_obj = ser  # 保存引用，供 send_command 使用
        log_debug(f"串口 {port} 已打开")
    except Exception as e:
        log_debug(f"串口错误: {e}")
        if current_app:
            current_app.chat_history.insert(tk.END, f"串口 {port} 无法打开: {e}\n")
        return

    timestamp_str = time.strftime("%Y%m%d_%H%M%S")
    filename = f"health_data_{timestamp_str}.csv"
    line_buffer = ""

    try:
        with open(filename, 'w', newline='', encoding='utf-8') as csvfile:
            import csv
            writer = csv.writer(csvfile)
            writer.writerow(['Time', 'HR', 'SpO2', 'Temp', 'Accel_X', 'Accel_Y', 'Accel_Z',
                             'Gyro_X', 'Gyro_Y', 'Gyro_Z', 'Fall_Conf', 'Status', 'HR_Source',
                             'Valid_HR', 'Valid_SpO2', 'Valid_Temp', 'Valid_IMU'])

            while is_running:
                try:
                    if ser.in_waiting:
                        raw = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
                        line_buffer += raw

                        while '\n' in line_buffer:
                            line, line_buffer = line_buffer.split('\n', 1)
                            line = line.strip()
                            if not line:
                                continue

                            if parse_json_line(line):
                                # 记录到历史
                                hr_history.append(g_hr if g_valid.get("hr") else None)
                                spo2_history.append(g_spo2 if g_valid.get("spo2") else None)
                                temp_history.append(g_temp if g_valid.get("temp") else None)
                                accel_mag = (g_accel[0]**2 + g_accel[1]**2 + g_accel[2]**2)**0.5
                                accel_mag_history.append(accel_mag)

                                # 写入 CSV
                                writer.writerow([
                                    time.strftime("%H:%M:%S"),
                                    g_hr, g_spo2, f"{g_temp:.1f}",
                                    f"{g_accel[0]:.2f}", f"{g_accel[1]:.2f}", f"{g_accel[2]:.2f}",
                                    f"{g_gyro[0]:.2f}", f"{g_gyro[1]:.2f}", f"{g_gyro[2]:.2f}",
                                    g_fall_conf, g_status_text, g_hr_source,
                                    g_valid.get("hr"), g_valid.get("spo2"),
                                    g_valid.get("temp"), g_valid.get("imu")
                                ])
                                csvfile.flush()

                                ai_advice = f"HR:{g_hr} SpO2:{g_spo2}% Temp:{g_temp:.1f}C"
                except Exception as e:
                    continue

            ser.close()
    except Exception as e:
        log_debug(f"文件保存错误: {e}")

# ================= GUI =================
class HealthMonitorApp:
    def __init__(self, root, port):
        self.root = root
        self.root.configure(bg="#1a1a2e")
        self.port = port

        # 主布局
        self.main_paned = ttk.PanedWindow(root, orient=tk.HORIZONTAL)
        self.main_paned.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        # ===== 左侧：数据显示区 =====
        self.left_frame = tk.Frame(self.main_paned, bg="#16213e")
        self.main_paned.add(self.left_frame, weight=3)

        # 顶部状态栏
        self._build_status_bar()

        # 核心指标面板
        self._build_metrics_panel()

        # 图表区
        self._build_plot_area()

        # 底部控制栏
        self._build_control_bar()

        # ===== 右侧：AI 聊天区 =====
        self.right_frame = tk.Frame(self.main_paned, bg="#0f3460")
        self.main_paned.add(self.right_frame, weight=1)
        self._build_chat_area()

        # 启动串口线程
        if port:
            threading.Thread(target=read_serial, args=(port,), daemon=True).start()

        global current_app, hr_history, spo2_history, temp_history, accel_mag_history
        current_app = self
        hr_history = deque(maxlen=MAX_POINTS)
        spo2_history = deque(maxlen=MAX_POINTS)
        temp_history = deque(maxlen=MAX_POINTS)
        accel_mag_history = deque(maxlen=MAX_POINTS)

        self.animate()

    def _build_status_bar(self):
        bar = tk.Frame(self.left_frame, bg="#0f3460", height=60)
        bar.pack(side=tk.TOP, fill=tk.X, padx=5, pady=5)
        bar.pack_propagate(False)

        tk.Label(bar, text="系统状态:", bg="#0f3460", fg="#a0a0a0",
                 font=("Microsoft YaHei", 10)).pack(side=tk.LEFT, padx=10)
        self.lbl_status = tk.Label(bar, text="等待数据...", bg="#0f3460", fg="#f1c40f",
                                   font=("Microsoft YaHei", 18, "bold"))
        self.lbl_status.pack(side=tk.LEFT, padx=10)

        # 跌倒置信度
        tk.Label(bar, text="跌倒:", bg="#0f3460", fg="#a0a0a0",
                 font=("Microsoft YaHei", 10)).pack(side=tk.RIGHT, padx=10)
        self.lbl_fall = tk.Label(bar, text="0%", bg="#0f3460", fg="#00b894",
                                 font=("Microsoft YaHei", 14, "bold"))
        self.lbl_fall.pack(side=tk.RIGHT)

        # 数据有效性指示
        self.lbl_valid = tk.Label(bar, text="", bg="#0f3460", fg="#a0a0a0",
                                  font=("Microsoft YaHei", 9))
        self.lbl_valid.pack(side=tk.RIGHT, padx=15)

    def _build_metrics_panel(self):
        frame = tk.Frame(self.left_frame, bg="#1a1a2e")
        frame.pack(side=tk.TOP, fill=tk.X, padx=5, pady=5)

        # 心率卡片
        self._make_card(frame, "HR", "#e74c3c", "hr")

        # 血氧卡片
        self._make_card(frame, "SpO2", "#3498db", "spo2")

        # 温度卡片
        self._make_card(frame, "Temp", "#f39c12", "temp")

        # 加速度卡片
        self._make_card(frame, "Accel", "#2ecc71", "accel")

    def _make_card(self, parent, label, color, key):
        card = tk.Frame(parent, bg=color, padx=10, pady=5)
        card.pack(side=tk.LEFT, padx=5, expand=True, fill=tk.X)

        tk.Label(card, text=label, bg=color, fg="white",
                 font=("Microsoft YaHei", 9)).pack()
        lbl = tk.Label(card, text="--", bg=color, fg="white",
                       font=("Consolas", 18, "bold"))
        lbl.pack()
        setattr(self, f"lbl_{key}", lbl)

    def _build_plot_area(self):
        self.plot_frame = tk.Frame(self.left_frame, bg="#1a1a2e")
        self.plot_frame.pack(side=tk.TOP, fill=tk.BOTH, expand=True, padx=5, pady=5)

        self.fig = Figure(figsize=(8, 4), dpi=100, facecolor='#1a1a2e')
        self.ax1 = self.fig.add_subplot(211)
        self.ax2 = self.fig.add_subplot(212)
        self.fig.tight_layout(pad=2.0)

        self.canvas = FigureCanvasTkAgg(self.fig, master=self.plot_frame)
        self.canvas.get_tk_widget().pack(side=tk.TOP, fill=tk.BOTH, expand=True)

        for ax in [self.ax1, self.ax2]:
            ax.set_facecolor('#16213e')
            ax.tick_params(colors='#a0a0a0')
            ax.spines['bottom'].set_color('#a0a0a0')
            ax.spines['top'].set_color('#a0a0a0')
            ax.spines['left'].set_color('#a0a0a0')
            ax.spines['right'].set_color('#a0a0a0')

    def _build_control_bar(self):
        bar = tk.Frame(self.left_frame, bg="#0f3460", height=60)
        bar.pack(side=tk.BOTTOM, fill=tk.X, padx=5, pady=5)
        bar.pack_propagate(False)

        btn_style = {"font": ("Microsoft YaHei", 9), "padx": 10, "pady": 3, "bd": 0}

        tk.Button(bar, text="开始呼吸引导", bg="#3498db", fg="white",
                  command=lambda: self.send_command("breath_start"), **btn_style).pack(side=tk.LEFT, padx=5)
        tk.Button(bar, text="停止呼吸引导", bg="#e67e22", fg="white",
                  command=lambda: self.send_command("breath_stop"), **btn_style).pack(side=tk.LEFT, padx=5)
        tk.Button(bar, text="静音", bg="#9b59b6", fg="white",
                  command=lambda: self.send_command("mute"), **btn_style).pack(side=tk.LEFT, padx=5)
        tk.Button(bar, text="确认报警", bg="#e74c3c", fg="white",
                  command=lambda: self.send_command("confirm"), **btn_style).pack(side=tk.LEFT, padx=5)

        self.btn_ai = tk.Button(bar, text="AI 诊断报告", bg="#00b894", fg="white",
                                command=self.generate_report, state="disabled", **btn_style)
        self.btn_ai.pack(side=tk.RIGHT, padx=10)

    def _build_chat_area(self):
        tk.Label(self.right_frame, text="AI 健康助手", bg="#0f3460", fg="white",
                 font=("Microsoft YaHei", 11, "bold")).pack(pady=5)

        self.chat_history = scrolledtext.ScrolledText(self.right_frame, bg="#16213e", fg="white",
                                                      font=("Consolas", 9), wrap=tk.WORD)
        self.chat_history.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        self.chat_history.insert(tk.END, "=== 星闪智能健康监测 v2.0 ===\n")
        self.chat_history.insert(tk.END, "等待传感器数据接入...\n\n")

        input_frame = tk.Frame(self.right_frame, bg="#0f3460")
        input_frame.pack(fill=tk.X, padx=5, pady=5)
        self.chat_entry = tk.Entry(input_frame, font=("Microsoft YaHei", 10))
        self.chat_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 5))
        self.chat_entry.bind("<Return>", self.send_message)
        tk.Button(input_frame, text="发送", bg="#0984e3", fg="white",
                  command=self.send_message).pack(side=tk.RIGHT)

    def send_command(self, cmd):
        """发送命令到开发板"""
        global serial_port_obj
        if serial_port_obj is None or not serial_port_obj.is_open:
            self.chat_history.insert(tk.END, f"[CMD] {cmd} (串口未连接)\n")
            return
        try:
            serial_port_obj.write((cmd + "\n").encode('utf-8'))
            self.chat_history.insert(tk.END, f"[CMD] -> {cmd}\n")
        except Exception as e:
            self.chat_history.insert(tk.END, f"[CMD] 发送失败: {e}\n")

    def generate_report(self):
        self.btn_ai.config(state="disabled", text="正在分析...")
        def _task():
            time.sleep(1)
            payload = {
                "hr": g_hr, "spo2": g_spo2, "temp": g_temp,
                "fall_conf": g_fall_conf, "status": g_status_text,
                "valid": g_valid
            }
            prompt = f"请基于以下健康数据生成简要评估报告：\n{json.dumps(payload, ensure_ascii=False)}"
            self.root.after(0, lambda: self.chat_history.insert(tk.END, f"我: 生成健康诊断报告\n"))
            self.ask_ai(prompt)
            self.root.after(0, lambda: self.btn_ai.config(state="normal", text="AI 诊断报告"))
        threading.Thread(target=_task, daemon=True).start()

    def send_message(self, event=None):
        msg = self.chat_entry.get()
        if msg:
            self.chat_history.insert(tk.END, f"我: {msg}\n")
            self.chat_entry.delete(0, tk.END)
            threading.Thread(target=self.ask_ai, args=(msg,), daemon=True).start()

    def ask_ai(self, user_msg):
        try:
            from openai import OpenAI
            c = OpenAI(api_key=DEEPSEEK_KEY, base_url="https://api.deepseek.com")
            context = f"HR:{g_hr}bpm, SpO2:{g_spo2}%, Temp:{g_temp:.1f}C, Status:{g_status_text}"
            response = c.chat.completions.create(
                model="deepseek-chat",
                messages=[
                    {"role": "system", "content": "你是一位专业的心血管内科医生。请基于数据给出简要分析。"},
                    {"role": "user", "content": f"[实时数据: {context}] {user_msg}"},
                ],
                stream=False
            )
            reply = response.choices[0].message.content
            self.root.after(0, lambda: self.chat_history.insert(tk.END, f"AI: {reply}\n\n"))
        except Exception as e:
            self.root.after(0, lambda: self.chat_history.insert(tk.END, f"AI 服务暂时不可用: {e}\n"))

    def animate(self):
        if is_running:
            try:
                self._update_ui()
                self._update_plot()
            except:
                pass
            self.root.after(REFRESH_INTERVAL, self.animate)

    def _update_ui(self):
        # 状态文本
        status_colors = {
            "Normal": "#00b894", "High HR": "#e74c3c", "Low HR": "#e67e22",
            "Low SpO2": "#e74c3c", "FALL!": "#ff0000", "Fever": "#f39c12"
        }
        color = status_colors.get(g_status_text, "#f1c40f")
        self.lbl_status.config(text=g_status_text, fg=color)

        # 核心指标
        if g_valid.get("hr"):
            self.lbl_hr.config(text=f"{g_hr}")
        else:
            self.lbl_hr.config(text="--")

        if g_valid.get("spo2"):
            self.lbl_spo2.config(text=f"{g_spo2}%")
        else:
            self.lbl_spo2.config(text="--")

        if g_valid.get("temp"):
            self.lbl_temp.config(text=f"{g_temp:.1f}C")
        else:
            self.lbl_temp.config(text="--")

        if g_valid.get("imu"):
            accel_mag = (g_accel[0]**2 + g_accel[1]**2 + g_accel[2]**2)**0.5
            self.lbl_accel.config(text=f"{accel_mag:.2f}g")
        else:
            self.lbl_accel.config(text="--")

        # 跌倒
        if g_fall_conf > 0:
            fall_color = "#e74c3c" if g_fall_conf >= 60 else "#f39c12"
            self.lbl_fall.config(text=f"{g_fall_conf}%", fg=fall_color)
        else:
            self.lbl_fall.config(text="0%", fg="#00b894")

        # 数据有效性
        valid_parts = []
        if g_valid.get("hr"): valid_parts.append("HR")
        if g_valid.get("spo2"): valid_parts.append("SpO2")
        if g_valid.get("temp"): valid_parts.append("T")
        if g_valid.get("imu"): valid_parts.append("IMU")
        self.lbl_valid.config(text=f"[{'+'.join(valid_parts)}]" if valid_parts else "[无数据]")

        # AI 按钮
        self.btn_ai.config(state="normal" if g_valid.get("hr") else "disabled")

    def _update_plot(self):
        if len(hr_history) < 2:
            return

        # 上图：HR (左轴) + SpO2 (右轴)
        self.ax1.clear()
        hr_data = [v for v in hr_history if v is not None]
        spo2_data = [v for v in spo2_history if v is not None]

        if hr_data:
            self.ax1.plot(list(hr_data), color="#e74c3c", linewidth=1.5, label="HR (bpm)")
        if spo2_data:
            ax1_twin = self.ax1.twinx()
            ax1_twin.plot(list(spo2_data), color="#3498db", linewidth=1.5, linestyle="--", label="SpO2 (%)")
            ax1_twin.set_ylabel("SpO2 (%)", color="#3498db", fontsize=8)
            ax1_twin.tick_params(axis='y', colors='#3498db')
            ax1_twin.set_ylim(0, 105)
            ax1_twin.spines['right'].set_color('#3498db')

        self.ax1.set_ylabel("HR (bpm)", color="#e74c3c", fontsize=8)
        self.ax1.set_title("Heart Rate & SpO2 Trend", color="white", fontsize=9)
        self.ax1.legend(loc="upper left", fontsize=7, facecolor="#16213e", edgecolor="#a0a0a0",
                        labelcolor="white")
        self.ax1.set_facecolor('#16213e')
        self.ax1.tick_params(colors='#a0a0a0')
        self.ax1.set_xlim(0, MAX_POINTS)

        # 下图：Accel magnitude
        self.ax2.clear()
        if len(accel_mag_history) > 0:
            self.ax2.plot(list(accel_mag_history), color="#2ecc71", linewidth=1.5, label="Accel (g)")
        self.ax2.set_ylabel("Accel (g)", color="#a0a0a0", fontsize=8)
        self.ax2.set_xlabel("Samples", color="#a0a0a0", fontsize=8)
        self.ax2.set_title("Accelerometer Magnitude", color="white", fontsize=9)
        self.ax2.legend(loc="upper left", fontsize=7, facecolor="#16213e", edgecolor="#a0a0a0",
                        labelcolor="white")
        self.ax2.set_facecolor('#16213e')
        self.ax2.tick_params(colors='#a0a0a0')
        self.ax2.set_xlim(0, MAX_POINTS)

        self.fig.tight_layout(pad=1.5)
        self.canvas.draw()

def main():
    root = tk.Tk()
    root.title("星闪智能健康监测系统 v2.0")
    root.geometry("400x150")
    root.configure(bg="#1a1a2e")

    screen_w = root.winfo_screenwidth()
    screen_h = root.winfo_screenheight()
    root.geometry(f"+{(screen_w-400)//2}+{(screen_h-150)//2}")

    lbl = tk.Label(root, text="正在初始化...", bg="#1a1a2e", fg="white",
                   font=("Microsoft YaHei", 12))
    lbl.pack(expand=True)
    progress = ttk.Progressbar(root, length=300, mode='indeterminate')
    progress.pack(pady=20)
    progress.start(10)

    def check_status():
        if libs_loaded:
            progress.stop()
            root.destroy()
            start_app()
        elif load_error:
            progress.stop()
            messagebox.showerror("启动失败", f"模块加载错误:\n{load_error}")
            root.destroy()
            sys.exit(1)
        else:
            try:
                if root.winfo_exists():
                    root.after(100, check_status)
            except:
                pass

    threading.Thread(target=load_heavy_modules, daemon=True).start()
    root.after(100, check_status)
    root.mainloop()

def start_app():
    root = tk.Tk()
    root.title("星闪智能健康监测系统 v2.0 - 选择设备")
    root.geometry("500x300")
    root.configure(bg="#1a1a2e")

    screen_w = root.winfo_screenwidth()
    screen_h = root.winfo_screenheight()
    root.geometry(f"+{(screen_w-500)//2}+{(screen_h-300)//2}")

    ports = list(list_ports.comports())

    tk.Label(root, text="星闪智能健康监测系统 v2.0", bg="#1a1a2e", fg="#00d2d3",
             font=("Microsoft YaHei", 16, "bold")).pack(pady=20)
    tk.Label(root, text="选择串口:", bg="#1a1a2e", fg="white",
             font=("Microsoft YaHei", 12)).pack(pady=10)

    port_values = [f"{p.device} - {p.description}" for p in ports]
    combo = ttk.Combobox(root, values=port_values, font=("Microsoft YaHei", 10), width=40)
    combo.pack(pady=10, padx=20)
    if port_values:
        combo.current(0)
    else:
        combo.set("未检测到设备")

    def on_confirm():
        selection = combo.get()
        selected_port = None
        if selection and "未检测到" not in selection:
            selected_port = selection.split(" - ")[0]

        for widget in root.winfo_children():
            widget.destroy()
        root.geometry("1400x800")
        root.geometry(f"+{(screen_w-1400)//2}+{(screen_h-800)//2}")
        HealthMonitorApp(root, selected_port)

    tk.Button(root, text="启动监测", command=on_confirm, bg="#0984e3", fg="white",
              font=("Microsoft YaHei", 12, "bold"), width=15).pack(pady=30)

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
    import multiprocessing
    multiprocessing.freeze_support()
    main()
