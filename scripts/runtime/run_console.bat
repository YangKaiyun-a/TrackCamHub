@echo off
setlocal
cd /d "%~dp0"
"%~dp0TrackCamHub.exe" --console "%~dp0config\trackcamhub.ini"
pause
