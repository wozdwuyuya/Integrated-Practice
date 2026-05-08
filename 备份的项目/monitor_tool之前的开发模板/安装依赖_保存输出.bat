@echo off
chcp 65001 >nul
title 安装依赖（所有输出保存到文件）
echo ========================================
echo 安装 matplotlib 和 numpy
echo 所有输出会保存到文件，方便复制
echo ========================================
echo.

set OUTPUT_FILE=安装输出_%date:~0,4%%date:~5,2%%date:~8,2%_%time:~0,2%%time:~3,2%%time:~6,2%.txt
set OUTPUT_FILE=%OUTPUT_FILE: =0%

echo 输出将保存到: %OUTPUT_FILE%
echo.
echo 正在安装...
echo.

echo ======================================== > %OUTPUT_FILE%
echo 安装 matplotlib 和 numpy >> %OUTPUT_FILE%
echo 时间: %date% %time% >> %OUTPUT_FILE%
echo ======================================== >> %OUTPUT_FILE%
echo. >> %OUTPUT_FILE%

echo [步骤1] 尝试使用阿里云镜像... >> %OUTPUT_FILE%
python -m pip install matplotlib numpy -i https://mirrors.aliyun.com/pypi/simple/ --trusted-host mirrors.aliyun.com >> %OUTPUT_FILE% 2>&1

if errorlevel 1 (
    echo. >> %OUTPUT_FILE%
    echo [步骤2] 阿里云镜像失败，尝试清华镜像... >> %OUTPUT_FILE%
    python -m pip install matplotlib numpy -i https://pypi.tuna.tsinghua.edu.cn/simple --trusted-host pypi.tuna.tsinghua.edu.cn >> %OUTPUT_FILE% 2>&1
    
    if errorlevel 1 (
        echo. >> %OUTPUT_FILE%
        echo [步骤3] 清华镜像失败，尝试使用信任主机参数... >> %OUTPUT_FILE%
        python -m pip install matplotlib numpy --trusted-host pypi.org --trusted-host files.pythonhosted.org >> %OUTPUT_FILE% 2>&1
    )
)

echo. >> %OUTPUT_FILE%
echo ======================================== >> %OUTPUT_FILE%
echo 验证安装结果 >> %OUTPUT_FILE%
echo ======================================== >> %OUTPUT_FILE%
python -c "import matplotlib; import numpy; print('安装成功！')" >> %OUTPUT_FILE% 2>&1

echo.
echo ========================================
echo 安装完成！
echo ========================================
echo.
echo 所有输出已保存到文件: %OUTPUT_FILE%
echo.
echo 文件完整路径:
cd
echo %CD%\%OUTPUT_FILE%
echo.
echo 正在打开文件，您可以查看并复制所有内容...
echo.

timeout /t 2 >nul
notepad %OUTPUT_FILE%

pause
