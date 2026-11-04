@echo off
setlocal
cd /d "%~dp0"
set "PATH=%CD%\.deps\Qt\6.8.3\msvc2022_64\bin;%PATH%"
if not exist "build\MedicalWand.exe" (
    echo Build first: scripts\build-windows.cmd -DMEDWAND_GUI=ON -DMEDWAND_VIEWER=ON
    pause
    exit /b 1
)
start "MedicalWand" "%CD%\build\MedicalWand.exe"
