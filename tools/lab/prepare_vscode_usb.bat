@echo off
chcp 936 >nul
setlocal enabledelayedexpansion
title 制作便携版 VSCode（U 盘）— STM32Hexapod
cd /d "%~dp0..\.."

echo ============================================================
echo  制作「免安装 VSCode」到 U 盘
echo ------------------------------------------------------------
echo  原理：VSCode 官方支持便携模式 —— Code.exe 同目录下有一个
echo        data\ 文件夹时，配置/扩展/缓存都写进这个 data，
echo        不碰注册表和 %APPDATA%，换电脑插上就能用。
echo  本脚本做 5 件事：
echo    1) 把本机 VSCode 整个目录复制到 U 盘 \VSCode
echo    2) 建好 data\ 目录（便携模式开关）
echo    3) 复制本机扩展 + 生成「实验室友好」的用户设置
echo    4) 清掉扩展注册表里的本机绝对路径（换电脑会失效的元凶）
echo    5) 放入 启动-VSCode-便携.cmd 和 诊断-扩展加载.cmd
echo ============================================================
echo.

REM ---------- 0. 参数：盘符 / --no-pause / --reuse ----------
set "DRIVE=%~1"
if "%DRIVE:~0,2%"=="--" set "DRIVE="
set "NOPAUSE="
set "REUSE="
for %%a in (%*) do (
    if /i "%%~a"=="--no-pause" set "NOPAUSE=1"
    if /i "%%~a"=="--reuse" set "REUSE=1"
)
if "%DRIVE%"=="" (
    echo 当前可移动磁盘：
    powershell -NoProfile -Command "Get-Volume | Where-Object {$_.DriveType -eq 'Removable'} | Select-Object DriveLetter,FileSystemLabel,@{n='剩余GB';e={[math]::Round($_.SizeRemaining/1GB,1)}} | Format-Table -AutoSize"
    echo.
    set /p "DRIVE=请输入 U 盘盘符（例如 E，直接回车退出）："
)
set "DRIVE=%DRIVE:~0,1%"
if "%DRIVE%"=="" (
    echo [取消] 没有输入盘符。
    if not defined NOPAUSE pause
    exit /b 0
)
if not exist "%DRIVE%:\" (
    echo [错误] 盘符 %DRIVE%: 不存在，插好 U 盘再试。
    if not defined NOPAUSE pause
    exit /b 1
)
set "DST=%DRIVE%:\VSCode"
set "PF86=%ProgramFiles(x86)%"

REM ---------- 1. 找本机 VSCode ----------
set "SRC="
if exist "%LOCALAPPDATA%\Programs\Microsoft VS Code\Code.exe" set "SRC=%LOCALAPPDATA%\Programs\Microsoft VS Code"
if not defined SRC if exist "%ProgramFiles%\Microsoft VS Code\Code.exe" set "SRC=%ProgramFiles%\Microsoft VS Code"
if not defined SRC if exist "%PF86%\Microsoft VS Code\Code.exe" set "SRC=%PF86%\Microsoft VS Code"

if not defined SRC if not exist "%DST%\Code.exe" (
    echo [错误] 在这台电脑上找不到已安装的 VSCode。
    echo.
    echo   办法 A（推荐）：https://code.visualstudio.com/download 下 "Windows x64 ^(zip^)"，
    echo      解压到 U 盘 \VSCode，再手动新建空文件夹 \VSCode\data 即可。
    echo   办法 B：先把本脚本拷到装过 VSCode 的电脑上跑。
    if not defined NOPAUSE pause
    exit /b 1
)

if defined SRC echo 源目录 ：%SRC%
echo 目标   ：%DST%
echo.

if not exist "%DST%" md "%DST%"

REM ---------- 2. 复制 VSCode 本体（--reuse 且已有则跳过） ----------
if defined REUSE if exist "%DST%\Code.exe" (
    echo [1/5] 复制 VSCode 本体 ... 已存在，跳过
    goto :SKIP_BODY
)
echo [1/5] 复制 VSCode 本体（约 1 GB，U 盘慢的话几分钟，别拔线）...
robocopy "%SRC%" "%DST%" /E /XD data /NFL /NDL /NJH /NJS /R:1 /W:1 >nul
if not exist "%DST%\Code.exe" (
    echo [错误] 复制失败：%DST%\Code.exe 不存在。
    if not defined NOPAUSE pause
    exit /b 1
)
echo       OK
:SKIP_BODY

REM ---------- 3. data 目录 + 扩展 ----------
echo [2/5] 创建 data\ 目录（便携模式开关）...
if not exist "%DST%\data\extensions" md "%DST%\data\extensions"
if not exist "%DST%\data\user-data\User" md "%DST%\data\user-data\User"
echo       OK

echo [3/5] 复制本机扩展 ...
set "EXTN=0"
for /d %%d in ("%DST%\data\extensions\*") do set /a EXTN+=1
if defined REUSE if !EXTN! GTR 5 (
    echo       已有 !EXTN! 个扩展文件夹，跳过复制
    goto :SKIP_EXT
)
if exist "%USERPROFILE%\.vscode\extensions" (
    robocopy "%USERPROFILE%\.vscode\extensions" "%DST%\data\extensions" /E /NFL /NDL /NJH /NJS /R:1 /W:1 >nul
    echo       已复制：%USERPROFILE%\.vscode\extensions
) else (
    echo       [跳过] 本机没有 %USERPROFILE%\.vscode\extensions
)
:SKIP_EXT

REM ---------- 4. 清掉带绝对路径的扩展注册表 ----------
echo [4/5] 清理扩展注册表（里面是本机绝对路径，换电脑/换盘符就失效）...
if exist "%DST%\data\extensions\extensions.json" del /f /q "%DST%\data\extensions\extensions.json"
if exist "%DST%\data\extensions\.obsolete" del /f /q "%DST%\data\extensions\.obsolete"
for /d %%d in ("%DST%\data\extensions\.*") do rd /s /q "%%d"
echo       OK（启动时会重新扫描扩展）

REM ---------- 5. 用户设置（剔除指死本机的项） ----------
echo [5/5] 生成便携版用户设置 ...
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0portable\make_portable_settings.ps1" -Src "%APPDATA%\Code\User\settings.json" -DstUser "%DST%\data\user-data\User"
if %ERRORLEVEL% NEQ 0 echo       [警告] 设置生成失败，可先手动复制 settings.json 进去

copy /y "%~dp0portable\start-portable.cmd" "%DST%\启动-VSCode-便携.cmd" >nul
copy /y "%~dp0portable\diagnose-extensions.cmd" "%DST%\诊断-扩展加载.cmd" >nul

echo.
echo ============================================================
echo  [完成] %DST%
echo ------------------------------------------------------------
echo  在实验室电脑上（重要：一定要用启动器，别直接双击 Code.exe）：
echo    1) 双击 %DRIVE%:\VSCode\启动-VSCode-便携.cmd
echo    2) 打开工程：文件 ^-^> 打开文件夹 ^-^> 选 STM32Hexapod
echo    3) Ctrl+Shift+B 编译；Ctrl+Shift+P ^> Tasks: Run Task 烧录
echo.
echo  为什么必须用启动器：它强制把「配置目录」和「扩展目录」都指向
echo  U 盘的 data\，并删掉带绝对路径的扩展注册表。直接双击 Code.exe
echo  时，VSCode 在家里能正常跑（因为本机就有 %APPDATA%\Code 的配置），
echo  换到实验室的空电脑上就可能退回普通模式，扩展和设置全部为空。
echo.
echo  ※ 如果还是不对：双击 %DRIVE%:\VSCode\诊断-扩展加载.cmd
echo    看它生成的 诊断报告.txt（会列出识别到的扩展和实际用的目录）
echo  ※ 便携版不自动更新；升级就在家里重跑一次本脚本
echo ============================================================
if not defined NOPAUSE pause
exit /b 0
