@echo off
REM Crash Test Runner Batch Script
REM Runs every crash scenario in CrashTestRunner.exe sequentially and logs the results.

setlocal enabledelayedexpansion

REM Locate the executable. Visual Studio multi-config builds put outputs in
REM build\Release (or build\Debug); single-config (Ninja, NMake) put them in
REM build\. Prefer Release if both are available.
set "EXECUTABLE_DIR=build"
if exist "build\Debug\CrashTestRunner.exe"   set "EXECUTABLE_DIR=build\Debug"
if exist "build\Release\CrashTestRunner.exe" set "EXECUTABLE_DIR=build\Release"

set "EXECUTABLE=CrashTestRunner.exe"
set "CRASH_DIR=test_crashFiles"
set "LOG_DIR=test_results"

REM Build a filename-safe timestamp (YYYYMMDD_HHMMSS).
for /f "tokens=2 delims==" %%I in ('wmic os get LocalDateTime /value ^| find "="') do set "_DT=%%I"
set "TIMESTAMP=!_DT:~0,8!_!_DT:~8,6!"

set "LOG_FILE=%LOG_DIR%\test_run_%TIMESTAMP%.log"

cd "%EXECUTABLE_DIR%"

if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"
if not exist "%CRASH_DIR%" mkdir "%CRASH_DIR%"

echo ========================================
echo CrashReport Library - Crash Test Suite
echo ========================================
echo.
echo Timestamp:  %TIMESTAMP%
echo Executable: %EXECUTABLE%
echo Log file:   %LOG_FILE%
echo.

if not exist "%EXECUTABLE%" (
    echo ERROR: Executable not found: %EXECUTABLE%
    echo Build the project first:
    echo   cmake -B build
    echo   cmake --build build --config Release
    echo.
    exit /b 1
)

REM Create log header.
> "%LOG_FILE%" echo CrashReport Library - Crash Test Suite
>> "%LOG_FILE%" echo Timestamp: %TIMESTAMP%
>> "%LOG_FILE%" echo =========================================
>> "%LOG_FILE%" echo.

set "TESTS=null divide stackoverflow exception abort purevirtual invalidparam accessviolation doubledelete bufferoverrun multithread nested"

set /a PASSED=0
set /a PARTIAL=0
set /a FAILED=0
set /a TOTAL=0

echo Running crash tests...
echo.

for %%T in (%TESTS%) do (
    set /a TOTAL+=1
    call :run_one %%T
)

echo.
echo ========================================
echo Test Summary
echo ========================================
echo Total:   %TOTAL%
echo Passed:  %PASSED%
echo Partial: %PARTIAL%
echo Failed:  %FAILED%
echo.
echo Log file:    %LOG_FILE%
echo Crash dumps: %CRASH_DIR%\
echo.

>> "%LOG_FILE%" echo.
>> "%LOG_FILE%" echo =========================================
>> "%LOG_FILE%" echo Test Summary
>> "%LOG_FILE%" echo =========================================
>> "%LOG_FILE%" echo Total:   %TOTAL%
>> "%LOG_FILE%" echo Passed:  %PASSED%
>> "%LOG_FILE%" echo Partial: %PARTIAL%
>> "%LOG_FILE%" echo Failed:  %FAILED%

if %FAILED% GTR 0 (
    exit /b 1
)
exit /b 0


REM ----------------------------------------------------------------------------
REM Subroutine: run_one <test_name>
REM Runs a single crash test, classifies the result, renames output files.
REM ----------------------------------------------------------------------------
:run_one
set "TEST_NAME=%~1"
set "DMP_FILE=%CRASH_DIR%\CrashTestRunner.exe.dmp"
set "STACK_FILE=%CRASH_DIR%\CrashTestRunner.exe_stack.txt"

REM Remove any leftover crash files from a previous run.
if exist "%DMP_FILE%"   del /F /Q "%DMP_FILE%"
if exist "%STACK_FILE%" del /F /Q "%STACK_FILE%"

REM Per-test output goes here so the main log isn't polluted with crash spew.
set "TEST_OUT=%LOG_DIR%\%TEST_NAME%_output.txt"

>> "%LOG_FILE%" echo ----------------------------------------
>> "%LOG_FILE%" echo TEST: %TEST_NAME%
>> "%LOG_FILE%" echo ----------------------------------------

REM Run with a 15-second watchdog so a hung test does not block the suite.
REM (Windows has no built-in process-timeout for child processes; use PowerShell.)
powershell -NoProfile -Command ^
  "$p = Start-Process -FilePath '.\%EXECUTABLE%' -ArgumentList '%TEST_NAME%' -PassThru -RedirectStandardOutput '%TEST_OUT%' -RedirectStandardError '%TEST_OUT%.err' -NoNewWindow;" ^
  "if (-not $p.WaitForExit(15000)) { $p.Kill(); exit 124 } else { exit $p.ExitCode }"
set "EXITCODE=!ERRORLEVEL!"
if exist "%TEST_OUT%.err" (
    type "%TEST_OUT%.err" >> "%TEST_OUT%"
    del /F /Q "%TEST_OUT%.err" >nul 2>&1
)

type "%TEST_OUT%" >> "%LOG_FILE%"
>> "%LOG_FILE%" echo Exit code: !EXITCODE!

REM Classify result based on which output files were produced.
set "HAS_DMP=0"
set "HAS_STACK=0"
if exist "%DMP_FILE%"   set "HAS_DMP=1"
if exist "%STACK_FILE%" set "HAS_STACK=1"

if "!HAS_DMP!"=="1" if "!HAS_STACK!"=="1" goto :run_one_pass
if "!HAS_DMP!"=="1"                       goto :run_one_partial
if "!HAS_STACK!"=="1"                     goto :run_one_partial
goto :run_one_fail

:run_one_pass
echo   [%TEST_NAME%] PASS - dump + stack trace generated
>> "%LOG_FILE%" echo Result: PASS - dump + stack trace
set /a PASSED+=1
goto :run_one_archive

:run_one_partial
echo   [%TEST_NAME%] PARTIAL - only one output file generated
>> "%LOG_FILE%" echo Result: PARTIAL - dump=!HAS_DMP! stack=!HAS_STACK!
set /a PARTIAL+=1
goto :run_one_archive

:run_one_fail
echo   [%TEST_NAME%] FAIL - no crash output produced
>> "%LOG_FILE%" echo Result: FAIL - no output files
set /a FAILED+=1
goto :run_one_done

:run_one_archive
REM Rename the produced files so each test keeps its own copy.
if exist "%DMP_FILE%"   move /Y "%DMP_FILE%"   "%CRASH_DIR%\%TEST_NAME%.dmp"      >nul 2>&1
if exist "%STACK_FILE%" move /Y "%STACK_FILE%" "%CRASH_DIR%\%TEST_NAME%_stack.txt" >nul 2>&1

:run_one_done
>> "%LOG_FILE%" echo.
exit /b 0
