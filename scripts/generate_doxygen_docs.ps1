# PowerShell script to generate Doxygen documentation
# This script checks for Doxygen installation and generates documentation

Write-Host "ESP32-P4 EtherNet/IP Device - Doxygen Documentation Generator" -ForegroundColor Cyan
Write-Host "=============================================================" -ForegroundColor Cyan
Write-Host ""

# Check if Doxygen is installed
$doxygenPath = $null
try {
    $doxygenPath = Get-Command doxygen -ErrorAction Stop
    Write-Host "[OK] Doxygen found at: $($doxygenPath.Source)" -ForegroundColor Green
    Write-Host "[OK] Version: $(& doxygen --version)" -ForegroundColor Green
} catch {
    Write-Host "[ERROR] Doxygen is not installed or not in PATH" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please install Doxygen using one of these methods:" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Method 1: Download installer" -ForegroundColor Yellow
    Write-Host "  https://www.doxygen.nl/download.html" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Method 2: Using winget (if available)" -ForegroundColor Yellow
    Write-Host "  winget install Doxygen.Doxygen" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Method 3: Using Chocolatey (if installed)" -ForegroundColor Yellow
    Write-Host "  choco install doxygen" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "After installation, restart your terminal and run this script again." -ForegroundColor Yellow
    exit 1
}

# Check if Doxyfile exists
if (-not (Test-Path "Doxyfile")) {
    Write-Host "[ERROR] Doxyfile not found in current directory" -ForegroundColor Red
    Write-Host "Please run this script from the project root directory" -ForegroundColor Yellow
    exit 1
}

Write-Host ""
Write-Host "[INFO] Configuration file found: Doxyfile" -ForegroundColor Green
Write-Host ""

# Create output directory if it doesn't exist
$outputDir = "docs\doxygen"
if (-not (Test-Path $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
    Write-Host "[INFO] Created output directory: $outputDir" -ForegroundColor Green
}

Write-Host "[INFO] Generating documentation..." -ForegroundColor Yellow
Write-Host ""

# Run Doxygen
$doxygenOutput = & doxygen Doxyfile 2>&1
$exitCode = $LASTEXITCODE

if ($exitCode -eq 0) {
    Write-Host ""
    Write-Host "[SUCCESS] Documentation generated successfully!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Output location: $outputDir\html\index.html" -ForegroundColor Cyan
    Write-Host ""
    
    # Ask if user wants to open the documentation
    $openDocs = Read-Host "Would you like to open the documentation in your browser? (Y/N)"
    if ($openDocs -eq "Y" -or $openDocs -eq "y") {
        $indexPath = Join-Path $outputDir "html\index.html"
        if (Test-Path $indexPath) {
            Start-Process $indexPath
            Write-Host "[INFO] Opening documentation..." -ForegroundColor Green
        } else {
            Write-Host "[WARNING] Index file not found at expected location" -ForegroundColor Yellow
        }
    }
} else {
    Write-Host ""
    Write-Host "[ERROR] Doxygen generation failed with exit code: $exitCode" -ForegroundColor Red
    Write-Host ""
    Write-Host "Doxygen output:" -ForegroundColor Yellow
    Write-Host $doxygenOutput
    exit $exitCode
}

Write-Host ""
Write-Host "Done!" -ForegroundColor Green

