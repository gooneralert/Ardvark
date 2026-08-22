@echo off
rem ============================================================
rem  updater_part2.bat - Part 2 of 2   (lives inside src)
rem  Everything after the source install:
rem    Step 3 - Merge offsets and replace Offsets.h
rem    Step 4 - Build the external with MSBuild (Release x64)
rem    Step 5 - Create a shortcut one folder above src
rem  Launched automatically by updater.bat (part 1) from the
rem  freshly downloaded src, or run it directly from src.
rem
rem  NOTE: this file ships inside the repo's src/ folder, so it
rem  arrives with the downloaded source. The offset-merge params
rem  (updater.ps1) and version.txt live at the repo root, i.e.
rem  one folder up from here.
rem ============================================================
setlocal

cd /d "%~dp0"

rem ============================================================
rem  Step 3 - Merge offsets and replace Offsets.h
rem ============================================================
echo [*] Merging offsets and replacing src\src\core\roblox\offsets\Offsets.h...
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\updater.ps1"
if errorlevel 1 (
    echo [!] Offset merge failed.
    pause
    exit /b 1
)
echo [+] Offsets updated.
echo.

rem ============================================================
rem  Step 4 - Build the external with MSBuild (Release x64)
rem ============================================================
echo [*] Locating Visual Studio / MSBuild...
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSINSTALL="
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
)

if not defined VSINSTALL (
    echo [!] Could not find Visual Studio with the C++ workload.
    echo     Install "Desktop development with C++" and try again.
    pause
    exit /b 1
)

echo [*] Setting up the MSBuild / VC environment...
call "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" -arch=x64 >nul
if errorlevel 1 (
    echo [!] Failed to set up the Visual Studio build environment.
    pause
    exit /b 1
)

echo [*] Building external (MSBuild Release x64)...
msbuild "%~dp0jewsploit.sln" /m /p:Configuration=Release /p:Platform=x64 /v:minimal
if errorlevel 1 (
    echo [!] Build failed.
    pause
    exit /b 1
)
echo [+] Build complete.
echo.

rem ============================================================
rem  Step 5 - Create a shortcut one folder above src
rem ============================================================
set "TARGET_EXE=%~dp0x64\Release\jewsploit.exe"
if not exist "%TARGET_EXE%" (
    echo [!] Built executable not found: %TARGET_EXE%
    pause
    exit /b 1
)

echo [*] Creating shortcut one folder above src...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$s=(New-Object -COM WScript.Shell).CreateShortcut('%~dp0..\Ardvark.lnk');$s.TargetPath='%TARGET_EXE%';$s.WorkingDirectory='%~dp0x64\Release';$s.Save()"
if errorlevel 1 (
    echo [!] Failed to create the shortcut.
    pause
    exit /b 1
)

echo.
echo [+] Done.
echo     Shortcut : %~dp0..\Ardvark.lnk
echo     Target   : %TARGET_EXE%
echo.

endlocal