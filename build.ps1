param([string]$SdkPath = $env:ANDROID_HOME, [string]$NdkPath = $env:ANDROID_NDK_HOME)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
$cacheRoot = Join-Path $PSScriptRoot '.local'
$env:GRADLE_USER_HOME = Join-Path $cacheRoot 'gradle'
$env:ANDROID_USER_HOME = Join-Path $cacheRoot 'android'
$env:TEMP = Join-Path $cacheRoot 'tmp'
$env:TMP = $env:TEMP
New-Item -ItemType Directory -Force $env:TEMP, $env:ANDROID_USER_HOME | Out-Null
$env:GRADLE_OPTS = "-Djava.io.tmpdir=$($env:TEMP.Replace('\', '/'))"
if ($SdkPath) { $env:ANDROID_HOME = $SdkPath }
if ($NdkPath) { $env:ANDROID_NDK_HOME = $NdkPath }
& .\gradlew.bat --no-daemon assembleDebug
if ($LASTEXITCODE -ne 0) { throw "Gradle failed: $LASTEXITCODE" }
New-Item -ItemType Directory -Force 'dist' | Out-Null
$version = (Get-Content 'gradle.properties' | Where-Object { $_ -like 'moduleVersionName=*' }) -replace '^moduleVersionName=', ''
$target = "dist/hyos-back-fix-$version.apk"
Copy-Item 'app/build/outputs/apk/debug/app-debug.apk' $target -Force
Write-Output "APK: $PSScriptRoot/$target"

