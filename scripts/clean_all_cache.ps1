#!/usr/bin/env pwsh
# Clean all cache folders that idf.py clean doesn't remove
# This script removes:
# - .cache folders in components
# - CMakeCache.txt files
# - Other persistent cache files

param(
    [switch]$DryRun = $false
)

$ErrorActionPreference = "Continue"

Write-Host "Cleaning cache folders..." -ForegroundColor Cyan

# List of cache locations to clean
$cachePaths = @(
    "components\.cache",
    "build\CMakeCache.txt",
    "build\bootloader\CMakeCache.txt",
    "build\CMakeFiles\CMakeCache.txt",
    ".cache"
)

$removedCount = 0
$totalSize = 0

foreach ($path in $cachePaths) {
    if (Test-Path $path) {
        $item = Get-Item $path -Force -ErrorAction SilentlyContinue
        if ($item) {
            $size = if ($item.PSIsContainer) {
                (Get-ChildItem $path -Recurse -File -ErrorAction SilentlyContinue | 
                 Measure-Object -Property Length -Sum).Sum
            } else {
                $item.Length
            }
            
            $sizeMB = [math]::Round($size / 1MB, 2)
            
            if ($DryRun) {
                Write-Host "  [DRY RUN] Would remove: $path ($sizeMB MB)" -ForegroundColor Yellow
            } else {
                try {
                    Remove-Item $path -Recurse -Force -ErrorAction Stop
                    Write-Host "  Removed: $path ($sizeMB MB)" -ForegroundColor Green
                    $removedCount++
                    $totalSize += $size
                } catch {
                    Write-Host "  Failed to remove: $path - $($_.Exception.Message)" -ForegroundColor Red
                }
            }
        }
    }
}

# Also clean CMake cache files in build directory
$cmakeCacheFiles = Get-ChildItem -Path "build" -Filter "CMakeCache.txt" -Recurse -Force -ErrorAction SilentlyContinue
foreach ($file in $cmakeCacheFiles) {
    if ($DryRun) {
        Write-Host "  [DRY RUN] Would remove: $($file.FullName)" -ForegroundColor Yellow
    } else {
        try {
            Remove-Item $file.FullName -Force -ErrorAction Stop
            Write-Host "  Removed: $($file.FullName)" -ForegroundColor Green
            $removedCount++
        } catch {
            Write-Host "  Failed to remove: $($file.FullName) - $($_.Exception.Message)" -ForegroundColor Red
        }
    }
}

if (-not $DryRun) {
    $totalSizeMB = [math]::Round($totalSize / 1MB, 2)
    Write-Host "`nCleaned $removedCount cache item(s), freed $totalSizeMB MB" -ForegroundColor Cyan
} else {
    Write-Host "`n[DRY RUN] Would clean $removedCount cache item(s)" -ForegroundColor Yellow
}

