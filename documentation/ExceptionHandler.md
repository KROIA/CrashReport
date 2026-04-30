# ExceptionHandler

```cpp
#include "ExceptionHandler.h"

namespace CrashReport {
    class ExceptionHandler;
}
```

`ExceptionHandler` installs every crash hook the library supports, captures
exception context when a crash occurs, and writes a minidump plus a
human-readable crash report.

The class is a singleton and exposes only static functions. There is nothing
to instantiate — call [`setup`](#setup) once at startup and the library does
the rest.

## What gets installed

Calling `setup()` registers all of the following handlers in one step:

| Hook | Purpose |
|------|---------|
| `SetUnhandledExceptionFilter` | Top-level Windows SEH filter. Catches access violations, divide-by-zero, stack overflow, heap corruption, etc. |
| `AddVectoredExceptionHandler` | Pre-SEH handler used as a fallback for stack overflow. |
| `std::set_terminate` | Catches uncaught C++ exceptions / `std::terminate`. |
| `_set_purecall_handler` | Catches calls to pure virtual functions. |
| `_set_invalid_parameter_handler` | Catches invalid arguments to CRT functions. |
| `std::set_new_handler` | Catches `operator new` allocation failures. |
| `signal(SIGABRT, ...)` etc. | Catches `abort()`, `Ctrl+C`, `Ctrl+Break`, `SIGTERM`. |
| `SetThreadStackGuarantee(256 KB)` | Reserves stack for the SEH filter so stack overflow can be handled. |
| `HeapEnableTerminationOnCorruption` | Forces clean termination on heap corruption (when SEH-catchable). |
| `SetErrorMode(...)` | Suppresses Windows error dialog boxes. |

## Public API

### setup

```cpp
static void setup(const std::string& crashExportPath);
```

Installs every crash handler listed above and remembers the directory to
which crash artifacts are written. The directory is created on demand if it
does not exist.

**Parameters**

| Name | Description |
|------|-------------|
| `crashExportPath` | Path (relative or absolute) where `<exe>.dmp` and `<exe>_stack.txt` will be written when a crash occurs. |

**Notes**

- Call exactly once, ideally as the first statement in `main()`.
- Calling more than once is harmless but does not re-register handlers a
  second time — the first registration wins.
- Writes happen on the crashing thread (or on a worker thread for stack
  overflow). The directory must be writable by the process.

**Example**

```cpp
int main(int argc, char** argv)
{
    CrashReport::ExceptionHandler::setup("crash_dumps");
    // ... rest of program
}
```

### setApplicationInfo

```cpp
static void setApplicationInfo(const std::string& name,
                               const std::string& version);
```

Tags every subsequent crash report with the calling application's name and
version. Both fields are free-form strings — typical values are the
executable name and a SemVer string, but any text is accepted.

When set, the report includes an additional section just below the report
banner:

```
------ Application Information ------
  Name:              MyApp
  Version:           2.5.1
```

This appears alongside the always-present library version block:

```
------ CrashReport Library ------
  Name:              CrashReport
  Version:           01.00.0000
  Build Type:        Release
  Compiler:          MSVC 1944
  Compilation Date:  May  1 2026 00:36:04
```

**Parameters**

| Name | Description |
|------|-------------|
| `name` | Display name of your application or library. Empty string suppresses the line. |
| `version` | Version string of your application. Empty string suppresses the line. |

**Notes**

- Optional. If never called, the **Application Information** section is
  omitted entirely; the **CrashReport Library** section is always present.
- Safe to call before or after `setup()`.
- Calling again replaces the previous values.

**Example**

```cpp
CrashReport::ExceptionHandler::setApplicationInfo(
    "MyApp",
    "2.5.1+build.42"
);
```

### setExceptionCallback

```cpp
using ExceptionCallback = void (*)(void);
static void setExceptionCallback(ExceptionCallback callback);
```

Registers a user function that runs **before** the stack trace file is
written. Useful for flushing logs, notifying the user, or saving session
state.

**Parameters**

| Name | Description |
|------|-------------|
| `callback` | Function pointer with signature `void()`. Pass `nullptr` to clear. |

**Notes**

- The callback is invoked once, on the crashing thread, after the minidump
  is written and before the stack trace is written.
- Exceptions that escape the callback are silently swallowed so they cannot
  derail crash reporting.
- The callback should be quick and avoid heavy allocations — the process is
  about to exit.

**Example**

```cpp
void onCrash() {
    log_flush();
    save_unsaved_documents();
}

CrashReport::ExceptionHandler::setExceptionCallback(&onCrash);
```

### terminate

```cpp
static void terminate();
```

Manually invokes the crash-report path. Equivalent to what happens when
`std::terminate` is called: a minidump is generated from the current
context, the callback runs, the stack trace file is written, and the process
exits with code `1`.

**Notes**

- Useful for fatal application-level errors where you want a crash report
  but do not have a real exception to throw.
- Does not return.

**Example**

```cpp
if (criticalInvariantBroken()) {
    CrashReport::ExceptionHandler::terminate();
}
```

## Output files

When a crash occurs the following files are written to the directory passed
to `setup()`:

| File | Contents |
|------|----------|
| `<exe-name>.dmp` | Windows minidump containing thread state, loaded modules, and (limited) memory. Open in Visual Studio with **File → Open → File** and click "Debug with Native Only". |
| `<exe-name>_stack.txt` | Human-readable report — see [Crash Report Format](CrashReportFormat.md). |

`<exe-name>` is the running executable's filename including `.exe`.

## Behavior matrix

| Crash type | Caught by | Dump | Stack trace |
|------------|-----------|------|-------------|
| NULL / invalid pointer write | `SetUnhandledExceptionFilter` | ✅ | ✅ |
| Read/write to bad address | `SetUnhandledExceptionFilter` | ✅ | ✅ |
| Integer divide by zero | `SetUnhandledExceptionFilter` | ✅ | ✅ |
| Stack overflow | `SetUnhandledExceptionFilter` (worker thread) | ✅ | ✅ |
| Uncaught C++ exception | `SetUnhandledExceptionFilter` | ✅ | ✅ |
| Heap corruption (`STATUS_HEAP_CORRUPTION`) | `SetUnhandledExceptionFilter` | ✅ | ✅ |
| Pure virtual call | `_set_purecall_handler` | ✅ | ✅ |
| Invalid CRT parameter | `_set_invalid_parameter_handler` | ✅ | ✅ |
| `new` allocation failure | `std::set_new_handler` | ✅ | ✅ |
| `abort()` / `SIGABRT` | `signal()` re-raised through SEH | ✅ | ✅ |
| Multi-threaded crash | Atomic crash lock — first thread wins | ✅ | ✅ |
| `__fastfail` (e.g. /GS check) | **Uncatchable** by design on Windows | ❌ | ❌ |

See [Crash Scenarios](CrashScenarios.md) for examples and details on
platform limitations.

## Thread safety

- `setup()` is **not** thread-safe. Call it once before spawning threads.
- All registered handlers serialise crash handling through an internal
  atomic flag. Threads that crash while another crash is already being
  processed are blocked indefinitely (the process is about to exit).

## Limitations

- **Windows-only.** The implementation uses `EXCEPTION_POINTERS`, DbgHelp,
  and Win32 APIs.
- **`__fastfail` is not catchable.** Buffer overruns detected by `/GS`,
  some heap-corruption fast paths, and `RtlFailFast` bypass SEH entirely.
- **Stack overflow on threads that did not call `SetThreadStackGuarantee`**
  may produce a partial dump. Worker threads created by your application
  may benefit from calling it themselves.
