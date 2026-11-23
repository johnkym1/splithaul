@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%~dp0"
cl /nologo /std:c++17 /DGT_MULTI_GAME_APP /EHsc /utf-8 /W4 /WX /O2 /MT /external:I vendor /external:W0 dual_game_tests.cpp build\ets2-app\dispatch_core.obj build\ets2-app\ats_catalog.obj build\ets2-app\job_history.obj /Fo:build\ets2-app\dual_game_tests.obj /Fe:build\ets2-app\dual_game_tests.exe /link bcrypt.lib /INCREMENTAL:NO
if errorlevel 1 exit /b 1
build\ets2-app\dual_game_tests.exe "%~dp0.." "%~dp0build\ets2-app\dual-game-data"
exit /b %errorlevel%
