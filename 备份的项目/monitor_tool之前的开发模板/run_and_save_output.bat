@echo off
chcp 65001 >nul
echo ========================================
echo 运行命令并保存输出到文件
echo ========================================
echo.

set OUTPUT_FILE=command_output.txt

echo 输出将保存到: %OUTPUT_FILE%
echo.

echo 正在运行安装命令...
echo ======================================== >> %OUTPUT_FILE%
echo 安装 matplotlib 和 numpy >> %OUTPUT_FILE%
echo 时间: %date% %time% >> %OUTPUT_FILE%
echo ======================================== >> %OUTPUT_FILE%
echo. >> %OUTPUT_FILE%

python -m pip install matplotlib numpy -i https://mirrors.aliyun.com/pypi/simple/ --trusted-host mirrors.aliyun.com >> %OUTPUT_FILE% 2>&1

if errorlevel 1 (
    echo 阿里云镜像失败，尝试清华镜像...
    echo. >> %OUTPUT_FILE%
    echo ======================================== >> %OUTPUT_FILE%
    echo 尝试使用清华镜像 >> %OUTPUT_FILE%
    echo ======================================== >> %OUTPUT_FILE%
    python -m pip install matplotlib numpy -i https://pypi.tuna.tsinghua.edu.cn/simple --trusted-host pypi.tuna.tsinghua.edu.cn >> %OUTPUT_FILE% 2>&1
    
    if errorlevel 1 (
        echo 清华镜像失败，尝试使用信任主机参数...
        echo. >> %OUTPUT_FILE%
        echo ======================================== >> %OUTPUT_FILE%
        echo 尝试使用信任主机参数 >> %OUTPUT_FILE%
        echo ======================================== >> %OUTPUT_FILE%
        python -m pip install matplotlib numpy --trusted-host pypi.org --trusted-host files.pythonhosted.org >> %OUTPUT_FILE% 2>&1
    )
)

echo.
echo ======================================== >> %OUTPUT_FILE%
echo 验证安装结果 >> %OUTPUT_FILE%
echo ======================================== >> %OUTPUT_FILE%
python -c "import matplotlib; import numpy; print('安装成功！')" >> %OUTPUT_FILE% 2>&1

echo.
echo ========================================
echo 完成！
echo ========================================
echo.
echo 所有输出已保存到: %OUTPUT_FILE%
echo 请打开该文件查看完整输出并复制内容
echo.
pause
