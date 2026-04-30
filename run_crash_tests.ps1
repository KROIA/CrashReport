# Crash Test Runner PowerShell Script
# This script runs all crash tests sequentially and logs the results

param(
    [string]$Config = "Release",
    [string]$BuildDir = "build"
)

$ErrorActionPreference = "Continue"

# Configuration
$Executable = Join-Path $BuildDir "bin\$Config\CrashTestRunner.exe"
$LogDir = "test_results"
$Timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$LogFile = Join-Path $LogDir "test_run_$Timestamp.log"
$CrashDir = "crashFiles_test"

# ANSI color codes
$Green = "`e[32m"
$Red = "`e[31m"
$Yellow = "`e[33m"
$Blue = "`e[34m"
$Reset = "`e[0m"

function Write-ColorOutput {
    param([string]$Message, [string]$Color = $Reset)
    Write-Host "$Color$Message$Reset"
}

function Write-Header {
    param([string]$Text)
    Write-ColorOutput "========================================" $Blue
    Write-ColorOutput $Text $Blue
    Write-ColorOutput "========================================" $Blue
    Write-Output ""
}

# Create directories
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
New-Item -ItemType Directory -Force -Path $CrashDir | Out-Null

Write-Header "CrashReport Library - Crash Test Suite"
Write-Host "Timestamp:    $Timestamp"
Write-Host "Executable:   $Executable"
Write-Host "Log file:     $LogFile"
Write-Host "Crash dumps:  $CrashDir"
Write-Host ""

# Check if executable exists
if (-not (Test-Path $Executable)) {
    Write-ColorOutput "ERROR: Executable not found: $Executable" $Red
    Write-Host ""
    Write-Host "Please build the project first with:"
    Write-Host "  cmake -B $BuildDir"
    Write-Host "  cmake --build $BuildDir --config $Config"
    Write-Host ""
    exit 1
}

# Initialize log file
@"
CrashReport Library - Crash Test Suite
Timestamp: $Timestamp
Executable: $Executable
=========================================

"@ | Out-File -FilePath $LogFile -Encoding UTF8

# Define test list
$Tests = @(
    "null",
    "divide",
    "stackoverflow",
    "exception",
    "abort",
    "purevirtual",
    "invalidparam",
    "accessviolation",
    "doubledelete",
    "bufferoverrun",
    "multithread",
    "nested"
)

$Results = @()
$Passed = 0
$Failed = 0
$Warning = 0

Write-ColorOutput "Running crash tests..." $Blue
Write-Host ""

foreach ($Test in $Tests) {
    $TestNum = $Tests.IndexOf($Test) + 1
    Write-Host "[$TestNum/$($Tests.Count)] Testing: " -NoNewline
    Write-ColorOutput $Test $Yellow

    # Log test header
    @"
----------------------------------------
TEST: $Test
----------------------------------------
"@ | Out-File -FilePath $LogFile -Append -Encoding UTF8

    # Run the test
    $OutputFile = Join-Path $LogDir "$Test`_output.txt"
    $DumpFile = Join-Path $CrashDir "CrashTestRunner.exe.dmp"
    $StackFile = Join-Path $CrashDir "CrashTestRunner.exe_stack.txt"
    $RenamedDump = Join-Path $CrashDir "$Test.dmp"
    $RenamedStack = Join-Path $CrashDir "$Test`_stack.txt"

    # Delete old crash files for this test
    Remove-Item -Path $RenamedDump -ErrorAction SilentlyContinue
    Remove-Item -Path $RenamedStack -ErrorAction SilentlyContinue

    try {
        $Process = Start-Process -FilePath $Executable -ArgumentList $Test `
            -Wait -PassThru -NoNewWindow `
            -RedirectStandardOutput $OutputFile `
            -RedirectStandardError "$OutputFile.err"

        $ExitCode = $Process.ExitCode

        # Combine stdout and stderr
        $Output = Get-Content $OutputFile -ErrorAction SilentlyContinue
        $ErrorOutput = Get-Content "$OutputFile.err" -ErrorAction SilentlyContinue

        "$Output`n$ErrorOutput" | Out-File -FilePath $LogFile -Append -Encoding UTF8
        "Exit code: $ExitCode`n" | Out-File -FilePath $LogFile -Append -Encoding UTF8

        # Check if crash dump was created
        if (Test-Path $DumpFile) {
            Write-Host "    " -NoNewline
            Write-ColorOutput "✓ PASSED - Crash dump created" $Green
            "Result: PASSED - Crash dump created`n" | Out-File -FilePath $LogFile -Append -Encoding UTF8
            $Passed++

            # Rename crash files
            Move-Item -Path $DumpFile -Destination $RenamedDump -Force
            if (Test-Path $StackFile) {
                Move-Item -Path $StackFile -Destination $RenamedStack -Force
            }

            $Results += [PSCustomObject]@{
                Test = $Test
                Status = "PASSED"
                Message = "Crash dump created"
            }
        }
        elseif ($ExitCode -eq 0) {
            Write-Host "    " -NoNewline
            Write-ColorOutput "⚠ WARNING - Test completed without crash" $Yellow
            "Result: WARNING - No crash detected (may be expected)`n" | Out-File -FilePath $LogFile -Append -Encoding UTF8
            $Warning++

            $Results += [PSCustomObject]@{
                Test = $Test
                Status = "WARNING"
                Message = "No crash detected"
            }
        }
        else {
            Write-Host "    " -NoNewline
            Write-ColorOutput "✗ FAILED - Crash not properly handled" $Red
            "Result: FAILED - Exit code $ExitCode but no dump created`n" | Out-File -FilePath $LogFile -Append -Encoding UTF8
            $Failed++

            $Results += [PSCustomObject]@{
                Test = $Test
                Status = "FAILED"
                Message = "Exit code $ExitCode, no dump"
            }
        }
    }
    catch {
        Write-Host "    " -NoNewline
        Write-ColorOutput "✗ ERROR - Failed to run test: $_" $Red
        "Result: ERROR - $($_.Exception.Message)`n" | Out-File -FilePath $LogFile -Append -Encoding UTF8
        $Failed++

        $Results += [PSCustomObject]@{
            Test = $Test
            Status = "ERROR"
            Message = $_.Exception.Message
        }
    }

    # Clean up temp files
    Remove-Item -Path "$OutputFile.err" -ErrorAction SilentlyContinue
}

Write-Host ""
Write-Header "Test Summary"

$Total = $Tests.Count
Write-Host "Total tests:  $Total"
Write-Host "Passed:       " -NoNewline
Write-ColorOutput $Passed $Green
Write-Host "Warnings:     " -NoNewline
Write-ColorOutput $Warning $Yellow
Write-Host "Failed:       " -NoNewline
Write-ColorOutput $Failed $Red
Write-Host ""

# Log summary
@"
=========================================
Test Summary
=========================================
Total tests:  $Total
Passed:       $Passed
Warnings:     $Warning
Failed:       $Failed

"@ | Out-File -FilePath $LogFile -Append -Encoding UTF8

# Detailed results
Write-Host "Detailed Results:"
$Results | Format-Table -AutoSize

Write-Host ""
Write-ColorOutput "Results saved to: $LogFile" $Blue
Write-ColorOutput "Crash dumps saved to: $CrashDir" $Blue
Write-Host ""

if ($Failed -gt 0) {
    Write-ColorOutput "WARNING: Some tests failed! Check the log file for details." $Red
    Write-Host ""
    exit 1
}

exit 0
