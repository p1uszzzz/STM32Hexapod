@echo off
chcp 936 >nul
setlocal enabledelayedexpansion
cd /d "%~dp0..\.."
set "REPO=%CD%"
set "BUNDLES=%LOCALAPPDATA%\stm32cube\bundles"

echo ============================================================
echo  制作实验室 U 盘（免安装环境）
echo ============================================================
echo   仓库   ：%REPO%
echo   工具链 ：%BUNDLES%
echo.

if not exist "%BUNDLES%\gnu-tools-for-stm32" (
    echo [错误] 没找到 STM32 工具链目录：%BUNDLES%
    echo        如果你用过 VS Code 的 STM32 扩展，这个目录应该存在。
    pause
    exit /b 1
)

echo 现有 U 盘盘符：
powershell -NoProfile -Command "Get-Volume | Where-Object {$_.DriveType -eq 'Removable' -and $_.DriveLetter} | ForEach-Object { '   ' + $_.DriveLetter + ':  ' + $_.FileSystemLabel + '   剩余 ' + [math]::Round($_.SizeRemaining/1GB,1) + ' GB' }"
echo.
set /p "D=请输入 U 盘盘符（例如 E）："
if "%D%"=="" ( echo [取消] & pause & exit /b 1 )
set "D=%D:~0,1%:"
if not exist "%D%\" ( echo [错误] 盘符 %D% 不存在 & pause & exit /b 1 )

if not exist "%REPO%\build\Debug\STM32Hexapod.elf" (
    echo [警告] 还没有编译产物 build\Debug\STM32Hexapod.elf
    echo        建议先运行 tools\lab\build.bat 再制作 U 盘。
    echo.
    pause
)

echo.
echo [1/4] 复制仓库（不含 .git / build 中间文件）
robocopy "%REPO%" "%D%\STM32Hexapod" /E /NFL /NDL /NJH /NJS /XD "%REPO%\.git" "%REPO%\build" >nul
if not exist "%D%\STM32Hexapod\Core\Src\gait.c" (
    echo [错误] 仓库复制失败
    pause
    exit /b 1
)

echo [2/4] 复制固件产物
if not exist "%D%\STM32Hexapod\build\Debug" mkdir "%D%\STM32Hexapod\build\Debug"
for %%f in (STM32Hexapod.elf STM32Hexapod.hex STM32Hexapod.bin STM32Hexapod.map) do (
    if exist "%REPO%\build\Debug\%%f" copy /y "%REPO%\build\Debug\%%f" "%D%\STM32Hexapod\build\Debug\" >nul
)

echo [3/4] 复制免安装工具链（gcc / ninja / cmake / programmer）
robocopy "%BUNDLES%\gnu-tools-for-stm32" "%D%\bundles\gnu-tools-for-stm32" /E /NFL /NDL /NJH /NJS >nul
robocopy "%BUNDLES%\ninja"              "%D%\bundles\ninja"              /E /NFL /NDL /NJH /NJS >nul
robocopy "%BUNDLES%\cmake"              "%D%\bundles\cmake"              /E /NFL /NDL /NJH /NJS >nul
robocopy "%BUNDLES%\programmer"         "%D%\bundles\programmer"         /E /NFL /NDL /NJH /NJS >nul

echo [4/4] 复制驱动安装包（如果有）
if exist "%~dp0drivers" (
    robocopy "%~dp0drivers" "%D%\drivers" /E /NFL /NDL /NJH /NJS >nul
    echo       已复制 %~dp0drivers
) else (
    echo       [注意] 没有 drivers 文件夹，新电脑将装不上 USB 驱动！
    echo       请先看 tools\lab\drivers\下载清单.md 把驱动下载好。
)

echo.
echo ============================================================
echo  检查结果
echo ============================================================
set "OK=1"
if not exist "%D%\bundles\gnu-tools-for-stm32" set "OK="
if not exist "%D%\bundles\programmer" set "OK="
if not exist "%D%\bundles\ninja" set "OK="
if not exist "%D%\bundles\cmake" set "OK="
if defined OK (echo   工具链 ：OK) else (echo   工具链 ：缺失！)
if exist "%D%\STM32Hexapod\build\Debug\STM32Hexapod.elf" (echo   固件   ：OK) else (echo   固件   ：缺失！)
if exist "%D%\drivers" (echo   驱动   ：需自己下载清单里的安装包) else (echo   驱动   ：无)

echo.
echo U 盘做好了。到实验室后照 tools\lab\README-lab.md 第 0 节的三步走即可。
echo 别忘了另外带：ST-LINK 或 USB-TTL、杜邦线、5V 电源、机器人本体。
pause
