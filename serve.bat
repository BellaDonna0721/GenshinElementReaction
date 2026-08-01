@echo off
chcp 65001 >nul
echo ========================================
echo   GenshinElement - 启动服务器
echo ========================================
echo.
echo   浏览器打开: http://localhost:3000/web/index.html
echo   按 Ctrl+C 停止服务器
echo ========================================
echo.

:: 用 Python 内置 HTTP 服务器（Emscripten 自带 Python）
python -m http.server 3000

pause