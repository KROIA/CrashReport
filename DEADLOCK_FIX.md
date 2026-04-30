# Deadlock Fix - Crash Handler

## Problem Identified

The crash handler had a critical deadlock bug:

1. `onException()` acquired the crash lock
2. Then called `onTerminate()`
3. `onTerminate()` tried to acquire the same lock again
4. Since the lock was already held, it failed and called `Sleep(INFINITE)`
5. **Result**: Program hung forever after creating .dmp file but before creating stack trace

## Symptoms

- Program stops responding but doesn't fully crash
- `.dmp` file created successfully
- `.txt` stack trace file **NOT** created
- Process must be killed manually

## Solution

Added a static flag `s_insideCrashHandler` to track when we're already inside the crash handling flow:

```cpp
// In ExceptionHandler.h
static volatile long s_insideCrashHandler;

// In onException()
inst.generateCrashDump(pExceptionPointers);
InterlockedExchange(&s_insideCrashHandler, 1);  // Mark we're inside handler
onTerminate();

// In onTerminate()
bool alreadyInHandler = (InterlockedCompareExchange(&s_insideCrashHandler, 1, 1) == 1);
if (!alreadyInHandler && !tryAcquireCrashLock()) {
    Sleep(INFINITE);  // Only sleep if we're not already in handler
}
```

## Additional Improvements

1. **Output Flushing**: Added flush calls before exit to ensure all output is written:
   ```cpp
   std::fflush(stdout);
   std::fflush(stderr);
   std::cout.flush();
   std::cerr.flush();
   ```

## Test Results After Fix

### ✅ Now Working (Previously Hanging)
- `exception` - Unhandled C++ exception
- `stackoverflow` - Recursive stack overflow  
- `multithread` - Multi-threaded crash

### ✅ Always Worked
- `null` - NULL pointer dereference
- `accessviolation` - Access violation
- `nested` - Crash in nested call stack

### ⚠️ May Not Crash Reliably
- `divide` - Integer divide by zero (x64 may not trap)
- `purevirtual` - Pure virtual call (hard to trigger)
- `doubledelete` - Double delete (heap manager dependent)
- `invalidparam` - Invalid CRT parameter (may just return error)
- `bufferoverrun` - Buffer overrun (needs /GS checks)
- `abort` - abort() call

## Platform-Specific Notes

### Integer Division by Zero (x64 Windows)
On x64 architecture, integer division by zero may NOT raise a hardware exception. Instead it may:
- Return undefined value
- Cause undefined behavior
- Not trap at all

**Workaround**: Use floating-point division which always traps:
```cpp
volatile double zero = 0.0;
volatile double result = 1.0 / zero;  // This will trap
```

### Pure Virtual Calls
Modern C++ compilers often inline or optimize away pure virtual calls, making them extremely difficult to trigger reliably in test scenarios.

### Stack Overflow
While now handled correctly, stack overflow crashes may:
- Corrupt the stack too badly to get clean traces
- Hit stack guard pages and terminate immediately
- Vary by compiler optimization level

## Verification

To verify the fix works:

```powershell
cd build\Release
.\CrashTestRunner.exe exception

# Check both files are created:
ls crashFiles_test\CrashTestRunner.exe.dmp        # Should exist
ls crashFiles_test\CrashTestRunner.exe_stack.txt  # Should exist now!
```

Both files should be created and the program should exit cleanly (not hang).

## Files Changed

- `core/inc/ExceptionHandler.h`
  - Added `s_insideCrashHandler` flag
  
- `core/src/ExceptionHandler.cpp`
  - Initialize `s_insideCrashHandler` 
  - Set flag in `onException()` before calling `onTerminate()`
  - Check flag in `onTerminate()` before trying to acquire lock
  - Added output flushing before exit

## Status

✅ **FIXED** - Deadlock resolved, stack trace files now created successfully

The crash handler now properly completes its work and creates both dump and stack trace files without hanging.
