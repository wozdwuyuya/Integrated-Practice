@echo off
chcp 65001 >nul
title 运行命令并保存输出（可直接复制）
echo ========================================
echo 运行命令并保存输出到文件
echo 所有输出会保存到文件，可以直接复制
echo ========================================
echo.

set OUTPUT_FILE=终端输出_%date:~0,4%%date:~5,2%%date:~8,2%_%time:~0,2%%time:~3,2%%time:~6,2%.txt
set OUTPUT_FILE=%OUTPUT_FILE: =0%

echo 输出文件: %OUTPUT_FILE%
echo.
echo 请选择要运行的命令:
echo [1] 安装 matplotlib 和 numpy
echo [2] 检查环境
echo [3] 运行 monitor.py 程序
echo [4] 自定义命令
echo [0] 退出
echo.

set /p choice="请选择 (0-4): "

if "%choice%"=="1" (
    echo.
    echo ======================================== >> %OUTPUT_FILE%
    echo 安装 matplotlib 和 numpy >> %OUTPUT_FILE%
    echo 时间: %date% %time% >> %OUTPUT_FILE%
    echo ======================================== >> %OUTPUT_FILE%
    echo. >> %OUTPUT_FILE%
    
    echo 正在安装 matplotlib 和 numpy...
    echo 正在安装 matplotlib 和 numpy... >> %OUTPUT_FILE%
    
    python -m pip install matplotlib numpy -i https://mirrors.aliyun.com/pypi/simple/ --trusted-host mirrors.aliyun.com >> %OUTPUT_FILE% 2>&1
    if errorlevel 1 (
        echo 阿里云镜像失败，尝试清华镜像... >> %OUTPUT_FILE%
        python -m pip install matplotlib numpy -i https://pypi.tuna.tsinghua.edu.cn/simple --trusted-host pypi.tuna.tsinghua.edu.cn >> %OUTPUT_FILE% 2>&1
    )
    echo. >> %OUTPUT_FILE%
    echo 验证安装... >> %OUTPUT_FILE%
    python -c "import matplotlib; import numpy; print('安装成功')" >> %OUTPUT_FILE% 2>&1
    
) else if "%choice%"=="2" (
    echo.
    echo ======================================== >> %OUTPUT_FILE%
    echo 检查环境 >> %OUTPUT_FILE%
    echo 时间: %date% %time% >> %OUTPUT_FILE%
    echo ======================================== >> %OUTPUT_FILE%
    echo. >> %OUTPUT_FILE%
    
    python check_and_save.py >> %OUTPUT_FILE% 2>&1
    type check_result.txt >> %OUTPUT_FILE%
    
) else if "%choice%"=="3" (
    echo.
    echo ======================================== >> %OUTPUT_FILE%
    echo 运行 monitor.py >> %OUTPUT_FILE%
    echo 时间: %date% %time% >> %OUTPUT_FILE%
    echo ======================================== >> %OUTPUT_FILE%
    echo. >> %OUTPUT_FILE%
    
    python monitor.py >> %OUTPUT_FILE% 2>&1
    
) else if "%choice%"=="4" (
    echo.
    set /p custom_cmd="请输入命令: "
    echo.
    echo ======================================== >> %OUTPUT_FILE%
    echo 自定义命令: %custom_cmd% >> %OUTPUT_FILE%
    echo 时间: %date% %time% >> %OUTPUT_FILE%
    echo ======================================== >> %OUTPUT_FILE%
    echo. >> %OUTPUT_FILE%
    
    %custom_cmd% >> %OUTPUT_FILE% 2>&1
    
) else if "%choice%"=="0" (
    exit /b 0
) else (
    echo 无效选择
    pause
    exit /b 1
)

echo.
echo ========================================
echo 完成！
echo ========================================
echo.
echo 所有输出已保存到: %OUTPUT_FILE%
echo.
echo 正在打开文件，您可以查看并复制所有内容...
echo.

timeout /t 1 >nul
notepad %OUTPUT_FILE%

pause
