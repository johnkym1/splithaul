@echo off
call "%~dp0build_common.cmd" ets2-app
exit /b %errorlevel%
