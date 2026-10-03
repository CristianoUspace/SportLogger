@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Avvia_Studio.ps1"
if errorlevel 1 pause
