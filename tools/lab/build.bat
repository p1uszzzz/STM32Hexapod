@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion
cd /d "%~dp0..\.."
REM 仓库根目录（本脚本位于 tools\lab\ 下）

echo ============================================================
echo  1/3 查找工具链（cmake / ninja / arm-none-eabi-gcc）
echo ============================================================

set "BUNDLES=%LOCALAPPDATA%\stm32cube\bundles"
set "USBBUNDLES=%~dp0..\..\..\bundles"

REM --- 依次尝试：系统 PATH -> USB 里的 bundles -> 本机 STM32 扩展的 bundles ---
set "CM=cmake"
set "NJ=ninja"
set "GCCPATH="

where arm-none-eabi-gcc >nul 2>&1 && set "GCCPATH=PATH"

if not defined GCCPATH if exist "%USBBUNDLES%\gnu-tools-for-stm32" (
    set "TCROOT=%USBBUNDLES%"
    set "GCCPATH=USB"
)
if not defined GCCPATH if exist "%BUNDLES%\gnu-tools-for-stm32" (
    set "TCROOT=%BUNDLES%"
    set "GCCPATH=APPDATA"
)

if defined TCROOT (
    for /d %%d in ("%TCROOT%\gnu-tools-for-stm32\*") do set "GCCDIR=%%d\bin"
    for /d %%d in ("%TCROOT%\ninja\*") do set "NJ=%%d\ninja.exe"
    for /d %%d in ("%TCROOT%\cmake\*") do set "CM=%%d\bin\cmake.exe"
)

if not defined GCCPATH (
    echo [错误] 找不到 arm-none-eabi-gcc。
    echo   方案 A：把 %BUNDLES% 整个文件夹拷到 U 盘根目录并改名为 bundles
    echo   方案 B：在实验室电脑装 STM32CubeCLT / 把工具链加进 PATH
    pause
    exit /b 1
)

if /i "%GCCPATH%"=="PATH" (
    echo   工具链：系统 PATH
) else (
    echo   工具链：%TCROOT%
)
echo   gcc    ：%GCCPATH%
echo   cmake  ：%CM%
echo   ninja  ：%NJ%

REM 让 gcc/objcopy 可用
if /i not "%GCCPATH%"=="PATH" set "PATH=%GCCDIR%;%PATH%"
for %%p in ("%NJ%") do set "PATH=%%~dpp;%PATH%"
for %%p in ("%CM%") do set "PATH=%%~dpp;%PATH%"

echo.
echo ============================================================
echo  2/3 配置 + 编译
echo ============================================================
"%CM%" -B build/Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake
if errorlevel 1 goto FAIL

"%CM%" --build build/Debug
if errorlevel 1 goto FAIL

echo.
echo ============================================================
echo  3/3 生成 .hex / .bin（CubeProgrammer 用 .elf 也行）
echo ============================================================
arm-none-eabi-objcopy -O ihex   build/Debug/STM32Hexapod.elf build/Debug/STM32Hexapod.hex
arm-none-eabi-objcopy -O binary build/Debug/STM32Hexapod.elf build/Debug/STM32Hexapod.bin
if errorlevel 1 goto FAIL
arm-none-eabi-size build/Debug/STM32Hexapod.elf

echo.
echo [完成] 产物：
echo     build\Debug\STM32Hexapod.elf / .hex / .bin
echo 烧录：tools\lab\flash.bat  （或 tools\lab\flash.bat COM4  走串口）
pause
exit /b 0

:FAIL
echo.
echo [失败] 编译没通过，请把上面的红色/错误信息发出来。
pause
exit /b 1
