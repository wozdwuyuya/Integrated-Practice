@echo off
chcp 65001 >nul
echo 正在运行程序，输出将保存到 monitor_output.txt
echo ============================================================
python monitor.py > monitor_output.txt 2>&1
echo.
echo 程序运行完成，输出已保存到 monitor_output.txt
echo 请打开该文件查看详细输出
pause
