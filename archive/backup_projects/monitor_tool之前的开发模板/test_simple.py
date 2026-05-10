#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
简单的测试脚本 - 检查环境并保存结果到文件
"""

import sys
import os

# 设置输出编码为UTF-8
if sys.platform == 'win32':
    import io
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8')

output_file = "test_result.txt"

with open(output_file, 'w', encoding='utf-8') as f:
    f.write("=" * 60 + "\n")
    f.write("环境检查结果\n")
    f.write("=" * 60 + "\n")
    f.write(f"Python 版本: {sys.version}\n")
    f.write(f"Python 路径: {sys.executable}\n\n")
    
    # 检查模块
    f.write("模块检查:\n")
    f.write("-" * 60 + "\n")
    
    modules = ['serial', 'matplotlib', 'numpy']
    all_ok = True
    
    for mod in modules:
        try:
            __import__(mod)
            f.write(f"[OK] {mod:15s} - 已安装\n")
            print(f"[OK] {mod:15s} - 已安装")
        except ImportError as e:
            f.write(f"[X]  {mod:15s} - 未安装\n")
            f.write(f"     错误: {e}\n")
            print(f"[X]  {mod:15s} - 未安装: {e}")
            all_ok = False
    
    f.write("\n")
    
    # 检查串口
    f.write("串口检查:\n")
    f.write("-" * 60 + "\n")
    try:
        import serial.tools.list_ports
        ports = list(serial.tools.list_ports.comports())
        f.write(f"[OK] serial.tools.list_ports - 可用\n")
        f.write(f"检测到的串口数量: {len(ports)}\n")
        print(f"[OK] 串口工具可用，检测到 {len(ports)} 个串口")
        if ports:
            for i, p in enumerate(ports):
                f.write(f"  [{i}] {p.device} - {p.description}\n")
                print(f"  [{i}] {p.device}")
    except Exception as e:
        f.write(f"[X]  串口检查失败: {e}\n")
        print(f"[X]  串口检查失败: {e}")
        all_ok = False
    
    f.write("\n")
    f.write("=" * 60 + "\n")
    if all_ok:
        f.write("[OK] 所有检查通过！\n")
        print("\n[OK] 所有检查通过！")
    else:
        f.write("[X] 发现问题，请运行: pip install -r requirements.txt\n")
        print("\n[X] 发现问题，请运行: pip install -r requirements.txt")
    f.write("=" * 60 + "\n")

print(f"\n结果已保存到: {output_file}")
