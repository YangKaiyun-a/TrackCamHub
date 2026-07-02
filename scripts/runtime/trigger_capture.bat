@echo off
setlocal
cd /d "%~dp0"

set "TRIGGER_HOST=127.0.0.1"
set "TRIGGER_PORT=7090"

if not "%~1"=="" set "TRIGGER_HOST=%~1"
if not "%~2"=="" set "TRIGGER_PORT=%~2"

"%~dp0TriggerCapture.exe" --host "%TRIGGER_HOST%" --port %TRIGGER_PORT% --message CAPTURE
pause
