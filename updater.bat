@echo off
setlocal
cd /d "%~dp0"

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0updater.ps1"

if %errorlevel% neq 0 (
    echo.
    echo [ERROR] updater.ps1 failed.
    pause
) else (
    echo.
    echo Done.
    timeout /t 2 >nul
)

endlocal