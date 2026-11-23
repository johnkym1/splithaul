@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cd /d "%~dp0"
if not exist build mkdir build
cl /nologo /std:c++17 /EHsc /utf-8 /W4 /WX /O2 /MT /LD /I vendor\scs_sdk_1_16\include /external:I ..\dispatch_cpp\vendor /external:W0 live_dispatch.cpp /Fo:build\live_dispatch.obj /link /OUT:build\GT_Telemetry.dll /EXPORT:scs_telemetry_init /EXPORT:scs_telemetry_shutdown /INCREMENTAL:NO
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /WX /O2 /MT /I vendor\scs_sdk_1_16\include smoke.cpp /Fo:build\smoke.obj /Fe:build\smoke.exe /link /INCREMENTAL:NO
if errorlevel 1 exit /b 1
build\smoke.exe "%~dp0build\GT_Telemetry.dll"
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /WX /O2 /MT /I vendor\scs_sdk_1_16\include assignment_test.cpp /Fo:build\assignment_test.obj /Fe:build\assignment_test.exe /link /INCREMENTAL:NO
if errorlevel 1 exit /b 1
build\assignment_test.exe
exit /b %errorlevel%

