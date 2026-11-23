@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cd /d "%~dp0..\dispatch_cpp"
if not exist ..\installer\build mkdir ..\installer\build
cl /nologo /std:c++17 /DGT_MULTI_GAME_APP /DUNICODE /D_UNICODE /EHsc /utf-8 /W4 /WX /O2 /MT /external:I vendor /external:W0 catalog_pack.cpp build\dispatch_core.obj build\ats_catalog.obj /Fo:..\installer\build\catalog_pack.obj /Fe:..\installer\build\CatalogPack.exe /link bcrypt.lib /INCREMENTAL:NO
exit /b %errorlevel%
