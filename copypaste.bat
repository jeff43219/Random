@REM ROBOCOPY SCRIPT
@echo off
setlocal enabledelayedexpansion

:GET_PATH
set /p "SOURCE_PATH=Please enter the source PATH: "

:: Remove quotes if the user dragged and dropped the folder
set "SOURCE_PATH=%SOURCE_PATH:"=%"

if exist "%SOURCE_PATH%\" (
    echo Path verified: "%SOURCE_PATH%"
) else (
    echo.
    echo ERROR: The path "%SOURCE_PATH%" does not exist.
    echo Please try again.
    echo.
    goto GET_PATH
)

set /p "DEST_PATH=Please enter the destination PATH: "
set "DEST_PATH=%DEST_PATH:"=%"

echo.
echo Starting move operation...
echo Using 14 threads on your Ryzen 7 processor.
echo.

:: /E      : Copies subdirectories, including empty ones.
:: /MOVE   : Moves files and dirs (deletes from source after copying).
:: /MT:14  : Uses 14 threads as requested.
:: /Z      : Restartable mode (safe for large transfers).
:: /W:5 /R:3 : Wait 5 seconds and retry 3 times on failed files.

robocopy "%SOURCE_PATH%" "%DEST_PATH%" /E /MOVE /MT:14 /Z /W:5 /R:3

echo.
echo Operation complete.
pause