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
rem  Step 2 - Download the src folder from GitHub using curl
rem ============================================================
echo [*] Downloading latest repo from GitHub (using curl)...
if exist "%TEMP_ZIP%" del /Q "%TEMP_ZIP%"

curl -L -o "%TEMP_ZIP%" "%REPO_ZIP_URL%"
if errorlevel 1 (
    echo [!] Failed to download the repo. Check your internet connection.
    pause
    exit /b 1
)

echo [*] Extracting archive (fast mode)...
if exist "%TEMP_EXTRACT%" rmdir /S /Q "%TEMP_EXTRACT%"
mkdir "%TEMP_EXTRACT%" 2>nul

rem Try tar first (built-in on Win10/11), then fallback to .NET ZipFile
where tar >nul 2>nul
if %errorlevel% equ 0 (
    tar -xf "%TEMP_ZIP%" -C "%TEMP_EXTRACT%"
) else (
    powershell -NoProfile -Command "Add-Type -AssemblyName System.IO.Compression.FileSystem; [System.IO.Compression.ZipFile]::ExtractToDirectory('%TEMP_ZIP%', '%TEMP_EXTRACT%')"
)
if errorlevel 1 (
    echo [!] Extraction failed.
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
rem  Part 1 of 2 is done - boot Part 2 (updater_part2.bat) from
rem  inside the freshly installed src. Part 2 does everything
rem  else: offset merge, build, and shortcut.
rem ============================================================
echo.
echo [+] Part 1 complete - src installed.
echo [*] Booting Part 2 from installed src (offset merge, build, shortcut)...
echo.
if not exist "%SRC_DIR%\updater_part2.bat" (
    echo [!] updater_part2.bat not found inside the installed src.
    pause
    exit /b 1
)
call "%SRC_DIR%\updater_part2.bat"
set "PART2_CODE=%ERRORLEVEL%"

endlocal & exit /b %PART2_CODE%