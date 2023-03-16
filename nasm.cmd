@echo off
%~dp0bin\nasm\nasm.exe %*
exit /b %errorlevel%
