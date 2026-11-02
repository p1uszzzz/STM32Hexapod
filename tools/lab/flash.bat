@echo off
chcp 936 >nul
setlocal enabledelayedexpansion
cd /d "%~dp0..\.."

echo ============================================================
echo  烧录 STM32Hexapod（STM32F103C8T6）
echo ------------------------------------------------------------
echo  用法：
echo    flash.bat            用 ST-LINK 走 SWD 烧录（推荐）
echo    flash.bat COM4       用 USB-TTL 走串口烧录（需 BOOT0=1 后复位）
echo    flash.bat DFU        用芯片自带 USB DFU 烧录（需 BOOT0=1 后复位）
echo ============================================================
echo.

set "BUNDLES=%LOCALAPPDATA%\stm32cube\bundles"
set "USBBUNDLES=%~dp0..\..\..\bundles"
set "CLI="

REM 找 STM32_Programmer_CLI.exe：USB 优先，其次本机 STM32 扩展
for /d %%d in ("%USBBUNDLES%\programmer\*") do if exist "%%d\bin\STM32_Programmer_CLI.exe" set "CLI=%%d\bin\STM32_Programmer_CLI.exe"
if not defined CLI for /d %%d in ("%BUNDLES%\programmer\*") do if exist "%%d\bin\STM32_Programmer_CLI.exe" set "CLI=%%d\bin\STM32_Programmer_CLI.exe"
if not defined CLI (
    where STM32_Programmer_CLI.exe >nul 2>&1 && set "CLI=STM32_Programmer_CLI.exe"
)
if not defined CLI (
    echo [错误] 找不到 STM32_Programmer_CLI.exe
    echo   把它一起拷到 U 盘： bundles\programmer\  （免安装，直接运行）
    pause
    exit /b 1
)
echo   烧录工具：%CLI%

set "HEX=build\Debug\STM32Hexapod.hex"
set "BIN=build\Debug\STM32Hexapod.bin"
if not exist "%HEX%" (
    echo [错误] 没有找到 %HEX%，请先跑 tools\lab\build.bat
    pause
    exit /b 1
)

set "MODE=%~1"
if "%MODE%"=="" goto SWD
if /i "%MODE%"=="SWD" goto SWD

if /i "%MODE%"=="DFU" (
    if not exist "%BIN%" (
        echo [错误] DFU 需要 %BIN%，请先跑 build.bat
        pause
        exit /b 1
    )
    echo.
    echo [DFU] BOOT0 跳线置 1，按一下复位，然后继续...
    pause
    "%CLI%" -c port=USB1 -w "%BIN%" 0x08000000 -v -rst
    goto DONE
)

REM 其余情况当作串口，例如 COM4 / COM12
echo.
echo [串口] 用 %MODE% 烧录：BOOT0 跳线置 1，按一下复位，然后继续...
echo        烧完记得把 BOOT0 拨回 0 再复位，才会运行新程序。
pause
"%CLI%" -c port=%MODE% br=115200 -w "%HEX%" -v -rst
goto DONE

:SWD
echo.
echo [SWD] 检查接线：SWDIO-PA13 / SWCLK-PA14 / GND / 3.3V
"%CLI%" -c port=SWD -w "%HEX%" -v -rst
if errorlevel 1 (
    echo.
    echo [提示] 连不上时依次检查：
    echo   1^) ST-LINK 驱动装了吗（STSW-LINK009）
    echo   2^) 板子供电了吗（ST-LINK 的 3.3V 一般带不动 18 个舵机）
    echo   3^) 上次烧录是否改了读保护 -^> 加 -ob RDP=0xAA 全片擦除
)

:DONE
echo.
echo ============================================================
echo  [安全提示] 上电后 18 路舵机会依次转到 90°（装配中位），
echo  再执行 Gait_Init^(^) 摆到 home 姿态。
echo  ★ 第一次烧录：先不要给舵机供电，只插 USB 看板子是否正常；
echo  ★ 给舵机供电时把机器人架空/放倒，手边随时可以断电。
echo ============================================================
pause
