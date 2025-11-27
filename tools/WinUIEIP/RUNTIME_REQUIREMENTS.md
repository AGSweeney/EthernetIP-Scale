# Windows App SDK Runtime Requirements

This WinUI 3 application requires the Windows App SDK runtime to be installed on your system.

## Installing the Windows App SDK Runtime

### Option 1: Download and Install Manually

1. Download the Windows App SDK Runtime from: https://aka.ms/windowsappsdk/1.5/stable/windowsappruntimeinstall-x64.exe
2. Run the installer
3. Restart your computer if prompted
4. Run the application again

### Option 2: Install via Winget (if available)

```powershell
winget install Microsoft.WindowsAppRuntime
```

### Option 3: Run as Packaged App (Recommended for Distribution)

To avoid requiring users to install the runtime separately, you can package the app as an MSIX:

1. Build the solution in Release mode
2. Use Visual Studio's "Create App Packages" option
3. This will create an MSIX package that includes the runtime dependencies

## Verifying Installation

After installing the runtime, verify it's installed by checking:
- Settings > Apps > Installed apps
- Look for "Windows App Runtime"

## Troubleshooting

If you still get the DLL error after installing the runtime:
1. Ensure you downloaded the x64 version (matching your app's platform)
2. Restart your computer
3. Try running the app again

