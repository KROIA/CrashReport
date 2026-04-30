# CrashReport — Documentation

Reference documentation for the **CrashReport** library, **version 1.0.0**.

> The library version is also embedded in every crash report (see the
> `------ CrashReport Library ------` section), so support engineers can
> always tell exactly which build produced a given report.

## Contents

| Document | Description |
|----------|-------------|
| [API Reference](API.md) | Full public API of the library |
| [ExceptionHandler](ExceptionHandler.md) | Sets up crash handlers, generates dumps and stack traces |
| [StackWatcher](StackWatcher.md) | Lightweight per-thread function-call tracker |
| [LibraryInfo](LibraryInfo.md) | Library metadata (name, version, build info) |
| [Crash Report Format](CrashReportFormat.md) | Layout of the generated `.txt` stack trace |
| [Crash Scenarios](CrashScenarios.md) | What kinds of crashes are caught and how |
| [Testing Guide](Testing.md) | How to run the bundled crash test suite |
| [Building](Building.md) | CMake configure/build instructions |

## Where to start

1. Read the top-level [README](../README.md) for an overview and a 2-minute quick-start.
2. Check the [API Reference](API.md) for the full public surface.
3. Skim [Crash Scenarios](CrashScenarios.md) to understand which crashes are
   caught reliably and which platform limitations exist.
4. Run `run_crash_tests.bat` (see [Testing Guide](Testing.md)) to see the
   library in action against twelve different crash types.

## Public API at a glance

```cpp
#include "CrashReport.h"

int main()
{
    CrashReport::ExceptionHandler::setup("crash_dumps");
    CrashReport::ExceptionHandler::setApplicationInfo("MyApp", "2.5.1"); // Optional
    CrashReport::ExceptionHandler::setExceptionCallback([] {
        // Optional: save state, notify user, etc.
    });

    STACK_WATCHER_FUNC;          // Optional: track function entry/exit
    // ... your application ...
}
```

When the application crashes, two files are written to the directory passed
to `setup()`:

- `<exe-name>.dmp` — Windows minidump, openable in Visual Studio.
- `<exe-name>_stack.txt` — Human-readable crash report with cause, registers,
  system info, and stack trace.
