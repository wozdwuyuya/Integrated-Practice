# 配置检查清单

## 当前程序需要的依赖

根据 `monitor.py` 代码分析，程序需要以下依赖：

### 1. Python 标准库（无需安装）
- ✅ `time` - 时间处理
- ✅ `re` - 正则表达式
- ✅ `csv` - CSV文件处理
- ✅ `threading` - 多线程
- ✅ `collections` - 数据结构（deque）
- ✅ `sys` - 系统相关
- ✅ `os` - 操作系统接口

### 2. 第三方库（需要安装）

根据 `requirements.txt` 和代码导入，需要以下库：

| 模块名 | 包名 | 用途 | 是否必需 |
|--------|------|------|---------|
| `serial` | `pyserial` | 串口通信 | ✅ 必需 |
| `matplotlib` | `matplotlib` | 数据可视化/绘图 | ✅ 必需 |
| `numpy` | `numpy` | 数值计算（滤波处理） | ✅ 必需 |

### 3. 配置参数

程序中的配置参数：
- `BAUD_RATE = 115200` - 串口波特率
- `MAX_POINTS = 100` - 显示数据点数量
- `REFRESH_INTERVAL = 50` - 刷新间隔（毫秒）
- `MEDIAN_WINDOW = 3` - 中值滤波窗口
- `AVG_WINDOW = 5` - 均值滤波窗口
- `MIN_VALID_BPM = 40` - 最小有效BPM
- `MAX_VALID_BPM = 220` - 最大有效BPM

## 检查步骤

### 步骤1: 检查Python版本
```bash
python --version
```
要求：Python 3.6 或更高版本（推荐 3.8+）

### 步骤2: 检查已安装的包
```bash
pip list | findstr /i "pyserial matplotlib numpy"
```

### 步骤3: 安装缺失的包
```bash
# 方法1: 使用requirements.txt（推荐）
pip install -r requirements.txt

# 方法2: 手动安装
pip install pyserial matplotlib numpy
```

## 当前状态检查

根据之前的检查：
- ✅ Python 3.8.10 - 已安装
- ✅ pyserial 3.5 - 已安装
- ❓ matplotlib - 需要检查
- ❓ numpy - 需要检查

## 建议的安装命令

如果缺少依赖，运行：
```bash
cd monitor_tool
pip install -r requirements.txt
```

## 额外说明

### Matplotlib后端
- Windows系统通常使用 `TkAgg` 后端
- 如果遇到显示问题，可能需要安装：`pip install tkinter`（但tkinter通常随Python一起安装）

### 串口驱动
- 确保USB转串口驱动已正确安装
- 在Windows设备管理器中检查COM端口是否正常

## 验证安装

运行以下命令验证所有依赖是否已安装：
```bash
python -c "import serial, matplotlib, numpy; print('所有依赖已安装')"
```

如果出现错误，说明缺少相应的模块。
