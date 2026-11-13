@echo off
chcp 936 >nul
setlocal
set "BASE=%~dp0"
set "BASE=%BASE:~0,-1%"
set "RPT=%BASE%\诊断报告.txt"
set "EX=%BASE%\data\extensions"
set "UD=%BASE%\data\user-data"
echo ===== VS Code 便携版自检报告 ===== > "%RPT%"
echo 时间: %DATE% %TIME% >> "%RPT%"
ver >> "%RPT%"
echo. >> "%RPT%"
echo [1] 程序目录: %BASE% >> "%RPT%"
if exist "%BASE%\Code.exe" (echo     Code.exe : 有 >> "%RPT%") else (echo     Code.exe : 缺失 >> "%RPT%")
echo [2] 便携 data 目录: >> "%RPT%"
if exist "%BASE%\data" (echo     data : 有 [便携模式必需] >> "%RPT%") else (echo     data : 缺失，会退回普通模式 >> "%RPT%")
echo [3] 版本: >> "%RPT%"
call "%BASE%\bin\code.cmd" --version >> "%RPT%" 2>&1
echo [4] 扩展目录里的文件夹数: >> "%RPT%"
dir /b /ad "%EX%" 2>nul | find /c /v "" >> "%RPT%"
echo [5] VS Code 实际能识别到的扩展: >> "%RPT%"
call "%BASE%\bin\code.cmd" --list-extensions --extensions-dir "%EX%" --user-data-dir "%UD%" >> "%RPT%" 2>&1
echo [6] 关键日志目录: >> "%RPT%"
if exist "%UD%\logs" (echo     U盘 logs : 有 >> "%RPT%") else (echo     U盘 logs : 没有 [说明没用便携目录跑过] >> "%RPT%")
if exist "%APPDATA%\Code\logs" (echo     普通模式 logs : 有 [可能用了普通模式] >> "%RPT%") else (echo     普通模式 logs : 没有 >> "%RPT%")
echo [7] 用户设置: >> "%RPT%"
if exist "%UD%\User\settings.json" (echo     有 %UD%\User\settings.json >> "%RPT%") else (echo     没有用户设置文件 >> "%RPT%")
echo. >> "%RPT%"
echo ===== 结束 ===== >> "%RPT%"
echo 报告已生成: %RPT%
if /i not "%~1"=="--no-open" notepad "%RPT%"
endlocal
