@echo off
setlocal enabledelayedexpansion

:: 1. Make sure we are in the right place and bin exists
if not exist "bin" mkdir "bin"

:: 2. Loop through every .cpp file in the src folder
:: Note: In a .bat file, you MUST use double %% for variables
for %%f in (src\*.cpp) do (
    echo ---------------------------------
    echo Compiling: %%~nf...
    
    :: The command uses the full path to the file and outputs to bin
    g++ "%%f" -I src/include -o "bin\%%~nf.exe"
    
    if !errorlevel! equ 0 (
        echo [SUCCESS] Created bin\%%~nf.exe
    ) else (
        echo [ERROR] Failed to compile %%~nf
    )
)

echo ---------------------------------
echo Build process finished.
timeout /t 3 > nul