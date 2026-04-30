# CrashReport

[![Version](https://img.shields.io/badge/version-1.0.0-blue.svg)](#)
[![Platform](https://img.shields.io/badge/platform-Windows-blue.svg)](https://www.microsoft.com/windows)
[![C++](https://img.shields.io/badge/C%2B%2B-14-blue.svg)](https://isocpp.org/)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](#license)

**Current version: 1.0.0** — every crash report includes the library version
and (optionally) your application's name and version.

A small, drop-in **crash-reporting library for Windows C++ applications**.
Add two lines to `main()` and your application will produce a Windows
minidump and a human-readable crash report whenever it dies — null pointer
dereference, uncaught exception, stack overflow, heap corruption, pure
virtual call, and a dozen other failure modes.

---

## What you get

When your application crashes, two files appear in the directory you nominate:

| File | What it is |
|------|------------|
| `<exe>.dmp` | Windows minidump. Open in Visual Studio for a full post-mortem debugging session (locals, call stack, threads, modules). |
| `<exe>_stack.txt` | UTF-8 text report with the **cause**, accessed address, CPU registers, OS/process info, symbolicated stack trace, and a per-thread function-call trail. |

A representative `_stack.txt` excerpt:

```
------ Exception Information ------
  Exception Code:    0xC0000005 (EXCEPTION_ACCESS_VIOLATION (memory access fault))
  Exception Address: 0x7FF7FF0723BB
  Access Type:       write
  Accessed Address:  0x0 (likely null/invalid pointer)

------ Stack Trace (DbgHelp) ------
    [0] CrashTestRunner!testNullPointer + 0x1B
        at C:\...\main.cpp:114
    [1] CrashTestRunner!main + 0x41A
        at C:\...\main.cpp:60
    ...
```

## Crashes caught

| Class of crash | Caught? |
|----------------|---------|
| Null / wild pointer dereference | ✅ |
| Read/write to invalid memory | ✅ |
| Integer divide by zero | ✅ |
| Stack overflow (recursive) | ✅ |
| Uncaught C++ exception | ✅ |
| Heap corruption (`STATUS_HEAP_CORRUPTION`) | ✅ |
| Pure virtual function call | ✅ |
| Invalid CRT parameter (`fclose(nullptr)` etc.) | ✅ |
| `operator new` allocation failure | ✅ |
| `abort()`, `Ctrl+C`, `Ctrl+Break`, `SIGTERM` | ✅ |
| Multi-threaded simultaneous crashes | ✅ (atomic crash lock — first thread wins) |
| `__fastfail` (e.g. `/GS` stack guard) | ❌ Uncatchable by Windows design |

See [`documentation/CrashScenarios.md`](documentation/CrashScenarios.md) for
the full list with examples.

---

## Quick guide

### 1. Build

```cmd
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The build produces `CrashReport.dll` (shared) and `CrashReport-s.lib`
(static), plus a usage example and a crash-test runner.

### 2. Use in your code

```cpp
#include "CrashReport.h"

void onCrash()
{
    // Optional: flush logs, save state, notify the user, etc.
    // The crash report itself is written for you.
}

int main(int argc, char* argv[])
{
    CrashReport::ExceptionHandler::setup("crash_dumps");

    // Optional: tag every crash report with your app's name + version.
    CrashReport::ExceptionHandler::setApplicationInfo("MyApp", "2.5.1");

    CrashReport::ExceptionHandler::setExceptionCallback(&onCrash);

    STACK_WATCHER_FUNC;          // Optional: name this frame in the report

    // ... your application code ...
    return 0;
}
```

That's it. If the process crashes, look in `crash_dumps/` for `<exe>.dmp`
and `<exe>_stack.txt`. The text report shows your application's version
**and** the version of the CrashReport library that produced it.

### 3. (Optional) Tag important functions

Add `STACK_WATCHER_FUNC;` as the first statement of any function whose name
should appear in the **Stack Trace (StackWatcher)** section of the report.
Useful for code paths where DbgHelp can't symbolicate (e.g. heavily
optimised release builds):

```cpp
void importantHandler()
{
    STACK_WATCHER_FUNC;
    // ...
}
```

### 4. Verify it works

The repo includes a test harness that exercises every supported crash type:

```cmd
run_crash_tests.bat
```

You should see twelve `PASS` lines and a summary. Each test's dump and
stack trace are archived in `build\test_crashFiles\`.

---

## Repository layout

```
CrashReport/
├── core/                    Library source + public headers
│   ├── inc/                 Public headers (CrashReport.h is the umbrella)
│   └── src/                 Implementation
├── examples/
│   ├── CrashTestRunner/     CLI runner that intentionally crashes 12 ways
│   └── LibraryExample/      Minimal usage demo
├── unittests/               Unit-test scaffolding
├── documentation/           Markdown docs (this README links to them)
├── run_crash_tests.bat      Windows batch script that runs the full suite
└── README.md                You are here
```

---

## Documentation

| Document | What it covers |
|----------|----------------|
| [`documentation/README.md`](documentation/README.md) | Documentation index |
| [`documentation/API.md`](documentation/API.md) | Full public API |
| [`documentation/ExceptionHandler.md`](documentation/ExceptionHandler.md) | The main `setup` / `setExceptionCallback` / `terminate` class |
| [`documentation/StackWatcher.md`](documentation/StackWatcher.md) | The opt-in function-call tracker |
| [`documentation/LibraryInfo.md`](documentation/LibraryInfo.md) | Compile-time library metadata |
| [`documentation/CrashReportFormat.md`](documentation/CrashReportFormat.md) | Layout of the generated `_stack.txt` |
| [`documentation/CrashScenarios.md`](documentation/CrashScenarios.md) | Every crash type the library catches |
| [`documentation/Testing.md`](documentation/Testing.md) | How to run and extend the test suite |
| [`documentation/Building.md`](documentation/Building.md) | CMake configure/build/install details |

---

## Requirements

- Windows 7 SP1 or newer (tested on Windows 10/11)
- MSVC 2022 (17.x) — the codebase uses `__try`/`__except` SEH constructs
- C++14
- CMake 3.20+
- DbgHelp.dll and Psapi.lib (ship with Windows)

Cross-platform support is **not** in scope; the implementation relies on
Windows-specific APIs (`SetUnhandledExceptionFilter`, `MiniDumpWriteDump`,
`StackWalk64`, `_set_purecall_handler`, etc.).

---

## Why use it

- **Two-line integration.** No SDK to manage, no service to deploy.
- **Rich reports out of the box.** Cause, accessed address, registers,
  process and system info, symbolicated stack — already formatted.
- **Catches more than just access violations.** Pure virtual calls, CRT
  invalid parameters, `new` failures, signals, and uncaught C++
  exceptions are all routed to the same report.
- **Thread-safe.** Concurrent crashes on multiple threads do not corrupt
  the output; one crash, one report.
- **No external dependencies.** Just Windows + DbgHelp.

---

## License

MIT

## Author

Alex Krieg
