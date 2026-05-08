#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
运行monitor.py并将输出保存到日志文件
"""

import sys
import os
from datetime import datetime

# 创建日志文件名
log_file = f"monitor_log_{datetime.now().strftime('%Y%m%d_%H%M%S')}.txt"

print(f"开始运行程序，日志将保存到: {log_file}")
print("=" * 60)

# 重定向输出到文件和控制台
class TeeOutput:
    def __init__(self, *files):
        self.files = files
    
    def write(self, obj):
        for f in self.files:
            f.write(obj)
            f.flush()
    
    def flush(self):
        for f in self.files:
            f.flush()

# 打开日志文件
log_f = open(log_file, 'w', encoding='utf-8')

# 保存原始stdout和stderr
original_stdout = sys.stdout
original_stderr = sys.stderr

# 重定向输出
sys.stdout = TeeOutput(sys.stdout, log_f)
sys.stderr = TeeOutput(sys.stderr, log_f)

try:
    # 导入并运行主程序
    import monitor
    monitor.main()
except KeyboardInterrupt:
    print("\n程序被用户中断")
except Exception as e:
    import traceback
    print(f"\n发生错误: {e}")
    print("\n完整错误信息:")
    traceback.print_exc()
finally:
    # 恢复原始输出
    sys.stdout = original_stdout
    sys.stderr = original_stderr
    log_f.close()
    print(f"\n日志已保存到: {log_file}")
