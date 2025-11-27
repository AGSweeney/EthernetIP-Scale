# Cache Cleanup Scripts

This directory contains scripts to clean cache folders. **Note:** As of the latest update, `idf.py clean` should now automatically remove `.cache` folders thanks to the `ADDITIONAL_CLEAN_FILES` property in `CMakeLists.txt`. However, these scripts are still available as a backup or for manual cleanup.

## Problem

ESP-IDF's `idf.py clean` command doesn't always remove all cache folders, particularly:
- `.cache` folders in the `components/` directory
- `.cache` folders in the project root
- CMake cache files that may persist

These cache folders can sometimes cause build issues if they contain stale data.

## Automatic Cleanup

The project's `CMakeLists.txt` has been configured with `ADDITIONAL_CLEAN_FILES` to automatically remove `.cache` folders when you run `idf.py clean`. This means you should no longer need to manually clean these folders in most cases.

## Solutions

### Option 1: Use the Clean Scripts

**PowerShell (Recommended for Windows):**
```powershell
.\scripts\clean_all_cache.ps1
```

**Dry run (see what would be removed):**
```powershell
.\scripts\clean_all_cache.ps1 -DryRun
```

**Batch file (Windows):**
```cmd
scripts\clean_all_cache.bat
```

### Option 2: Manual Cleanup

You can manually remove these folders:
- `components\.cache`
- `.cache` (in project root)
- `build\CMakeCache.txt`
- `build\bootloader\CMakeCache.txt`

### Option 3: Full Clean (Recommended)

For a complete clean, run both:
```bash
idf.py clean
.\scripts\clean_all_cache.ps1
```

Or on Windows CMD:
```cmd
idf.py clean
scripts\clean_all_cache.bat
```

## When to Use Manual Scripts

Use the manual cleanup scripts if:
- You're still experiencing cache-related build issues after `idf.py clean`
- You want to clean cache folders without running a full clean
- You need to verify what would be cleaned (dry-run mode)

## What Gets Cleaned

- `components/.cache` - Component manager cache
- `.cache` - Project-level cache
- `build/CMakeCache.txt` - CMake cache files
- `build/bootloader/CMakeCache.txt` - Bootloader CMake cache

