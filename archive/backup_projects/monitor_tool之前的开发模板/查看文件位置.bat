@echo off
chcp 65001 >nul
echo ========================================
echo 查看文件位置
echo ========================================
echo.

echo 当前目录:
cd
echo.

echo 完整路径:
cd
echo.

echo ========================================
echo 查找 check_result.txt 文件
echo ========================================

if exist check_result.txt (
    echo [找到] check_result.txt
    echo.
    echo 文件完整路径:
    cd
    echo %CD%\check_result.txt
    echo.
    echo 正在打开文件...
    notepad check_result.txt
) else (
    echo [未找到] check_result.txt
    echo.
    echo 正在生成文件...
    python check_and_save.py
    echo.
    if exist check_result.txt (
        echo [生成成功] check_result.txt
        echo.
        echo 文件完整路径:
        cd
        echo %CD%\check_result.txt
        echo.
        echo 正在打开文件...
        notepad check_result.txt
    ) else (
        echo [错误] 无法生成文件
    )
)

echo.
pause
