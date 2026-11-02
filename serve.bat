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

:: 用自定义 HTTP 服务器（强制 no-cache，防止 Edge 缓存旧版 wasm/js）
python serve.py 3000

pause