# 星闪心率监测程序

本程序用于接收星闪开发板输出的心率数据，实时绘制波形并记录数据。

## 功能
- 实时显示 KY-039 传感器采集的 ADC 波形。
- 实时显示计算出的心率 (BPM)。
- 将采集的数据（时间戳、ADC值、BPM）自动保存到 CSV 文件中。

## 环境要求
- Python 3.x
- 必要的 Python 库：`pyserial`, `matplotlib`

## 安装步骤
1. 确保已安装 Python。
2. 打开终端或命令行，进入本目录。
3. 安装依赖库：
   ```bash
   pip install -r requirements.txt
   ```
   或者手动安装：
   ```bash
   pip install pyserial matplotlib
   ```

## 使用方法
1. 将开发板通过 USB 连接到电脑，确认串口号（如 COM3, COM4 等）。
2. 运行程序：
   ```bash
   python monitor.py
   ```
3. 根据提示输入串口号（例如输入 `COM3` 或直接输入 `3`）。
4. 程序将自动开始接收数据并绘图。
5. 关闭窗口或按 Ctrl+C 停止程序，数据将保存在当前目录下的 `heart_rate_data_YYYYMMDD_HHMMSS.csv` 文件中。

## 注意事项
- 确保开发板已烧录正确的程序 (`qrswork_client`)。
- 确保串口未被其他软件占用。
- 默认波特率为 115200。
