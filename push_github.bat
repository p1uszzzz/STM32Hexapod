@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"

echo ============================================================
echo  STM32Hexapod - 提交并推送到 GitHub
echo  远端：https://github.com/p1uszzzz/STM32Hexapod.git  分支：main
echo ============================================================
echo.

where git >nul 2>&1
if errorlevel 1 (
    echo [错误] 没找到 git，请先安装 Git for Windows：https://git-scm.com/download/win
    pause
    exit /b 1
)

git status --short
echo.

git add -A
git commit -m "腿长改为实测值 L1=22.3/L2=57.9/L3=110；角度层按装配中位重写（中位锚点+左右方向表+18路trim+软限位）；LEG_HOME 修正坐标系并结合新腿长取 z=-L3；IK 越界改为投影到可达边界；新增 tools/ik_check.py 与实验室便携构建/烧录脚本"
if errorlevel 1 (
    echo.
    echo [提示] 提交没成功。若是提示 "Please tell me who you are"，先设置身份：
    echo     git config --global user.name  "你的名字"
    echo     git config --global user.email "你的邮箱"
    echo     然后重新运行本脚本。
    echo 若只是没有改动可提交，可以忽略，下面照样会尝试推送。
)

echo.
echo ---- 开始推送 ----
git push origin main
if errorlevel 1 (
    echo.
    echo [提示] 推送失败。常见原因：
    echo   1) 远端有别人的新提交 -^> 先执行：git pull --rebase origin main
    echo   2) 没有登录凭据 -^> 弹出窗口登录 GitHub，或用 PAT 作为密码
    echo   3) 网络问题 -^> 检查代理 / 重试
    echo.
    echo 完整命令：
    echo     git pull --rebase origin main
    echo     git push origin main
    pause
    exit /b 1
)

echo.
echo [完成] 已推送到 main。
git log --oneline -3
pause
