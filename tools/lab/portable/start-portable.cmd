@echo off
chcp 936 >nul
setlocal
set "BASE=%~dp0"
set "BASE=%BASE:~0,-1%"
if not exist "%BASE%\data\extensions" (
  echo [错误] 找不到 %BASE%\data\extensions，确认是在 U 盘的 VSCode 目录里运行
  pause
  exit /b 1
)
rem 扩展注册表里存的是绝对路径（换电脑/换盘符就失效），删掉让 VS Code 重新扫描
if exist "%BASE%\data\extensions\extensions.json" del /f /q "%BASE%\data\extensions\extensions.json"
if exist "%BASE%\data\extensions\.obsolete" del /f /q "%BASE%\data\extensions\.obsolete"
echo [便携模式] 程序目录：%BASE%
echo [便携模式] 扩展目录：%BASE%\data\extensions
echo [便携模式] 配置目录：%BASE%\data\user-data
start "" "%BASE%\Code.exe" --user-data-dir "%BASE%\data\user-data" --extensions-dir "%BASE%\data\extensions" --disable-workspace-trust --disable-telemetry %*
endlocal
