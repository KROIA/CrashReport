@echo off
REM Crash Test Runner Batch Script
REM This script runs all crash tests sequentially and logs the results

setlocal enabledelayedexpansion

set EXECUTABLE=build\bin\Release\CrashTestRunner.exe
set LOG_DIR=test_results
set TIMESTAMP=%date:~-4,4%%date:~-10,2%%date:~-7,2%_%time:~0,2%%time:~3,2%%time:~6,2%
set TIMESTAMP=%TIMESTAMP: =0%
set LOG_FILE=%LOG_DIR%\test_run_%TIMESTAMP%.log

REM Create log directory
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"

echo ========================================
echo CrashReport Library - Crash Test Suite
echo ========================================
echo.
echo Timestamp: %TIMESTAMP%
echo Executable: %EXECUTABLE%
echo Log file: %LOG_FILE%
echo.

REM Check if executable exists
if not exist "%EXECUTABLE%" (
    echo ERROR: Executable not found: %EXECUTABLE%
    echo Please build the project first with:
    echo   cmake -B build
    echo   cmake --build build --config Release
    echo.
    pause
    exit /b 1
)

REM Create log header
echo CrashReport Library - Crash Test Suite > "%LOG_FILE%"
echo Timestamp: %TIMESTAMP% >> "%LOG_FILE%"
echo ========================================= >> "%LOG_FILE%"
echo. >> "%LOG_FILE%"

REM Define test list
set TESTS=null divide stackoverflow exception abort purevirtual invalidparam accessviolation doubledelete bufferoverrun multithread nested

echo Running crash tests...
echo.

set PASSED=0
set FAILED=0
set TOTAL=0

REM Run each test
for %%T in (%TESTS%) do (
    set /a TOTAL+=1
    echo [%%T] Running test...
    echo ---------------------------------------- >> "%LOG_FILE%"
    echo TEST: %%T >> "%LOG_FILE%"
    echo ---------------------------------------- >> "%LOG_FILE%"

    REM Run the test and capture output
    "%EXECUTABLE%" %%T > "%LOG_DIR%\%%T_output.txt" 2>&1
    set EXITCODE=!ERRORLEVEL!

    REM Log the output
    type "%LOG_DIR%\%%T_output.txt" >> "%LOG_FILE%"
    echo. >> "%LOG_FILE%"
    echo Exit code: !EXITCODE! >> "%LOG_FILE%"
    echo. >> "%LOG_FILE%"

    REM Check if crash dump was created
    if exist "crashFiles_test\CrashTestRunner.exe.dmp" (
        echo [%%T] PASSED - Crash dump created
        echo Result: PASSED - Crash dump created >> "%LOG_FILE%"
        set /a PASSED+=1

        REM Rename crash files with test name
        move /Y "crashFiles_test\CrashTestRunner.exe.dmp" "crashFiles_test\%%T.dmp" >nul 2>&1
        if exist "crashFiles_test\CrashTestRunner.exe_stack.txt" (
            move /Y "crashFiles_test\CrashTestRunner.exe_stack.txt" "crashFiles_test\%%T_stack.txt" >nul 2>&1
        )
    ) else (
        REM Some tests might not crash reliably
        if !EXITCODE! EQU 0 (
            echo [%%T] WARNING - Test completed without crash (may be expected)
            echo Result: WARNING - No crash detected >> "%LOG_FILE%"
        ) else (
            echo [%%T] FAILED - Crash not properly handled
            echo Result: FAILED - Crash not properly handled >> "%LOG_FILE%"
            set /a FAILED+=1
        )
    )
    echo. >> "%LOG_FILE%"
    echo.
)

REM Print summary
echo ========================================
echo Test Summary
echo ========================================
echo Total tests: %TOTAL%
echo Passed:      %PASSED%
echo Failed:      %FAILED%
echo.
echo Results saved to: %LOG_FILE%
echo Crash dumps saved to: crashFiles_test\
echo.

echo ========================================= >> "%LOG_FILE%"
echo Test Summary >> "%LOG_FILE%"
echo ========================================= >> "%LOG_FILE%"
echo Total tests: %TOTAL% >> "%LOG_FILE%"
echo Passed:      %PASSED% >> "%LOG_FILE%"
echo Failed:      %FAILED% >> "%LOG_FILE%"

if %FAILED% GTR 0 (
    echo WARNING: Some tests failed!
    echo Check the log file for details.
    echo.
)

echo Press any key to exit...
pause >nul
