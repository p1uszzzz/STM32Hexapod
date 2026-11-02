@echo off
chcp 936 >nul
setlocal enabledelayedexpansion
title 制作便携版 VSCode（U 盘）— STM32Hexapod
cd /d "%~dp0..\.."

echo ============================================================
echo  制作「免安装 VSCode」到 U 盘
echo ------------------------------------------------------------
echo  原理：VSCode 官方支持便携模式 —— Code.exe 同目录下有一个
echo        data\ 文件夹时，所有配置/扩展/缓存都写进这个 data，
echo        完全不碰注册表和 %APPDATA%，换电脑插上就能用。
echo  本脚本做三件事：
echo    1^) 把本机已装的 VSCode 整个目录复制到 U 盘 \VSCode
echo    2^) 建好 data\ 目录（激活便携模式）
echo    3^) 把本机扩展 + 用户设置也复制进去
echo ============================================================
echo.

REM ---------- 1. 选 U 盘 ----------
set "DRIVE=%~1"
if "%DRIVE%"=="" (
    echo 当前可移动磁盘：
    powershell -NoProfile -Command "Get-Volume | Where-Object {$_.DriveType -eq 'Removable'} | Select-Object DriveLetter,FileSystemLabel,@{n='容量GB';e={[math]::Round($_.Size/1GB,1)}} | Format-Table -AutoSize"
    echo.
    set /p "DRIVE=请输入 U 盘盘符（例如 E，直接回车退出）："
)
REM 不管用户输入 "E" / "E:" / "E:\"，只取第一个字符当盘符
set "DRIVE=%DRIVE:~0,1%"
if "%DRIVE%"=="" (
    echo [取消] 没有输入盘符。
    pause
    exit /b 0
)
if not exist "%DRIVE%:\" (
    echo [错误] 盘符 %DRIVE%: 不存在，插好 U 盘再试。
    pause
    exit /b 1
)
set "DST=%DRIVE%:\VSCode"

REM ---------- 2. 找本机 VSCode ----------
set "PF86=%ProgramFiles(x86)%"
set "SRC="
if exist "%LOCALAPPDATA%\Programs\Microsoft VS Code\Code.exe" set "SRC=%LOCALAPPDATA%\Programs\Microsoft VS Code"
if not defined SRC if exist "%ProgramFiles%\Microsoft VS Code\Code.exe" set "SRC=%ProgramFiles%\Microsoft VS Code"
if not defined SRC if exist "%PF86%\Microsoft VS Code\Code.exe" set "SRC=%PF86%\Microsoft VS Code"

if not defined SRC (
    echo [错误] 在这台电脑上找不到已安装的 VSCode。
    echo.
    echo   办法 A（推荐）：去 https://code.visualstudio.com/download
    echo      下载 "Windows x64 ^(zip^)"，解压到 U 盘 \VSCode，
    echo      然后在 \VSCode 里手动新建一个空文件夹 data 即可（这一步就是便携模式）。
    echo   办法 B：先把本脚本拷到装过 VSCode 的电脑上跑。
    pause
    exit /b 1
)

echo 源目录 ：%SRC%
echo 目标   ：%DST%
echo.
echo [提示] 大约占 400~600 MB，U 盘慢的话要几分钟，别拔线。
echo.

REM ---------- 3. 复制 VSCode 本体 ----------
if not exist "%DST%" md "%DST%"
echo [1/3] 复制 VSCode 本体 ...
robocopy "%SRC%" "%DST%" /E /XD data /NFL /NDL /NJH /NJS /R:1 /W:1 >nul
if not exist "%DST%\Code.exe" (
    echo [错误] 复制失败：%DST%\Code.exe 不存在。
    pause
    exit /b 1
)
echo       OK

REM ---------- 4. 建 data（便携模式开关） + 扩展 + 设置 ----------
echo [2/3] 创建 data\ 目录（便携模式）...
if not exist "%DST%\data\extensions" md "%DST%\data\extensions"
if not exist "%DST%\data\user-data\User" md "%DST%\data\user-data\User"
echo       OK

echo [3/3] 复制本机扩展和用户设置 ...
if exist "%USERPROFILE%\.vscode\extensions" (
    robocopy "%USERPROFILE%\.vscode\extensions" "%DST%\data\extensions" /E /NFL /NDL /NJH /NJS /R:1 /W:1 >nul
    echo       已复制扩展：%USERPROFILE%\.vscode\extensions
) else (
    echo       [跳过] 本机没有 %USERPROFILE%\.vscode\extensions
)
if exist "%APPDATA%\Code\User\settings.json" (
    copy /y "%APPDATA%\Code\User\settings.json" "%DST%\data\user-data\User\settings.json" >nul
    echo       已复制用户设置：%APPDATA%\Code\User\settings.json
) else (
    echo       [跳过] 本机没有 %APPDATA%\Code\User\settings.json
)

echo.
echo ============================================================
echo  [完成] %DST%
echo ------------------------------------------------------------
echo  在实验室电脑上：
echo    1^) 双击 %DRIVE%:\VSCode\Code.exe 启动（这就是便携版，无需安装）
echo    2^) 打开工程：文件 ^-^> 打开文件夹 ^-^> 选 U 盘或本地拷过去的 STM32Hexapod
echo    3^) Ctrl+Shift+B 编译（等价于 tools\lab\build.bat）
echo    4^) Ctrl+Shift+P ^> Tasks: Run Task ^> 选「烧录：SWD / 串口 / DFU」
echo.
echo  ※ 中文界面：本脚本已带上语言包扩展；若界面还是英文，
echo    按 Ctrl+Shift+P 输入 "Configure Display Language" 选 zh-cn。
echo  ※ 便携版不会自动更新，要升级就重新跑一次本脚本。
echo  ※ 建议把工程拷到本地磁盘再编译，U 盘直跑索引会慢。
echo ============================================================
pause
exit /b 0
