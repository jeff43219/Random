@echo off
setlocal enabledelayedexpansion

:: 1. Make sure we are in the right place and bin exists
if not exist "bin" mkdir "bin"

:: 2. Build video-compressor separately (multi-file + FFmpeg)
echo ---------------------------------
echo Compiling: video-compressor...
g++ -std=c++17 src/compress.cpp src/main_compressor.cpp -I C:/ffmpeg/include -I src/include -L C:/ffmpeg/lib -lavcodec -lavformat -lavutil -lavdevice -lswscale -lswresample -lpthread -o bin/compressor.exe
if !errorlevel! equ 0 (
    echo [SUCCESS] Created bin\compressor.exe
) else (
    echo [ERROR] Failed to compile video-compressor
)

:: 3. Loop through single-file .cpp files
for %%f in (src\*.cpp) do (
    if /i not "%%~nf"=="compress" (
        if /i not "%%~nf"=="main" (
            if /i not "%%~nf"=="main_compressor" (
                echo ---------------------------------
                echo Compiling: %%~nf...

                :: --- Custom flags per file ---
                if /i "%%~nf"=="alwaysontop" (
                    g++ "%%f" -mwindows -static -lgdi32 -o "bin\%%~nf.exe"
                ) else (
                    g++ "%%f" -I src/include -o "bin\%%~nf.exe"
                )

                if !errorlevel! equ 0 (
                    echo [SUCCESS] Created bin\%%~nf.exe
                ) else (
                    echo [ERROR] Failed to compile %%~nf
                )
            )
        )
    )
)

echo ---------------------------------
echo Build process finished.
timeout /t 3 > nul