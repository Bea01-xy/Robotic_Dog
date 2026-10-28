@echo off
chcp 65001 >nul 2>&1
REM ========================================
REM  STM32H723 一键编译烧录 (Windows 批处理)
REM  转发到 PowerShell 脚本执行
REM ========================================

set "SCRIPT_DIR=%~dp0"
set "PS_SCRIPT=%SCRIPT_DIR%flash_openocd.ps1"

REM 检查脚本是否存在
if not exist "%PS_SCRIPT%" (
    echo [错误] 找不到 PowerShell 脚本: %PS_SCRIPT%
    pause
    exit /b 1
)

REM 检查是否传了参数
if "%1"=="" (
    REM 无参数 -> 编译 + 烧录
    powershell -NoProfile -ExecutionPolicy Bypass -File "%PS_SCRIPT%"
) else (
    REM 传了参数（ELF 路径）-> 跳过编译直接烧录
    powershell -NoProfile -ExecutionPolicy Bypass -File "%PS_SCRIPT%" -ElfPath "%~1" -NoBuild
)

if %ERRORLEVEL% neq 0 (
    echo.
    echo [错误] 烧录失败，错误码: %ERRORLEVEL%
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo 按任意键退出...
pause >nul 2>&1
