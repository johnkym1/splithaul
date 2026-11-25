@echo off
setlocal
rem All app targets use the current multi-game configuration and isolated outputs.
set "buildOutput="
set "targetDefine=/DGT_MULTI_GAME_APP"
if "%~1"=="multi" set "buildOutput=build"
if "%~1"=="ets2-app" set "buildOutput=build\ets2-app"
if "%~1"=="regression" set "buildOutput=build\ets2-regression"
if not defined buildOutput (
    echo Usage: build_common.cmd multi ^| ets2-app ^| regression
    exit /b 2
)
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cd /d "%~dp0"
if not exist "%buildOutput%" mkdir "%buildOutput%"
if errorlevel 1 exit /b 1
set "compilerFlags=/nologo /std:c++17 %targetDefine% /EHsc /utf-8 /W4 /WX /O2 /MT /DUNICODE /D_UNICODE /D_WIN32_WINNT=0x0A00 /external:I vendor /external:W0"
set "consoleFlags=/nologo /std:c++17 %targetDefine% /EHsc /utf-8 /W4 /WX /O2 /MT /external:I vendor /external:W0"
cl %compilerFlags% /c dispatch_core.cpp /Fo:%buildOutput%\dispatch_core.obj
if errorlevel 1 exit /b 1
cl %compilerFlags% /c ats_catalog.cpp job_history.cpp live_panel.cpp history_page.cpp /Fo:%buildOutput%\
if errorlevel 1 exit /b 1
rc /nologo /fo %buildOutput%\resources.res resources.rc
if errorlevel 1 exit /b 1
cl %compilerFlags% main.cpp %buildOutput%\dispatch_core.obj %buildOutput%\ats_catalog.obj %buildOutput%\job_history.obj %buildOutput%\live_panel.obj %buildOutput%\history_page.obj %buildOutput%\resources.res /Fo:%buildOutput%\main.obj /Fe:%buildOutput%\DispatchPlanner.exe /link user32.lib gdi32.lib comctl32.lib shell32.lib bcrypt.lib gdiplus.lib ole32.lib dwmapi.lib uxtheme.lib uuid.lib /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTINPUT:dispatch.manifest /INCREMENTAL:NO
if errorlevel 1 exit /b 1
cl %compilerFlags% tests.cpp %buildOutput%\dispatch_core.obj /Fo:%buildOutput%\tests.obj /Fe:%buildOutput%\dispatch_tests.exe /link /INCREMENTAL:NO
if errorlevel 1 exit /b 1
%buildOutput%\dispatch_tests.exe "%~dp0.." "%~dp0%buildOutput%\test-data"
if errorlevel 1 exit /b 1
cl %compilerFlags% /I ..\native_job_bridge\vendor\scs_sdk_1_16\include live_tests.cpp %buildOutput%\dispatch_core.obj %buildOutput%\ats_catalog.obj %buildOutput%\job_history.obj /Fo:%buildOutput%\live_tests.obj /Fe:%buildOutput%\live_tests.exe /link bcrypt.lib /INCREMENTAL:NO
if errorlevel 1 exit /b 1
%buildOutput%\live_tests.exe "%~dp0.." "%~dp0%buildOutput%\live-test-data"
if errorlevel 1 exit /b 1
cl %compilerFlags% selection_tests.cpp %buildOutput%\dispatch_core.obj %buildOutput%\ats_catalog.obj %buildOutput%\job_history.obj %buildOutput%\live_panel.obj /Fo:%buildOutput%\selection_tests.obj /Fe:%buildOutput%\selection_tests.exe /link user32.lib gdi32.lib comctl32.lib shell32.lib bcrypt.lib ole32.lib /INCREMENTAL:NO
if errorlevel 1 exit /b 1
%buildOutput%\selection_tests.exe "%~dp0.."
if errorlevel 1 exit /b 1
cl %compilerFlags% profile_manager_tests.cpp /Fo:%buildOutput%\profile_manager_tests.obj /Fe:%buildOutput%\profile_manager_tests.exe /link user32.lib comctl32.lib bcrypt.lib shell32.lib ole32.lib /INCREMENTAL:NO
if errorlevel 1 exit /b 1
%buildOutput%\profile_manager_tests.exe "%~dp0%buildOutput%\profile-manager-data"
if errorlevel 1 exit /b 1
cl %compilerFlags% update_check_tests.cpp /Fo:%buildOutput%\update_check_tests.obj /Fe:%buildOutput%\update_check_tests.exe /link /INCREMENTAL:NO
if errorlevel 1 exit /b 1
%buildOutput%\update_check_tests.exe
if errorlevel 1 exit /b 1
cl %compilerFlags% history_tests.cpp %buildOutput%\dispatch_core.obj %buildOutput%\ats_catalog.obj %buildOutput%\job_history.obj /Fo:%buildOutput%\history_tests.obj /Fe:%buildOutput%\history_tests.exe /link bcrypt.lib /INCREMENTAL:NO
if errorlevel 1 exit /b 1
%buildOutput%\history_tests.exe "%~dp0.." "%~dp0%buildOutput%\history-test-data"
if errorlevel 1 exit /b 1
cl %compilerFlags% history_browse_tests.cpp %buildOutput%\dispatch_core.obj %buildOutput%\ats_catalog.obj %buildOutput%\job_history.obj %buildOutput%\history_page.obj /Fo:%buildOutput%\history_browse_tests.obj /Fe:%buildOutput%\history_browse_tests.exe /link user32.lib gdi32.lib comctl32.lib shell32.lib bcrypt.lib gdiplus.lib ole32.lib dwmapi.lib uxtheme.lib uuid.lib /INCREMENTAL:NO
if errorlevel 1 exit /b 1
%buildOutput%\history_browse_tests.exe "%~dp0%buildOutput%\history-browse-data"
if errorlevel 1 exit /b 1
cl %compilerFlags% history_export.cpp %buildOutput%\dispatch_core.obj %buildOutput%\ats_catalog.obj %buildOutput%\job_history.obj /Fo:%buildOutput%\history_export.obj /Fe:%buildOutput%\HistoryExport.exe /link bcrypt.lib /INCREMENTAL:NO
if errorlevel 1 exit /b 1
cl %consoleFlags% dual_game_tests.cpp %buildOutput%\dispatch_core.obj %buildOutput%\ats_catalog.obj %buildOutput%\job_history.obj /Fo:%buildOutput%\dual_game_tests.obj /Fe:%buildOutput%\dual_game_tests.exe /link bcrypt.lib /INCREMENTAL:NO
if errorlevel 1 exit /b 1
%buildOutput%\dual_game_tests.exe "%~dp0.." "%~dp0%buildOutput%\dual-game-data"
exit /b %errorlevel%

