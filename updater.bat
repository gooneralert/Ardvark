@echo off
rem ============================================================
rem  updater.bat - Runner for the two-part updater
rem  Part 1 (updater.ps1)        : installs the latest src from GitHub
rem  Part 2 (src\updater_part2.ps1): offset merge, build, shortcut
rem  Part 1 boots Part 2 automatically from inside the freshly
rem  installed src. Run updater.bat for a full update, or run
rem  src\updater_part2.ps1 directly to just re-merge offsets +
rem  rebuild with existing source.
rem ============================================================
setlocal
cd /d "%~dp0"

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0updater.ps1"
if errorlevel 1 (
    echo [!] Updater failed.
    pause
    exit /b 1
)

echo.
echo [+] Full update complete.

endlocal
