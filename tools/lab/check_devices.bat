@echo off
chcp 936 >nul
setlocal
cd /d "%~dp0..\.."

echo ============================================================
echo  检查烧录环境（在新电脑上运行这个，确认能不能烧）
echo ============================================================
echo.

echo [1] 有没有调试器 / 串口设备被识别
echo     （插上 ST-LINK 或 USB-TTL 后再看，空列表=没识别）
powershell -NoProfile -Command "Get-PnpDevice -PresentOnly | Where-Object { $_.FriendlyName -match 'ST-?Link|STM32|STMicro|CH340|CH341|CP210|Silicon Labs|USB-SERIAL|USB Serial' } | ForEach-Object { '    ' + $_.Status + '  ' + $_.FriendlyName }"
echo.

echo [2] 串口号
powershell -NoProfile -Command "$p=[System.IO.Ports.SerialPort]::GetPortNames(); if($p){$p | ForEach-Object {'    已发现 '+$_}}else{'    （没有串口，用 ST-LINK/SWD 的时候正常）'}"
echo.

echo [3] 编程器能不能列出探针
set "BUNDLES=%LOCALAPPDATA%\stm32cube\bundles"
set "USBBUNDLES=%~dp0..\..\..\bundles"
set "CLI="
for /d %%d in ("%USBBUNDLES%\programmer\*") do if exist "%%d\bin\STM32_Programmer_CLI.exe" set "CLI=%%d\bin\STM32_Programmer_CLI.exe"
if not defined CLI for /d %%d in ("%BUNDLES%\programmer\*") do if exist "%%d\bin\STM32_Programmer_CLI.exe" set "CLI=%%d\bin\STM32_Programmer_CLI.exe"
if not defined CLI (
    echo     找不到 STM32_Programmer_CLI.exe，U 盘里的 bundles 是不是没复制全？
) else (
    echo     使用：%CLI%
    "%CLI%" -l
)
echo.

echo ============================================================
echo  判读方法
echo    * 第 1 节出现 "ST-Link" 且状态 OK  -^> SWD 可以烧
echo    * 第 3 节列出 ST-LINK SN           -^> 驱动已就绪
echo    * 出现 CH340/CP210x 且第 2 节有 COMx -^> 串口可以烧
echo    * 三样都没有 -^> 驱动没装（设备管理器里会有黄色感叹号）
echo ============================================================
pause
