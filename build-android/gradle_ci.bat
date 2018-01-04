@echo off

REM
REM Copyright 2016 The Android Open Source Project
REM Copyright (C) 2015 Valve Corporation
REM
REM Licensed under the Apache License, Version 2.0 (the "License");
REM you may not use this file except in compliance with the License.
REM You may obtain a copy of the License at
REM
REM      http://www.apache.org/licenses/LICENSE-2.0
REM
REM Unless required by applicable law or agreed to in writing, software
REM distributed under the License is distributed on an "AS IS" BASIS,
REM WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
REM See the License for the specific language governing permissions and
REM limitations under the License.
REM

set ANDROID_HOME=c:/Android/sdk
set ANDROID_NDK_HOME=c:/Android/android-ndk-r16b
set PATH=%ANDROID_NDK_HOME%;%PATH%

setlocal enabledelayedexpansion

call update_external_sources_android.bat
call android-generate.bat

cd cmake\test
call gradlew.bat clean assembleDebug

if not exist app\build\outputs\apk\debug\app-debug.apk (
   echo.
   echo "***** ERROR: Validation Layers for Android build failed *****"
   if %ERRORLEVEL% NEQ 0 (
      set ERRORLEVEL=-1
   )
)
endlocal

