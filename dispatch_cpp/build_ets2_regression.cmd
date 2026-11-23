@echo off
call "%~dp0build_common.cmd" regression
exit /b %errorlevel%
