@echo off
chcp 65001 >nul
echo ========================================
echo 运行任意命令并保存输出
echo ========================================
echo.

set OUTPUT_FILE=command_output_%date:~0,4%%date:~5,2%%date:~8,2%_%time:~0,2%%time:~3,2%%time:~6,2%.txt
set OUTPUT_FILE=%OUTPUT_FILE: =0%

echo 输出将保存到: %OUTPUT_FILE%
echo.
echo 请输入要运行的命令（例如: python -c "import matplotlib"）
echo 或者输入 exit 退出
echo.

:loop
set /p COMMAND="命令: "
if /i "%COMMAND%"=="exit" goto end

echo.
echo ======================================== >> %OUTPUT_FILE%
echo 时间: %date% %time% >> %OUTPUT_FILE%
echo 命令: %COMMAND% >> %OUTPUT_FILE%
echo ======================================== >> %OUTPUT_FILE%
echo. >> %OUTPUT_FILE%

%COMMAND% >> %OUTPUT_FILE% 2>&1

echo 完成！输出已追加到: %OUTPUT_FILE%
echo.
goto loop

:end
echo.
echo 所有输出已保存到: %OUTPUT_FILE%
echo 请打开该文件查看完整输出
pause
