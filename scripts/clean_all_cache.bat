@echo off
REM Clean all cache folders that idf.py clean doesn't remove
REM This script removes .cache folders and other persistent cache files

echo Cleaning cache folders...

REM Remove .cache folders
if exist "components\.cache" (
    echo Removing components\.cache
    rmdir /s /q "components\.cache"
)

if exist ".cache" (
    echo Removing .cache
    rmdir /s /q ".cache"
)

REM Remove CMakeCache.txt files
if exist "build\CMakeCache.txt" (
    echo Removing build\CMakeCache.txt
    del /q "build\CMakeCache.txt"
)

if exist "build\bootloader\CMakeCache.txt" (
    echo Removing build\bootloader\CMakeCache.txt
    del /q "build\bootloader\CMakeCache.txt"
)

echo Cache cleanup complete.

