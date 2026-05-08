#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
配置检查脚本 - 检查所有依赖和配置
"""

import sys

def check_config():
    print("=" * 60)
    print("配置检查")
    print("=" * 60)
    
    # Python版本
    print(f"\n[1] Python版本: {sys.version}")
    print(f"    Python路径: {sys.executable}")
    
    # 检查必需的模块
    print("\n[2] 必需的Python模块:")
    print("-" * 60)
    
    required_modules = {
        'serial': 'pyserial',
        'matplotlib': 'matplotlib',
        'numpy': 'numpy',
    }
    
    missing_modules = []
    installed_modules = []
    
    for module_name, package_name in required_modules.items():
        try:
            mod = __import__(module_name)
            version = getattr(mod, '__version__', '未知版本')
            print(f"    [OK] {package_name:15s} - 已安装 (版本: {version})")
            installed_modules.append(package_name)
        except ImportError:
            print(f"    [X]  {package_name:15s} - 未安装")
            missing_modules.append(package_name)
    
    # 检查标准库模块（通常都可用）
    print("\n[3] 标准库模块:")
    print("-" * 60)
    std_modules = ['time', 're', 'csv', 'threading', 'collections', 'sys', 'os']
    for mod_name in std_modules:
        try:
            __import__(mod_name)
            print(f"    [OK] {mod_name:15s} - 可用")
        except ImportError:
            print(f"    [X]  {mod_name:15s} - 不可用")
    
    # 检查串口工具
    print("\n[4] 串口工具:")
    print("-" * 60)
    try:
        import serial.tools.list_ports
        ports = list(serial.tools.list_ports.comports())
        print(f"    [OK] serial.tools.list_ports - 可用")
        print(f"    检测到的串口数量: {len(ports)}")
        if ports:
            for i, p in enumerate(ports):
                print(f"      [{i}] {p.device} - {p.description}")
        else:
            print("      [提示] 未检测到串口设备（可能设备未连接）")
    except Exception as e:
        print(f"    [X]  串口工具检查失败: {e}")
    
    # 检查matplotlib后端
    print("\n[5] Matplotlib后端:")
    print("-" * 60)
    try:
        import matplotlib
        backend = matplotlib.get_backend()
        print(f"    [OK] 后端: {backend}")
    except:
        print(f"    [X]  无法检查后端（matplotlib未安装）")
    
    # 总结
    print("\n" + "=" * 60)
    print("检查结果:")
    print("=" * 60)
    
    if missing_modules:
        print(f"\n[X] 缺少以下模块，请安装:")
        for mod in missing_modules:
            print(f"    pip install {mod}")
        print("\n或者一次性安装所有依赖:")
        print("    pip install -r requirements.txt")
        return False
    else:
        print("\n[OK] 所有必需的模块都已安装！")
        print("\n配置检查通过，可以运行程序了。")
        print("\n运行程序:")
        print("    python monitor.py")
        return True

if __name__ == "__main__":
    try:
        success = check_config()
        sys.exit(0 if success else 1)
    except Exception as e:
        print(f"\n检查过程中出错: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
