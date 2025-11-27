@echo off
REM Batch script to generate Doxygen documentation
REM This script checks for Doxygen installation and generates documentation

echo ESP32-P4 EtherNet/IP Device - Doxygen Documentation Generator
echo =============================================================
echo.

REM Check if Doxygen is installed
where doxygen >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Doxygen is not installed or not in PATH
    echo.
    echo Please install Doxygen using one of these methods:
    echo.
    echo Method 1: Download installer
    echo   https://www.doxygen.nl/download.html
    echo.
    echo Method 2: Using winget (if available)
    echo   winget install Doxygen.Doxygen
    echo.
    echo Method 3: Using Chocolatey (if installed)
    echo   choco install doxygen
    echo.
    echo After installation, restart your terminal and run this script again.
    exit /b 1
)

echo [OK] Doxygen found
doxygen --version
echo.

REM Check if Doxyfile exists
if not exist "Doxyfile" (
    echo [ERROR] Doxyfile not found in current directory
    echo Please run this script from the project root directory
    exit /b 1
)

echo [INFO] Configuration file found: Doxyfile
echo.

REM Create output directory if it doesn't exist
if not exist "docs\doxygen" mkdir "docs\doxygen"

echo [INFO] Generating documentation...
echo.

REM Run Doxygen
doxygen Doxyfile

if %ERRORLEVEL% EQU 0 (
    echo.
    echo [SUCCESS] Documentation generated successfully!
    echo.
    echo Output location: docs\doxygen\html\index.html
    echo.
    set /p openDocs="Would you like to open the documentation in your browser? (Y/N): "
    if /i "%openDocs%"=="Y" (
        if exist "docs\doxygen\html\index.html" (
            start "" "docs\doxygen\html\index.html"
            echo [INFO] Opening documentation...
        ) else (
            echo [WARNING] Index file not found at expected location
        )
    )
) else (
    echo.
    echo [ERROR] Doxygen generation failed
    exit /b %ERRORLEVEL%
)

echo.
echo Done!

