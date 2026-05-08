@echo off
chcp 65001 >nul
echo ========================================
echo 安装 matplotlib 和 numpy
echo ========================================
echo.

echo 正在尝试使用阿里云镜像安装...
python -m pip install matplotlib numpy -i https://mirrors.aliyun.com/pypi/simple/ --trusted-host mirrors.aliyun.com

if errorlevel 1 (
    echo.
    echo [失败] 使用阿里云镜像失败，尝试使用清华镜像...
    python -m pip install matplotlib numpy -i https://pypi.tuna.tsinghua.edu.cn/simple --trusted-host pypi.tuna.tsinghua.edu.cn
    
    if errorlevel 1 (
        echo.
        echo [失败] 使用清华镜像也失败，尝试使用 --trusted-host 参数...
        python -m pip install matplotlib numpy --trusted-host pypi.org --trusted-host files.pythonhosted.org
        
        if errorlevel 1 (
            echo.
            echo [错误] 所有方法都失败了，请检查网络连接或参考 INSTALL_GUIDE.md 文件
            pause
            exit /b 1
        )
    )
)

echo.
echo ========================================
echo 验证安装...
echo ========================================
python -c "import matplotlib; import numpy; print('[OK] matplotlib 和 numpy 安装成功！')" 2>nul

if errorlevel 1 (
    echo [X] 验证失败，可能安装不完整
) else (
    echo [OK] 所有依赖已安装完成！
    echo.
    echo 现在可以运行程序了：
    echo     python monitor.py
)

echo.
pause
