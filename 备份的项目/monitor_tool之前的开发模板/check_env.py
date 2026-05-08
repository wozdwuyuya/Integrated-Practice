#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
环境检查脚本 - 检查所有依赖是否正确安装
将结果保存到 check_env_result.txt 文件中
"""

import sys

output_lines = []
output_lines.append("=" * 60)
output_lines.append("环境检查结果")
output_lines.append("=" * 60)
output_lines.append(f"Python 版本: {sys.version}")
output_lines.append(f"Python 路径: {sys.executable}")
output_lines.append("")

# 检查各个模块
modules_to_check = [
    ('serial', 'pyserial'),
    ('matplotlib', 'matplotlib'),
    ('numpy', 'numpy'),
    ('csv', 'csv (标准库)'),
    ('time', 'time (标准库)'),
    ('re', 're (标准库)'),
    ('threading', 'threading (标准库)'),
    ('collections', 'collections (标准库)'),
]

output_lines.append("模块检查:")
output_lines.append("-" * 60)
all_ok = True

for module_name, display_name in modules_to_check:
    try:
        __import__(module_name)
        output_lines.append(f"[OK] {display_name:30s} - 已安装")
    except ImportError as e:
        output_lines.append(f"[X]  {display_name:30s} - 未安装")
        output_lines.append(f"     错误: {e}")
        all_ok = False

output_lines.append("")

# 检查串口工具
output_lines.append("串口工具检查:")
output_lines.append("-" * 60)
try:
    import serial.tools.list_ports
    ports = list(serial.tools.list_ports.comports())
    output_lines.append(f"[OK] serial.tools.list_ports - 可用")
    output_lines.append(f"检测到的串口数量: {len(ports)}")
    if ports:
        for i, p in enumerate(ports):
            output_lines.append(f"  [{i}] {p.device} - {p.description}")
    else:
        output_lines.append("  [WARN] 未检测到串口设备")
except Exception as e:
    output_lines.append(f"[X]  serial.tools.list_ports - 错误: {e}")
    all_ok = False

output_lines.append("")

# 检查文件
output_lines.append("文件检查:")
output_lines.append("-" * 60)
import os
monitor_file = "monitor.py"
if os.path.exists(monitor_file):
    output_lines.append(f"[OK] {monitor_file} - 存在")
    size = os.path.getsize(monitor_file)
    output_lines.append(f"      文件大小: {size} 字节")
else:
    output_lines.append(f"[X]  {monitor_file} - 不存在")
    all_ok = False

output_lines.append("")

# 总结
output_lines.append("=" * 60)
if all_ok:
    output_lines.append("[OK] 所有检查通过！")
else:
    output_lines.append("[X] 发现问题，请安装缺失的模块:")
    output_lines.append("  pip install -r requirements.txt")
output_lines.append("=" * 60)

# 输出到控制台和文件
result_text = "\n".join(output_lines)
print(result_text)

# 保存到文件
output_file = "check_env_result.txt"
with open(output_file, 'w', encoding='utf-8') as f:
    f.write(result_text)

print(f"\n结果已保存到: {output_file}")
