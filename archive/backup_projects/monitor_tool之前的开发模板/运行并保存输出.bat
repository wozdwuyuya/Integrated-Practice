@echo off
chcp 65001 >nul
echo ========================================
echo 安装依赖并保存输出
echo ========================================
echo.

set OUTPUT_FILE=安装输出结果.txt

echo 输出将保存到: %OUTPUT_FILE%
echo 安装完成后，您可以打开该文件查看并复制所有输出
echo.

python -m pip install matplotlib numpy -i https://mirrors.aliyun.com/pypi/simple/ --trusted-host mirrors.aliyun.com > %OUTPUT_FILE% 2>&1

if errorlevel 1 (
    echo 阿里云镜像失败，尝试清华镜像... >> %OUTPUT_FILE%
    python -m pip install matplotlib numpy -i https://pypi.tuna.tsinghua.edu.cn/simple --trusted-host pypi.tuna.tsinghua.edu.cn >> %OUTPUT_FILE% 2>&1
)

echo.
echo ========================================
echo 完成！
echo ========================================
echo.
echo 所有输出已保存到: %OUTPUT_FILE%
echo 请打开该文件查看完整输出并复制内容
echo.
echo 文件位置: %CD%\%OUTPUT_FILE%
echo.
pause
