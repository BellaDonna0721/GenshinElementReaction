@echo off
chcp 65001 >nul
echo ========================================
echo   GenshinElement - 构建脚本
echo ========================================

:: 设置 Emscripten 环境
set PATH=D:\emsdk\upstream\bin;D:\emsdk\upstream\emscripten;D:\emsdk\node\22.16.0_64bit\bin;D:\emsdk\python\3.13.3_64bit;%PATH%

:: ===== 每次构建强制清理 build 缓存，彻底避免规则/结构体改了但没重编的问题 =====
if exist build\ (
    echo [Cleanup] 清理旧 build 目录...
    rmdir /S /Q build 2>nul
)

echo [1/3] 重新配置 CMake...
emcmake cmake -B build -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 goto :error

echo [2/3] 编译 C++ -> WASM...
cmake --build build
if %errorlevel% neq 0 goto :error

echo [3/3] 编译 TypeScript...
esbuild web\main.ts --bundle --outfile=dist\main.js --format=esm
if %errorlevel% neq 0 goto :error

echo ========================================
echo   构建成功！执行 serve.bat 启动游戏
echo ========================================
pause
exit /b 0

:error
echo ========================================
echo   构建失败！请检查上方错误信息
echo ========================================
pause
exit /b 1