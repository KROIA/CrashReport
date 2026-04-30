# Crash Testing Guide

This guide explains how to test the CrashReport library to ensure it catches all types of crashes reliably.

## Overview

The CrashReport library has been enhanced to catch the following types of crashes:

1. **Windows SEH (Structured Exception Handling)**
   - Access violations (null pointer, invalid memory access)
   - Stack overflows
   - Divide by zero
   - Illegal instructions
   - All other structured exceptions

2. **Signal Handlers**
   - SIGABRT (abort)
   - SIGFPE (floating point exception)
   - SIGILL (illegal instruction)
   - SIGINT (interrupt)
   - SIGSEGV (segmentation fault)
   - SIGTERM (termination)
   - SIGBREAK (Ctrl+Break)

3. **C++ Exception Handlers**
   - Unhandled exceptions (std::exception and derived)
   - Unknown exceptions
   - std::terminate calls

4. **Windows CRT Handlers**
   - Pure virtual function calls
   - Invalid parameters to CRT functions
   - Memory allocation failures (new handler)
   - Security errors (buffer overruns)

5. **Multi-threading**
   - Thread-safe crash handling (only one crash processed at a time)
   - Crashes in worker threads

## Improvements Made

### 1. Additional Exception Handlers
- **Pure Virtual Call Handler**: Catches calls to pure virtual functions
- **Invalid Parameter Handler**: Catches invalid parameters passed to CRT functions
- **New Handler**: Catches memory allocation failures
- **Security Error Handler**: Catches buffer overrun detection

### 2. Thread Safety
- Atomic crash flag ensures only one thread processes a crash
- Other threads are blocked if multiple threads crash simultaneously
- Prevents corrupted crash dumps from concurrent access

### 3. Improved Exception Callback Safety
- Exception callbacks are wrapped in try-catch to prevent secondary crashes
- Null checks for std::current_exception before rethrowing

### 4. Better Error Reporting
- Each handler type is identified in error output
- Stack traces include both DbgHelp and StackWatcher information
- Thread IDs are logged for multi-threaded crashes

## Testing Tools

### 1. Unit Tests (TST_CrashScenarios.h)

Location: `unittests/ExampleTest/tests/TST_CrashScenarios.h`

Contains 13 test cases with crash code **commented out by default** for safety:
- `testNullPointerDereference` - Tests null pointer access
- `testDivideByZero` - Tests integer division by zero
- `testStackOverflow` - Tests recursive stack overflow
- `testUnhandledException` - Tests uncaught C++ exception
- `testAbortSignal` - Tests abort() call
- `testPureVirtualCall` - Tests pure virtual function call
- `testInvalidParameter` - Tests invalid CRT parameter
- `testArrayOutOfBounds` - Tests array bounds violation
- `testMultiThreadedCrash` - Tests crashes in multiple threads
- `testDoubleDelete` - Tests double deletion
- `testAccessViolationRead` - Tests read from invalid memory
- `testAccessViolationWrite` - Tests write to invalid memory
- `testStackWatcherIntegration` - Tests stack tracking (non-crashing)

**To test a specific crash scenario:**
1. Open `TST_CrashScenarios.h`
2. Find the test you want to run
3. Uncomment the crash code
4. Run the unit test executable
5. Verify crash dump and stack trace are created in `crashFiles_unittest/`
6. Re-comment the crash code

### 2. CrashTestRunner (Dedicated Test Executable)

Location: `unittests/ExampleTest/CrashTestRunner.cpp`

A standalone command-line tool for testing crash scenarios individually.

**Usage:**
```bash
CrashTestRunner.exe <test_name>
```

**Available tests:**
- `null` - NULL pointer dereference
- `divide` - Divide by zero
- `stackoverflow` - Stack overflow (recursive)
- `exception` - Unhandled C++ exception
- `abort` - Call abort()
- `purevirtual` - Pure virtual function call
- `invalidparam` - Invalid parameter to CRT function
- `accessviolation` - Access violation (bad pointer)
- `doubledelete` - Double delete
- `bufferoverrun` - Buffer overrun
- `multithread` - Crash in multiple threads
- `nested` - Crash in nested call stack

**Examples:**
```bash
# Test null pointer crash
CrashTestRunner.exe null

# Test divide by zero
CrashTestRunner.exe divide

# Test multi-threaded crash
CrashTestRunner.exe multithread
```

**Output:**
- Crash dumps: `crashFiles_test/<exe_name>.dmp`
- Stack traces: `crashFiles_test/<exe_name>_stack.txt`

## Manual Testing Procedure

### Step 1: Build the Project
```bash
cmake -B build
cmake --build build --config Release
```

### Step 2: Test Individual Crash Scenarios

For each crash type, run:
```bash
cd build/bin/Release
CrashTestRunner.exe <test_name>
```

### Step 3: Verify Output

After each crash, check that the following files are created:
1. `crashFiles_test/<exe_name>.dmp` - Minidump file (can be opened in Visual Studio)
2. `crashFiles_test/<exe_name>_stack.txt` - Human-readable stack trace

### Step 4: Inspect Stack Trace

The stack trace file should contain:
- Library information (name, version, build date)
- Timestamp of crash
- Thread ID
- Signal information (if applicable)
- DbgHelp stack trace (with return addresses)
- StackWatcher stack trace (custom function tracking)

Example stack trace:
```
CrashReport
Version:        1.0.0
Build:          Apr 30 2026 - 10:30:15

Timestamp: 2026-04-30 10:35:42
An exception caused a crash.
Crashed thread ID: 12345
Abort signal: 11 SIGSEGV: segment violation

DbgHelp stack content:
    [0]	testNullPointer                                  Return address: 0x7ff6abc12340
    [1]	main                                             Return address: 0x7ff6abc12450
    [2]	__scrt_common_main_seh                          Return address: 0x7ff6abc12560

StackWatcher content:
Stack:
  Stack of thread ID: 12345
    [0] int __cdecl main(int,char **)
    [1] void __cdecl testNullPointer(void)
```

### Step 5: Debug with Visual Studio

To analyze the crash dump:
1. Open Visual Studio
2. File → Open → File → Select the `.dmp` file
3. Click "Debug with Native Only"
4. Visual Studio will show the exact line where the crash occurred

## Integration Testing

### Example Application

Location: `examples/LibraryExample/main.cpp`

The example application demonstrates:
- Setting up crash handling
- Registering a callback
- Multi-threaded crash scenarios
- StackWatcher usage

**To test:**
1. Uncomment the crash code in `main.cpp`
2. Build and run the example
3. Verify crash is caught and logged

## Expected Behavior

When a crash occurs, the library should:
1. ✅ Catch the crash (no Windows error dialog appears)
2. ✅ Call the registered callback (if set)
3. ✅ Generate a minidump file (.dmp)
4. ✅ Generate a stack trace file (_stack.txt)
5. ✅ Print stack trace to console
6. ✅ Exit gracefully with exit code 1

## Thread Safety Verification

To verify thread-safe crash handling:
1. Run `CrashTestRunner.exe multithread`
2. Verify only ONE crash dump is created (not multiple)
3. Check that the stack trace shows one of the threads
4. Verify no corruption or garbled output

## Troubleshooting

### Crash Not Caught
- Ensure `ExceptionHandler::setup()` is called early in `main()`
- Check that crash handler isn't being removed by another library
- Verify the application isn't running under a debugger (debuggers catch exceptions first)

### Stack Trace Empty or Incomplete
- Ensure DbgHelp.dll is available on the system
- Build with symbols enabled (Debug or RelWithDebInfo)
- Check that SymInitialize succeeds

### Callback Not Called
- Verify callback is registered with `setExceptionCallback()`
- Check that callback doesn't throw exceptions (they are caught but may interfere)

### StackWatcher Shows No Functions
- Make sure `STACK_WATCHER_FUNC` macro is used in functions
- Verify it's called before the crash occurs

## Best Practices

1. **Always call setup() early**: Call `ExceptionHandler::setup()` at the start of `main()`
2. **Use StackWatcher**: Add `STACK_WATCHER_FUNC` to important functions
3. **Test regularly**: Run crash tests after any changes to the library
4. **Check all scenarios**: Test at least the most common crash types
5. **Verify thread safety**: Test multi-threaded scenarios
6. **Keep dumps secure**: Crash dumps may contain sensitive data

## Platform-Specific Notes

### Windows
- All handlers are fully functional on Windows
- Minidump files can be opened in Visual Studio or WinDbg
- DbgHelp.dll must be available (ships with Windows)

### Future Cross-Platform Support
Currently Windows-only. For cross-platform support, consider:
- Linux: Use signal handlers and generate core dumps
- macOS: Use Mach exception handlers
- Cross-platform: Google Breakpad or Mozilla Crashpad

## Performance Considerations

The crash handling adds minimal overhead:
- Setup: One-time initialization cost (negligible)
- Runtime: No overhead when no crash occurs
- StackWatcher: Small overhead per function call/return
- Crash handling: Only executes when a crash occurs

## Conclusion

The CrashReport library now provides comprehensive crash handling for Windows applications. By following this guide and running the provided tests, you can verify that all crash scenarios are properly caught and logged, ensuring your application can recover gracefully from unexpected errors and provide useful debugging information.
