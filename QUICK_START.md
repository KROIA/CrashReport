# Quick Start Guide - Testing CrashReport

This guide gets you testing the improved crash handling in under 5 minutes.

## Step 1: Build the Project (2 minutes)

```bash
# Configure and build
cmake -B build
cmake --build build --config Release
```

## Step 2: Run a Quick Test (1 minute)

### Option A: Test a Single Crash (Recommended)

```bash
cd build\bin\Release
CrashTestRunner.exe null
```

**Expected output:**
- Console shows crash is happening
- Callback message: "CRASH CALLBACK TRIGGERED"
- Stack trace printed to console
- Files created in `crashFiles_test/`:
  - `CrashTestRunner.exe.dmp` - Binary dump file
  - `CrashTestRunner.exe_stack.txt` - Human-readable stack trace

### Option B: Run All Tests Automatically (2 minutes)

From the project root directory:

**Windows Command Prompt:**
```bash
run_crash_tests.bat
```

**PowerShell (Recommended):**
```powershell
pwsh run_crash_tests.ps1
```

**Expected output:**
- All 12 crash scenarios tested
- Results logged to `test_results/test_run_<timestamp>.log`
- Individual crash dumps in `crashFiles_test/`

## Step 3: Verify the Results (1 minute)

### Check Console Output

You should see:
```
========================================
Running crash test: null
========================================

Testing NULL pointer dereference...

========================================
CRASH CALLBACK TRIGGERED - Handler working!
========================================

CrashReport
Version:        1.0.0
...

Stack trace appears here...
```

### Check Generated Files

1. **Crash Dump** (`crashFiles_test/*.dmp`)
   - Binary file that can be opened in Visual Studio
   - Contains full memory state at crash time
   - Can be debugged with Visual Studio debugger

2. **Stack Trace** (`crashFiles_test/*_stack.txt`)
   - Human-readable text file
   - Contains library info, timestamp, thread ID
   - Shows both DbgHelp and StackWatcher stack traces

### Example Stack Trace Content

```
CrashReport
Version:        1.0.0
Build:          Apr 30 2026 - 10:30:15

Timestamp: 2026-04-30 10:35:42
An exception caused a crash.
Crashed thread ID: 12345

DbgHelp stack content:
    [0]	testNullPointer            Return address: 0x7ff6abc12340
    [1]	main                       Return address: 0x7ff6abc12450

StackWatcher content:
Stack:
  Stack of thread ID: 12345
    [0] int __cdecl main(int,char **)
    [1] void __cdecl testNullPointer(void)
```

## Available Test Scenarios

Run any of these with `CrashTestRunner.exe <test_name>`:

| Test Name | Description |
|-----------|-------------|
| `null` | NULL pointer dereference |
| `divide` | Division by zero |
| `stackoverflow` | Recursive stack overflow |
| `exception` | Unhandled C++ exception |
| `abort` | Call to abort() |
| `purevirtual` | Pure virtual function call |
| `invalidparam` | Invalid CRT parameter |
| `accessviolation` | Access violation |
| `doubledelete` | Double delete |
| `bufferoverrun` | Buffer overrun |
| `multithread` | Multi-threaded crash |
| `nested` | Crash in nested call stack |

## Debugging with Visual Studio

1. Open Visual Studio
2. **File → Open → File**
3. Select a `.dmp` file from `crashFiles_test/`
4. Click **"Debug with Native Only"**
5. Visual Studio shows the exact crash location

## Integration in Your Application

Add to your `main()` function:

```cpp
#include "CrashReport.h"

void myCrashCallback()
{
    std::cout << "Application is crashing, saving state...\n";
    // Save important data before exit
}

int main(int argc, char* argv[])
{
    // Setup crash handler (do this first!)
    CrashReport::ExceptionHandler::setup("crash_dumps");
    CrashReport::ExceptionHandler::setExceptionCallback(myCrashCallback);

    // Use StackWatcher in important functions
    STACK_WATCHER_FUNC;

    // Your application code here...

    return 0;
}
```

## Troubleshooting

### Problem: Executable not found
**Solution:** Build the project first with the commands in Step 1

### Problem: No crash dump created
**Solution:** 
- Make sure you're not running under a debugger
- Check that `ExceptionHandler::setup()` was called
- Verify the crash dump directory exists and is writable

### Problem: Stack trace is empty
**Solution:**
- Build with debug symbols: `cmake --build build --config RelWithDebInfo`
- Ensure DbgHelp.dll is available (comes with Windows)

### Problem: Test doesn't crash
**Solution:**
- Some tests (like `purevirtual`) are hard to trigger
- This is expected behavior for certain scenarios
- Try other tests like `null` or `accessviolation` which always work

## Next Steps

1. ✅ **Read IMPROVEMENTS.md** - See all improvements made
2. ✅ **Read CRASH_TESTING_GUIDE.md** - Detailed testing guide
3. ✅ **Integrate in your app** - Add crash handling to your application
4. ✅ **Test regularly** - Run crash tests before each release

## Summary

You should now have:
- ✅ Built the project
- ✅ Ran at least one crash test
- ✅ Verified crash dumps are created
- ✅ Seen the stack trace output

**The crash handler is working if:**
1. No Windows error dialog appears
2. Crash dumps (`.dmp`) are created
3. Stack traces (`.txt`) are generated
4. Your callback is executed (if set)

## Support

For issues or questions:
- Check `CRASH_TESTING_GUIDE.md` for detailed documentation
- Review `IMPROVEMENTS.md` for what changed
- Look at example code in `examples/LibraryExample/main.cpp`

---

**Time to fully test: ~5 minutes**  
**Time to integrate: ~2 minutes**

**Status: Production Ready ✅**
