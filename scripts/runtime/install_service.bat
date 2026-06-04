@echo off
setlocal

set "SERVICE_NAME=TrackCamHub"
set "DISPLAY_NAME=TrackCamHub"
set "BASE_DIR=%~dp0"
set "EXE=%BASE_DIR%TrackCamHub.exe"
set "CONFIG=%BASE_DIR%config\trackcamhub.ini"

if not exist "%EXE%" (
    echo TrackCamHub.exe not found: "%EXE%"
    pause
    exit /b 1
)

if not exist "%CONFIG%" (
    echo Config file not found: "%CONFIG%"
    pause
    exit /b 1
)

sc query "%SERVICE_NAME%" >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    echo Service "%SERVICE_NAME%" already exists. Stop and delete it first if you want to reinstall.
    pause
    exit /b 1
)

sc create "%SERVICE_NAME%" binPath= "\"%EXE%\" --service \"%CONFIG%\"" start= auto DisplayName= "%DISPLAY_NAME%"
if %ERRORLEVEL% NEQ 0 (
    echo Failed to create service. Please run this script as Administrator.
    pause
    exit /b 1
)

sc failure "%SERVICE_NAME%" reset= 60 actions= restart/5000/restart/5000/""/0
sc description "%SERVICE_NAME%" "TrackCamHub Windows-side serial and camera Thrift coordinator."
net start "%SERVICE_NAME%"

echo.
echo Service installed and started.
echo Logs: "%BASE_DIR%logs\TrackCamHub.log"
pause
