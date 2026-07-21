@echo off
setlocal enabledelayedexpansion

:: Default MSYS2 installation path (can be overridden by environment variable)
if not defined MSYS2_ROOT set MSYS2_ROOT=C:\msys64

echo [ENV] Setting up MSYS2 UCRT64 environment...
echo [ENV] Using MSYS2_ROOT: %MSYS2_ROOT%

:: Verify MSYS2 installation
if not exist "%MSYS2_ROOT%\ucrt64\bin" (
    echo [ERROR] MSYS2 UCRT64 not found at: %MSYS2_ROOT%
    echo [ERROR] Please install MSYS2 or set MSYS2_ROOT environment variable
    pause
    exit /b 1
)

set PATH=%MSYS2_ROOT%\ucrt64\bin;%MSYS2_ROOT%\usr\bin;C:\Windows\system32;C:\Windows;C:\Windows\System32\WindowsPowerShell\v1.0\
set CC=
set CXX=
set MAKE=
set INCLUDE=
set LIB=

echo [CLEAN] Removing old build directory...
if exist "build" rd /s /q build

echo [CMAKE] Configuring project (Debug mode)...
cmake -G "Ninja" -B build -S . -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=%MSYS2_ROOT%/ucrt64/bin/g++.exe
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake configuration failed!
    pause
    exit /b %ERRORLEVEL%
)

echo [BUILD] Compiling...
cmake --build build
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Compilation failed!
    pause
    exit /b %ERRORLEVEL%
)

echo [DEPLOY] Preparing tessdata folder...
if not exist "build\tessdata" mkdir "build\tessdata"
xcopy /Y /Q "src\res\ocr\*.traineddata" "build\tessdata\"

echo [DEPLOY] Copying DLL dependencies (Tesseract/Leptonica)...
for %%F in (libtesseract-*.dll libleptonica-*.dll libwinpthread-1.dll libtiff-*.dll libpng*.dll libjpeg-*.dll libwebp-*.dll zlib*.dll liblzma-*.dll libstdc++-6.dll libgcc_s_seh-1.dll) do (
    if exist "%MSYS2_ROOT%\ucrt64\bin\%%F" (
        copy /Y "%MSYS2_ROOT%\ucrt64\bin\%%F" "build\" >nul
        echo [OK] %%F
    )
)

echo [DEPLOY] Setting locale...
copy /Y "src\res\locales\strings-eng.json" "build\strings.json"

echo [DONE] Project ready in build folder.
pause