@echo off
setlocal

cd /d "%~dp0"

set "REPO_ZIP_URL=https://github.com/gooneralert/Ardvark/archive/refs/heads/main.zip"
set "TEMP_ZIP=%TEMP%\ardvark_src_update.zip"
set "TEMP_EXTRACT=%TEMP%\ardvark_src_update"
set "SRC_DIR=%~dp0src"

rem ============================================================
rem  Step 1 - Delete the existing src folder
rem ============================================================
echo [*] Deleting existing src folder...
if exist "%SRC_DIR%" (
    rmdir /S /Q "%SRC_DIR%"
    if errorlevel 1 (
        echo [!] Failed to delete src folder.
        pause
        exit /b 1
    )
)
echo [+] src deleted.

rem ============================================================
rem  Step 2 - Download the src folder from GitHub
rem ============================================================
echo [*] Downloading latest repo from GitHub...
if exist "%TEMP_ZIP%" del /Q "%TEMP_ZIP%"
powershell -NoProfile -ExecutionPolicy Bypass -Command "Invoke-WebRequest -Uri '%REPO_ZIP_URL%' -OutFile '%TEMP_ZIP%' -UseBasicParsing"
if errorlevel 1 (
    echo [!] Failed to download the repo. Check your internet connection.
    pause
    exit /b 1
)

echo [*] Extracting archive...
if exist "%TEMP_EXTRACT%" rmdir /S /Q "%TEMP_EXTRACT%"
powershell -NoProfile -ExecutionPolicy Bypass -Command "Expand-Archive -Path '%TEMP_ZIP%' -DestinationPath '%TEMP_EXTRACT%' -Force"
if errorlevel 1 (
    echo [!] Failed to extract the archive.
    pause
    exit /b 1
)

rem The zip extracts to a top-level "<Repo>-<branch>" folder; find its src subfolder.
set "EXTRACTED_SRC=%TEMP_EXTRACT%\Ardvark-main\src"
if not exist "%EXTRACTED_SRC%" set "EXTRACTED_SRC=%TEMP_EXTRACT%\Ardvark-master\src"
if not exist "%EXTRACTED_SRC%" (
    echo [!] Could not find the src folder inside the downloaded archive.
    pause
    exit /b 1
)

echo [*] Copying src folder into place...
xcopy /E /I /Y "%EXTRACTED_SRC%" "%SRC_DIR%" >nul
if errorlevel 1 (
    echo [!] Failed to copy the src folder.
    pause
    exit /b 1
)

echo [*] Cleaning up temporary files...
rmdir /S /Q "%TEMP_EXTRACT%" 2>nul
del /Q "%TEMP_ZIP%" 2>nul
echo [+] src updated.

rem ============================================================
rem  Step 3 - Merge offsets and replace Offsets.h
rem ============================================================
echo [*] Merging offsets and replacing src\src\core\roblox\offsets\Offsets.h...
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0updater.ps1"
if errorlevel 1 (
    echo [!] Offset merge failed.
    pause
    exit /b 1
)
echo [+] Offsets updated.

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
msbuild "%~dp0src\jewsploit.sln" /m /p:Configuration=Release /p:Platform=x64 /v:minimal
if errorlevel 1 (
    echo [!] Build failed.
    pause
    exit /b 1
)
echo [+] Build complete.

rem ============================================================
rem  Step 5 - Create a shortcut one folder above src
rem ============================================================
set "TARGET_EXE=%~dp0src\x64\Release\jewsploit.exe"
if not exist "%TARGET_EXE%" (
    echo [!] Built executable not found: %TARGET_EXE%
    pause
    exit /b 1
)

echo [*] Creating shortcut one folder above src...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$s=(New-Object -COM WScript.Shell).CreateShortcut('%~dp0Ardvark.lnk');$s.TargetPath='%TARGET_EXE%';$s.WorkingDirectory='%~dp0src\x64\Release';$s.Save()"
if errorlevel 1 (
    echo [!] Failed to create the shortcut.
    pause
    exit /b 1
)

echo.
echo [+] Done.
echo     Shortcut : %~dp0Ardvark.lnk
echo     Target   : %TARGET_EXE%
echo.

endlocal
