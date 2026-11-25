@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cd /d "%~dp0"
if not exist build\ets2 mkdir build\ets2
cl /nologo /std:c++17 /DGT_TARGET_ETS2 /EHsc /utf-8 /W4 /WX /O2 /MT /LD /DGT_ENABLE_EXPERIMENTAL_CANCELLATION /I vendor\scs_sdk_1_16\include /external:I ..\dispatch_cpp\vendor /external:W0 live_dispatch.cpp /Fo:build\ets2\live_dispatch.obj /link /OUT:build\ets2\GT_Telemetry.dll /IMPLIB:build\ets2\GT_Telemetry.lib /EXPORT:scs_telemetry_init /EXPORT:scs_telemetry_shutdown /INCREMENTAL:NO
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /DGT_TARGET_ETS2 /EHsc /W4 /WX /O2 /MT /I vendor\scs_sdk_1_16\include smoke.cpp /Fo:build\ets2\smoke.obj /Fe:build\ets2\smoke.exe /link /INCREMENTAL:NO
if errorlevel 1 exit /b 1
build\ets2\smoke.exe "%~dp0build\ets2\GT_Telemetry.dll"
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /DGT_TARGET_ETS2 /EHsc /W4 /WX /O2 /MT /I vendor\scs_sdk_1_16\include assignment_test.cpp /Fo:build\ets2\assignment_test.obj /Fe:build\ets2\assignment_test.exe /link /INCREMENTAL:NO
if errorlevel 1 exit /b 1
build\ets2\assignment_test.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /DGT_TARGET_ETS2 /EHsc /utf-8 /W4 /WX /O2 /MT /external:I ..\dispatch_cpp\vendor /external:W0 ets2_target_test.cpp /Fo:build\ets2\ets2_target_test.obj /Fe:build\ets2\ets2_target_test.exe /link bcrypt.lib /INCREMENTAL:NO
if errorlevel 1 exit /b 1
build\ets2\ets2_target_test.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /DGT_TARGET_ETS2 /EHsc /utf-8 /W4 /WX /O2 /MT /I vendor\scs_sdk_1_16\include /external:I ..\dispatch_cpp\vendor /external:W0 cancel_integration_test.cpp /Fo:build\ets2\cancel_test.obj /Fe:build\ets2\cancel_test.exe /link bcrypt.lib user32.lib shell32.lib ole32.lib /INCREMENTAL:NO
if errorlevel 1 exit /b 1
build\ets2\cancel_test.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /DGT_TARGET_ETS2 /EHsc /utf-8 /W4 /WX /O2 /MT /I vendor\scs_sdk_1_16\include /external:I ..\dispatch_cpp\vendor /external:W0 trailer_combination_test.cpp /Fo:build\ets2\trailer_test.obj /Fe:build\ets2\trailer_test.exe /link bcrypt.lib user32.lib shell32.lib ole32.lib /INCREMENTAL:NO
if errorlevel 1 exit /b 1
build\ets2\trailer_test.exe
exit /b %errorlevel%

