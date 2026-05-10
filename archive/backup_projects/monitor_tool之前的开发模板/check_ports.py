#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
串口检测诊断脚本
用于检查系统可用串口
"""

import sys

print("=== 串口检测诊断工具 ===\n")

# 方法1: 使用 pyserial
try:
    import serial.tools.list_ports
    print("✓ pyserial 已安装\n")
    
    ports = list(serial.tools.list_ports.comports())
    print(f"检测到的串口数量: {len(ports)}\n")
    
    if ports:
        print("可用串口列表:")
        for i, p in enumerate(ports):
            print(f"  [{i}] {p.device}")
            print(f"      描述: {p.description}")
            print(f"      硬件ID: {p.hwid}")
            print()
    else:
        print("⚠ 未检测到串口设备\n")
        print("可能的原因:")
        print("  1. 串口设备未连接")
        print("  2. 串口驱动未安装")
        print("  3. 串口被其他程序占用")
        print("  4. USB转串口设备需要重新插拔")
        print()
        
except ImportError:
    print("✗ pyserial 未安装")
    print("请运行: pip install pyserial\n")
    sys.exit(1)

# 方法2: 尝试列出所有可能的COM口
print("尝试检测所有COM端口:")
for i in range(1, 21):  # COM1-COM20
    port_name = f"COM{i}"
    try:
        import serial
        ser = serial.Serial(port_name, timeout=0.1)
        ser.close()
        print(f"  ✓ {port_name} 可用")
    except (OSError, serial.SerialException):
        # 串口不存在或被占用是正常的，不打印
        pass
    except Exception as e:
        # 其他错误可能表示端口存在但无法打开
        print(f"  ? {port_name} 可能存在但无法打开: {type(e).__name__}")

print("\n诊断完成")