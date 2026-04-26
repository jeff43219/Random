@echo off
setlocal enabledelayedexpansion

:: 1. CHECK FOR ARGUMENTS (%1 is source, %2 is destination)
set "SOURCE_PATH=%~1"
set "DEST_PATH=%~2"

:: 2. IF ARGUMENTS ARE MISSING, ASK MANUALLY
if "%SOURCE_PATH%"=="" (
    set /p "SOURCE_PATH=Please enter the source PATH: "
)
set "SOURCE_PATH=%SOURCE_PATH:"=%"

if not exist "%SOURCE_PATH%" (
    echo ERROR: Source path not found: "%SOURCE_PATH%"
    exit /b
)

if "%DEST_PATH%"=="" (
    set /p "DEST_PATH=Please enter the destination PATH: "
)
set "DEST_PATH=%DEST_PATH:"=%"

echo.
echo ----------------------------------------------------
echo Operation: Copying with 14 Threads
echo From: "%SOURCE_PATH%"
echo To:   "%DEST_PATH%"
echo ----------------------------------------------------

:: 3. EXECUTION LOGIC
if exist "%SOURCE_PATH%\" (
    :: It's a Folder
    robocopy "%SOURCE_PATH%" "%DEST_PATH%" /E /Z /MT:14 /R:3 /W:5 /XJ /ETA
) else (
    :: It's a Single File
    for %%i in ("%SOURCE_PATH%") do (
        set "FILE_DIR=%%~dpi"
        set "FILE_NAME=%%~nxi"
        robocopy "!FILE_DIR!." "%DEST_PATH%" "!FILE_NAME!" /Z /MT:14 /R:3 /W:5 /ETA
    )
)

echo.
echo Operation complete.