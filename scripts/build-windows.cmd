@echo off
setlocal
cd /d "%~dp0.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.44
if errorlevel 1 exit /b 1
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_DEBUG=/Z7 -DCMAKE_PREFIX_PATH="%CD%\.deps\Qt\6.8.3\msvc2022_64" %*
if errorlevel 1 exit /b 1
cmake --build build
if errorlevel 1 exit /b 1
set "PATH=%CD%\.deps\Qt\6.8.3\msvc2022_64\bin;%PATH%"
ctest --test-dir build --output-on-failure
