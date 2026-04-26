@echo off
setlocal enabledelayedexpansion

:GET_PATH
set "SOURCE_PATH="
set /p "SOURCE_PATH=Please enter the source PATH: "
:: Clean up quotes
set "SOURCE_PATH=%SOURCE_PATH:"=%"

if exist "%SOURCE_PATH%" (
    echo Path verified.
) else (
    echo.
    echo ERROR: Path not found. Check the spelling or drag the file here.
    goto GET_PATH
)

:GET_DEST
set "DEST_PATH="
set /p "DEST_PATH=Please enter the destination PATH: "
:: Clean up quotes properly without typos
set "DEST_PATH=%DEST_PATH:"=%"

echo.
echo Starting copy operation...
echo From: "%SOURCE_PATH%"
echo To:   "%DEST_PATH%"
echo.

:: Check if source is a folder
if exist "%SOURCE_PATH%\" (
    robocopy "%SOURCE_PATH%" "%DEST_PATH%" /E /MT:14 /Z /W:5 /R:3
) else (
    :: Logic for a single file
    for %%i in ("%SOURCE_PATH%") do (
        robocopy "%%~dpi." "%DEST_PATH%" "%%~nxi" /MT:14 /Z /W:5 /R:3
    )
)

echo.
echo Done!
pause