@echo off
setlocal enabledelayedexpansion

:GET_PATH
set /p "SOURCE_PATH=Please enter the source PATH: "

:: Remove quotes if the user dragged and dropped
set "SOURCE_PATH=%SOURCE_PATHSync:"=%"

:: Check if the path exists (works for both files and folders now)
if exist "%SOURCE_PATH%" (
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
echo Using 14 threads.
echo.

:: Note: If you are moving a single file, Robocopy treats the 
:: Destination as a directory. 
robocopy "%SOURCE_PATH%" "%DEST_PATH%" /E /MOVE /MT:14 /Z /W:5 /R:3

echo.
echo Operation complete.
pause