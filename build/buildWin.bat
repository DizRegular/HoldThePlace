

@echo off
setlocal

:: --- Configuration ---
set RAYLIB_PATH=C:\raylib\raylib\src
set OUTPUT=airmouse.exe
set SOURCES=../src/program/Window/mainWin.c ../src/program/Window/win_mouse.c

:: --- Compiler & Flags ---
set CC=gcc
set CFLAGS=-Wall -std=c99 -O2 -I"%RAYLIB_PATH%"
set LDFLAGS=-L"%RAYLIB_PATH%" -lraylib -lws2_32 -lopengl32 -lgdi32 -lwinmm

echo ===================================
echo Compiling %SOURCES%...
echo ===================================

%CC% %SOURCES% -o %OUTPUT% %CFLAGS% %LDFLAGS%

if %ERRORLEVEL% equ 0 (
    echo.
    echo Build successful: %OUTPUT%
    echo Launching program...
    echo ===================================
    %OUTPUT%
) else (
    echo.
    echo ===================================
    echo Build failed with error code %ERRORLEVEL%.
    echo ===================================
    pause
)

endlocal