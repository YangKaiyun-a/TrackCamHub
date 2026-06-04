@echo off
setlocal

set "REPO_ROOT=%~dp0"
set "VCVARS="

if exist "D:\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VCVARS=D:\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
)

if not defined VCVARS (
    echo Cannot find Visual Studio 2022 vcvarsall.bat.
    echo Please edit package.bat and set VCVARS to your Visual Studio path.
    goto :fail
)

echo Using Visual Studio environment:
echo   %VCVARS%
call "%VCVARS%" amd64
if errorlevel 1 goto :fail

cd /d "%REPO_ROOT%"
powershell -NoProfile -ExecutionPolicy Bypass -File "%REPO_ROOT%scripts\package.ps1"
if errorlevel 1 goto :fail

echo.
echo Package completed:
echo   %REPO_ROOT%dist\TrackCamHub
goto :done

:fail
echo.
echo Package failed.
if /I not "%~1"=="--no-pause" pause
exit /b 1

:done
if /I not "%~1"=="--no-pause" pause
exit /b 0
