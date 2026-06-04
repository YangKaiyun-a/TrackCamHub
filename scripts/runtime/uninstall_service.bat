@echo off
setlocal

set "SERVICE_NAME=TrackCamHub"

sc query "%SERVICE_NAME%" >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo Service "%SERVICE_NAME%" does not exist.
    pause
    exit /b 0
)

net stop "%SERVICE_NAME%" >nul 2>nul
sc delete "%SERVICE_NAME%"

echo.
echo Service removed.
pause
