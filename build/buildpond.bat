@echo off
setlocal

:: --- Configuration ---
set OUTPUT=airmouse.exe
set SOURCES=../src/program/Window/mainWin.c ../src/program/Window/win_mouse.c ../src/program/Window/serial_win.c ../src/program/Window/localip_win.c

:: --- Compiler & Flags ---
set CC=gcc
set CFLAGS=-Wall -std=c99 -O2 -I"../src/program/Window"
set LDFLAGS=-lraylib -lws2_32 -lopengl32 -lgdi32 -lwinmm

echo ===================================
echo Compiling %SOURCES%...
echo ===================================

%CC% %CFLAGS% %SOURCES% -o %OUTPUT% %LDFLAGS%

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