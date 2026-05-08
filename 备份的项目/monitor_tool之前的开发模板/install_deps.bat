@echo off
chcp 65001 >nul
echo ========================================
echo 安装程序依赖
echo ========================================
echo.

echo [1/3] 检查Python版本...
python --version
if errorlevel 1 (
    echo [错误] 未找到Python，请先安装Python
    pause
    exit /b 1
)
echo.

echo [2/3] 升级pip...
python -m pip install --upgrade pip
echo.

echo [3/3] 安装依赖包...
python -m pip install -r requirements.txt
echo.

echo ========================================
echo 安装完成！
echo ========================================
echo.

echo 验证安装...
python -c "import serial, matplotlib, numpy; print('[OK] 所有依赖已安装')" 2>nul
if errorlevel 1 (
    echo [X] 验证失败，请检查错误信息
) else (
    echo [OK] 验证通过，可以运行程序了！
    echo.
    echo 运行程序: python monitor.py
)
echo.
pause
