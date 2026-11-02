@echo off
chcp 936 >nul
setlocal enabledelayedexpansion
cd /d "%~dp0..\.."
REM 仓库根目录（本脚本位于 tools\lab\ 下）

echo ============================================================
echo  1/3 查找工具链（cmake / ninja / arm-none-eabi-gcc）
echo ============================================================

set "BUNDLES=%LOCALAPPDATA%\stm32cube\bundles"
set "USBBUNDLES=%~dp0..\..\..\bundles"

REM --- 工具链查找顺序：系统 PATH -> U 盘 bundles -> 本机 STM32 扩展的 bundles ---
set "CM="
set "NJ="
set "GCCPATH="
set "GCCDIR="
set "TCROOT="

REM 1) 先在系统 PATH 里找 cmake / ninja
where cmake >nul 2>&1 && set "CM=cmake"
where ninja >nul 2>&1 && set "NJ=ninja"

REM 2) 找一个可用的 bundles 根目录（哪边有 cmake 子目录就用哪边）
if not defined TCROOT if exist "%USBBUNDLES%\cmake" set "TCROOT=%USBBUNDLES%"
if not defined TCROOT if exist "%BUNDLES%\cmake" set "TCROOT=%BUNDLES%"
if not defined TCROOT if exist "%USBBUNDLES%\gnu-tools-for-stm32" set "TCROOT=%USBBUNDLES%"
if not defined TCROOT if exist "%BUNDLES%\gnu-tools-for-stm32" set "TCROOT=%BUNDLES%"

if defined TCROOT (
    if not defined CM for /d %%d in ("%TCROOT%\cmake\*") do set "CM=%%~d\bin\cmake.exe"
    if not defined NJ for /d %%d in ("%TCROOT%\ninja\*") do set "NJ=%%~d\ninja.exe"
    for /d %%d in ("%TCROOT%\gnu-tools-for-stm32\*") do set "GCCDIR=%%~d\bin"
)

REM 3) gcc：PATH 优先，其次 bundles
where arm-none-eabi-gcc >nul 2>&1 && set "GCCPATH=PATH"
if not defined GCCPATH if defined GCCDIR set "GCCPATH=BUNDLE"

if not defined GCCPATH (
    echo [错误] 找不到 arm-none-eabi-gcc。
    echo   方案 A：把工具链 bin 目录加进系统 PATH
    echo   方案 B：把 bundles 整个文件夹放到U盘根目录（脚本会自动使用）
    if /i not "%~1"=="--no-pause" pause
    exit /b 1
)
if not defined CM (
    echo [错误] 找不到 cmake。
    echo   方案 A：安装 CMake 并加进 PATH
    echo   方案 B：把 bundles 整个文件夹放到U盘根目录（脚本会自动使用）
    if /i not "%~1"=="--no-pause" pause
    exit /b 1
)
if not defined NJ (
    echo [错误] 找不到 ninja。
    echo   方案 A：安装 ninja 并加进 PATH
    echo   方案 B：把 bundles 整个文件夹放到U盘根目录（脚本会自动使用）
    if /i not "%~1"=="--no-pause" pause
    exit /b 1
)

echo   工具链根：%TCROOT%
echo   gcc    ：%GCCPATH%
echo   cmake  ：%CM%
echo   ninja  ：%NJ%

REM 让 gcc / objcopy / ninja / cmake 可用
if defined GCCDIR set "PATH=%GCCDIR%;%PATH%"
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
if /i not "%~1"=="--no-pause" pause
exit /b 0

:FAIL
echo.
echo [失败] 编译没通过，请把上面的红色/错误信息发出来。
if /i not "%~1"=="--no-pause" pause
exit /b 1
