@echo off
setlocal

call "%~dp0compile.bat"
exit /b %ERRORLEVEL%
