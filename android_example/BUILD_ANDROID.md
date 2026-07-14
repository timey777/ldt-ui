# Android Build

## Prerequisites

- Android SDK & NDK installed
- Java JDK 11+
- Gradle 8.x

## Environment Setup

Set the following environment variables according to your local installation:

```powershell
# Windows (PowerShell)
$env:JAVA_HOME="<YOUR_JAVA_HOME>"
$env:ANDROID_HOME="<YOUR_ANDROID_SDK_PATH>"
$env:ANDROID_NDK_HOME="<YOUR_ANDROID_NDK_PATH>"
```

```bash
# Linux / macOS
export JAVA_HOME=<YOUR_JAVA_HOME>
export ANDROID_HOME=<YOUR_ANDROID_SDK_PATH>
export ANDROID_NDK_HOME=<YOUR_ANDROID_NDK_PATH>
```

## Build Debug APK

```bash
gradle assembleDebug
# or on Windows:
gradle.bat assembleDebug
```

## Output

```text
android_example/app/build/outputs/apk/debug/app-debug.apk
```
