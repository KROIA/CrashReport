# Testing Guide

The library ships with a dedicated test executable (`CrashTestRunner.exe`)
and a batch script that exercises every supported crash scenario in turn.

## Quick run

After building (see [Building](Building.md)):

```cmd
run_crash_tests.bat
```

You should see twelve `PASS` lines and a summary:

```
[null] PASS - dump + stack trace generated
[divide] PASS - dump + stack trace generated
...
========================================
Test Summary
========================================
Total:   12
Passed:  12
Partial: 0
Failed:  0
```

## What the script does

For each test name in the list:

1. Removes any leftover `CrashTestRunner.exe.dmp` and
   `CrashTestRunner.exe_stack.txt` from the previous test.
2. Runs `build\CrashTestRunner.exe <test_name>` and captures stdout/stderr
   to `test_results\<test_name>_output.txt`.
3. Classifies the result:
   - **PASS** — both `.dmp` and `_stack.txt` were produced.
   - **PARTIAL** — only one of the two was produced.
   - **FAIL** — neither was produced.
4. Renames the produced files to `<test_name>.dmp` and
   `<test_name>_stack.txt` so each test keeps its own copy.
5. Appends per-test output and result to a single combined log
   `test_results\test_run_<timestamp>.log`.

## Running an individual test

Each test is a separate command-line argument to `CrashTestRunner.exe`:

```cmd
build\CrashTestRunner.exe null
build\CrashTestRunner.exe stackoverflow
build\CrashTestRunner.exe exception
```

The full list:

```
null divide stackoverflow exception abort purevirtual invalidparam
accessviolation doubledelete bufferoverrun multithread nested
```

Run with no arguments to see usage and the test list.

## Outputs

| Location | What |
|----------|------|
| `build\test_crashFiles\<test>.dmp` | Per-test minidump (open in Visual Studio). |
| `build\test_crashFiles\<test>_stack.txt` | Per-test human-readable report. |
| `build\test_results\<test>_output.txt` | Per-test stdout/stderr capture. |
| `build\test_results\test_run_<timestamp>.log` | Combined log of an entire suite run. |

## Inspecting a dump

1. Open Visual Studio.
2. **File → Open → File**, select the `.dmp`.
3. In the dump summary, click **Debug with Native Only**.
4. The debugger jumps to the exact instruction that crashed; locals and
   call-stack windows behave as in a live debugging session.

A matching `.pdb` next to the executable produces full source/line info.
The provided test setup builds with PDBs by default.

## Inspecting a stack-trace text file

The `_stack.txt` files are pure UTF-8 text — open them in any editor.
See [Crash Report Format](CrashReportFormat.md) for the layout.

## Re-running after changes

After modifying library code:

```cmd
cmake --build build --config Release
run_crash_tests.bat
```

The script overwrites `test_results\` and `test_crashFiles\` on each run,
so the most recent results are always at hand.

## Continuous integration

The batch script returns:

| Exit code | Meaning |
|-----------|---------|
| `0` | All tests passed (no `FAIL`s) |
| `1` | At least one test failed |

`PARTIAL` results do **not** cause a non-zero exit — a partial result
indicates a quirk of a specific scenario rather than a regression.

## Adding a new scenario

1. Add a new test function to
   `examples/CrashTestRunner/main.cpp` and include its name in the
   `if/else if` dispatch in `main()`.
2. Add the new name to the `TESTS=` list in `run_crash_tests.bat`.
3. Document the scenario in
   [CrashScenarios.md](CrashScenarios.md).
4. Rebuild and run `run_crash_tests.bat`.
