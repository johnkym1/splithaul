@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cd /d "%~dp0"
if not exist build\shift-f9-ats mkdir build\shift-f9-ats
cl /nologo /std:c++17 /EHsc /utf-8 /W4 /WX /O2 /MT /LD /DGT_ENABLE_EXPERIMENTAL_CANCELLATION /I vendor\scs_sdk_1_16\include /external:I ..\dispatch_cpp\vendor /external:W0 live_dispatch.cpp /Fo:build\shift-f9-ats\live_dispatch.obj /link /OUT:build\shift-f9-ats\GT_Telemetry.dll /IMPLIB:build\shift-f9-ats\GT_Telemetry.lib /EXPORT:scs_telemetry_init /EXPORT:scs_telemetry_shutdown /INCREMENTAL:NO
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /WX /O2 /MT /I vendor\scs_sdk_1_16\include smoke.cpp /Fo:build\shift-f9-ats\smoke.obj /Fe:build\shift-f9-ats\smoke.exe /link /INCREMENTAL:NO
if errorlevel 1 exit /b 1
build\shift-f9-ats\smoke.exe "%~dp0build\shift-f9-ats\GT_Telemetry.dll"
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /WX /O2 /MT /I vendor\scs_sdk_1_16\include assignment_test.cpp /Fo:build\shift-f9-ats\assignment_test.obj /Fe:build\shift-f9-ats\assignment_test.exe /link /INCREMENTAL:NO
if errorlevel 1 exit /b 1
build\shift-f9-ats\assignment_test.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /utf-8 /W4 /WX /O2 /MT /I vendor\scs_sdk_1_16\include /external:I ..\dispatch_cpp\vendor /external:W0 cancel_integration_test.cpp /Fo:build\shift-f9-ats\cancel_test.obj /Fe:build\shift-f9-ats\cancel_test.exe /link bcrypt.lib user32.lib shell32.lib ole32.lib /INCREMENTAL:NO
if errorlevel 1 exit /b 1
build\shift-f9-ats\cancel_test.exe
exit /b %errorlevel%

