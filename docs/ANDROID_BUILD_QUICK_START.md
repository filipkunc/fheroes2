# Android Build Quick Start Guide

This guide provides quick steps to build and deploy fheroes2 to an Android device from Windows.

## Prerequisites

- Android SDK with NDK installed
- ADB (Android Debug Bridge) in PATH
- USB debugging enabled on your Android device

## Build Steps

### 1. Install Dependencies

Run the package installer script to download SDL2 and other required libraries:

```powershell
& "c:\Projects\fheroes2\script\android\install_packages.bat"
```

### 2. Build Debug APK

Navigate to the android directory and run Gradle:

```powershell
Set-Location c:\Projects\fheroes2\android
.\gradlew.bat assembleDebug
```

The APK will be generated at:
```
android\app\build\outputs\apk\debug\app-debug.apk
```

### 3. Build Release APK (Optional)

For a release build, you need to configure signing. Set these environment variables:

```powershell
$env:FHEROES2_KEYSTORE = "path\to\your\keystore.jks"
$env:FHEROES2_KEYSTORE_PASSWORD = "your_keystore_password"
$env:FHEROES2_KEY_ALIAS = "your_key_alias"
$env:FHEROES2_KEY_PASSWORD = "your_key_password"
```

Then build:

```powershell
.\gradlew.bat assembleRelease
```

## ADB Deployment

### Check Connected Devices

```powershell
adb devices
```

If your device shows as `unauthorized`, check your phone for a USB debugging authorization prompt and tap **Allow**.

### Install APK

```powershell
adb install "C:\Projects\fheroes2\android\app\build\outputs\apk\debug\app-debug.apk"
```

### Reinstall (If Signature Mismatch)

If you get `INSTALL_FAILED_UPDATE_INCOMPATIBLE`, uninstall the existing app first:

```powershell
adb uninstall org.fheroes2
adb install "C:\Projects\fheroes2\android\app\build\outputs\apk\debug\app-debug.apk"
```

### View Logs

To view app logs in real-time:

```powershell
adb logcat -s fheroes2
```

## Notes

- Debug builds are automatically signed with a debug keystore
- The app requires Heroes of Might and Magic II game data files to run
- First launch may take a moment to initialize
