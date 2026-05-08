#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
检查环境并将结果保存到文件
"""

import sys
import os
from datetime import datetime

output_file = "check_result.txt"

with open(output_file, 'w', encoding='utf-8') as f:
    f.write("=" * 60 + "\n")
    f.write("环境检查结果\n")
    f.write("时间: " + datetime.now().strftime("%Y-%m-%d %H:%M:%S") + "\n")
    f.write("=" * 60 + "\n\n")
    
    # Python信息
    f.write("[1] Python信息:\n")
    f.write("-" * 60 + "\n")
    f.write(f"Python版本: {sys.version}\n")
    f.write(f"Python路径: {sys.executable}\n\n")
    
    # 检查模块
    f.write("[2] 模块检查:\n")
    f.write("-" * 60 + "\n")
    
    modules = {
        'serial': 'pyserial',
        'matplotlib': 'matplotlib',
        'numpy': 'numpy',
    }
    
    all_ok = True
    for mod_name, package_name in modules.items():
        try:
            mod = __import__(mod_name)
            version = getattr(mod, '__version__', '未知版本')
            f.write(f"[OK] {package_name:15s} - 已安装 (版本: {version})\n")
            print(f"[OK] {package_name:15s} - 已安装")
        except ImportError as e:
            f.write(f"[X]  {package_name:15s} - 未安装\n")
            f.write(f"     错误: {e}\n")
            print(f"[X]  {package_name:15s} - 未安装")
            all_ok = False
    
    f.write("\n")
    
    # 检查串口
    f.write("[3] 串口检查:\n")
    f.write("-" * 60 + "\n")
    try:
        import serial.tools.list_ports
        ports = list(serial.tools.list_ports.comports())
        f.write(f"[OK] serial.tools.list_ports - 可用\n")
        f.write(f"检测到的串口数量: {len(ports)}\n")
        if ports:
            for i, p in enumerate(ports):
                f.write(f"  [{i}] {p.device} - {p.description}\n")
        else:
            f.write("  [提示] 未检测到串口设备\n")
    except Exception as e:
        f.write(f"[X]  串口检查失败: {e}\n")
    
    f.write("\n")
    f.write("=" * 60 + "\n")
    if all_ok:
        f.write("[OK] 所有必需的模块都已安装！\n")
    else:
        f.write("[X] 缺少模块，请安装缺失的依赖\n")
        f.write("安装命令: pip install -r requirements.txt\n")
    f.write("=" * 60 + "\n")

print(f"\n检查完成！结果已保存到: {output_file}")
print("请打开该文件查看完整输出并复制内容")
