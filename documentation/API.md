# API Reference

**Library version: 1.0.0**

All public symbols live in the `CrashReport` namespace and are declared in the
single umbrella header:

```cpp
#include "CrashReport.h"
```

Including `CrashReport.h` pulls in the three public classes documented below.

## Public classes

| Class | Header | Purpose |
|-------|--------|---------|
| [`ExceptionHandler`](ExceptionHandler.md) | `ExceptionHandler.h` | Installs OS- and CRT-level crash handlers, writes minidumps and stack traces. |
| [`StackWatcher`](StackWatcher.md) | `StackWatcher.h` | Per-thread, RAII function-entry tracker (used to enrich crash reports). |
| [`LibraryInfo`](LibraryInfo.md) | `CrashReport_info.h` | Compile-time metadata about the library. |

## Public macros

```cpp
STACK_WATCHER_FUNC
```

Place this at the top of any function whose name should appear in the
StackWatcher trace section of the crash report. It declares a stack-allocated
`StackWatcher` object whose lifetime matches the enclosing function. See
[StackWatcher](StackWatcher.md).

## Public typedefs

```cpp
namespace CrashReport {
    using ExceptionCallback = void (*)(void);
}
```

Function-pointer signature for the crash callback registered with
[`ExceptionHandler::setExceptionCallback`](ExceptionHandler.md#setexceptioncallback).

## Quick-reference table

| Call | Effect |
|------|--------|
| `ExceptionHandler::setup(dir)` | Install all handlers, set output directory. Call once at start of `main()`. |
| `ExceptionHandler::setApplicationInfo(name, version)` | Tag every crash report with your application's name and version. |
| `ExceptionHandler::setExceptionCallback(cb)` | Register a function that runs just before the crash report is written. |
| `ExceptionHandler::terminate()` | Manually trigger the crash-report path (rare; useful for fatal-error paths). |
| `StackWatcher::printStack()` | Print the live StackWatcher state to `stdout` (debugging aid). |
| `StackWatcher::toString()` | Return the live StackWatcher state as a `std::string`. |
| `LibraryInfo::printInfo(stream)` | Stream library name, version, build date/time. |
| `LibraryInfo::version` | `constexpr` `Version{ major, minor, patch }` (currently `{1, 0, 0}`). |

## Linking

The library produces both a shared and a static target. Pick one when linking:

```cmake
target_link_libraries(your_app PRIVATE CrashReport_shared)
# or
target_link_libraries(your_app PRIVATE CrashReport_static)
```

Both link against `DbgHelp.lib` and `Psapi.lib` automatically.

## Thread safety

- `ExceptionHandler::setup()` must be called from a single thread once at
  startup. After that, all handlers are thread-safe.
- A crash on any thread is funneled through an atomic crash flag; concurrent
  crashes on other threads block until the first crash report is written.
- `StackWatcher` uses an internal mutex and a per-thread stack, so it is
  safe to use from any thread.

## Headers in detail

- [ExceptionHandler.md](ExceptionHandler.md)
- [StackWatcher.md](StackWatcher.md)
- [LibraryInfo.md](LibraryInfo.md)
