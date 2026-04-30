# Crash Report Format

When the crash handler fires, it writes two artifacts side-by-side:

| File | Format | Audience |
|------|--------|----------|
| `<exe>.dmp` | Windows minidump (binary) | Visual Studio, WinDbg |
| `<exe>_stack.txt` | UTF-8 text | Humans, log aggregators |

This document describes the `_stack.txt` layout. The minidump uses the
standard format documented by Microsoft and is best opened in Visual Studio
via **File → Open → File → Debug with Native Only**.

## Sections

The text file is composed of these sections, in this fixed order:

1. **Header banner** — `CRASH REPORT` title with timestamp and thread id.
2. **Application Information** *(if `setApplicationInfo` was called)* — your app's name and version.
3. **CrashReport Library** — name, version, build type, compiler, build date of the library that produced the report.
4. **Exception Information** — what went wrong.
5. **Process Information** — what was running.
6. **System Information** — what the host looked like.
7. **C++ Exception** *(if applicable)* — the active `std::exception` and its `what()`.
8. **CPU Registers** — register snapshot at the crash site.
9. **Stack Trace (DbgHelp)** — symbolicated frames walked from the captured `CONTEXT`.
10. **Stack Trace (StackWatcher)** — opt-in user-side function trail.
11. **Footer banner** — `END OF CRASH REPORT`.

Each section starts with a header line beginning with `------`.

## Sample report (`null` test)

```
================================================================
                       CRASH REPORT
================================================================
Timestamp:           2026-05-01 00:42:21
Crashed Thread ID:   12296

------ Application Information ------
  Name:              CrashTestRunner
  Version:           1.0.0

------ CrashReport Library ------
  Name:              CrashReport
  Version:           01.00.0000
  Build Type:        Release
  Compiler:          MSVC 1944
  Compilation Date:  May  1 2026 00:36:04

------ Exception Information ------
  Exception Code:    0xC0000005 (EXCEPTION_ACCESS_VIOLATION (memory access fault))
  Exception Flags:   0x0
  Exception Address: 0x7FF68B2123BB
  Parameters:        2
  Access Type:       write
  Accessed Address:  0x0 (likely null/invalid pointer)

------ Process Information ------
  Process ID:        13688
  Executable:        C:\...\CrashTestRunner.exe
  Command Line:      "C:\...\CrashTestRunner.exe" null
  Working Dir:       C:\...\build
  Process Uptime:    0 seconds

------ System Information ------
  OS Version:        Windows 10.0 (build 19045)
  CPU Architecture:  x64 (AMD64)
  Logical CPUs:      6
  Page Size:         4096 bytes
  Memory Load:       46%
  Total Physical:    16383 MB
  Available Phys:    8788 MB
  Process Working:   3 MB (peak 4 MB)

------ CPU Registers ------
  RAX = 0x0    RBX = 0x6C6C756E
  RCX = 0xFFFFFFFF    RDX = 0x7FFD05736C00
  ...
  RIP = 0x7FF7FF0723BB    EFlags = 0x10246

------ Stack Trace (DbgHelp) ------
    [0] CrashTestRunner!testNullPointer + 0x1B (0x7FF7FF0723BB)
        at C:\...\examples\CrashTestRunner\main.cpp:114
    [1] CrashTestRunner!main + 0x41A (0x7FF7FF0728D3)
        at C:\...\examples\CrashTestRunner\main.cpp:60
    [2] KERNEL32!BaseThreadInitThunk + 0x14 (0x7FFD124D7344)
    [3] ntdll!RtlUserThreadStart + 0x21 (0x7FFD13EC26B1)

------ Stack Trace (StackWatcher) ------
  Stack of thread ID: 6640
    [0] int __cdecl main(int,char *[])
    [1] void __cdecl testNullPointer(void)

================================================================
                    END OF CRASH REPORT
================================================================
```

## Section reference

### Application Information

Present **only** when the application called
[`ExceptionHandler::setApplicationInfo(name, version)`](ExceptionHandler.md#setapplicationinfo).
Both fields are free-form strings supplied by the application; no formatting
is enforced.

| Field | Meaning |
|-------|---------|
| `Name` | Application or component display name. |
| `Version` | Application version string (typically SemVer). |

### CrashReport Library

Always present. Identifies the exact build of the **CrashReport** library
that produced the report. Useful for triage when multiple builds of an
application coexist in the field.

| Field | Source |
|-------|--------|
| `Name` | `LibraryInfo::name` (always `"CrashReport"`). |
| `Version` | `LibraryInfo::version.toString()` — driven by `LIBRARY_VERSION` in `CMakeLists.txt`. |
| `Build Type` | `Debug` or `Release`. |
| `Compiler` | `MSVC <_MSC_VER>` / `GCC <__VERSION__>` / `Clang <__clang_version__>`. |
| `Compilation Date` | `__DATE__` `__TIME__` from when the library was compiled. |

### Exception Information

The most important section — describes *what* happened.

| Field | Meaning |
|-------|---------|
| `Exception Code` | Hex NTSTATUS code with a human-readable name (e.g. `EXCEPTION_ACCESS_VIOLATION`). |
| `Exception Flags` | Bitmask. `0x1` = `EXCEPTION_NONCONTINUABLE`. |
| `Exception Address` | Instruction pointer at the time of the fault. |
| `Parameters` | Number of additional `ExceptionInformation[]` entries. |

Code-specific extras are decoded:

- **`EXCEPTION_ACCESS_VIOLATION`** — adds `Access Type` (`read` / `write` /
  `DEP`) and `Accessed Address`. Low addresses are flagged as "likely
  null/invalid pointer".
- **`EXCEPTION_IN_PAGE_ERROR`** — same plus `NTSTATUS` of the underlying I/O
  failure.
- **`EXCEPTION_CXX` (`0xE06D7363`)** — adds `C++ Object Addr` (the address of
  the thrown object).

### Process Information

| Field | Meaning |
|-------|---------|
| `Process ID` | Win32 PID. |
| `Executable` | Full path to the running EXE. |
| `Command Line` | Verbatim from `GetCommandLineA()`. |
| `Working Dir` | Current working directory at crash time. |
| `Process Uptime` | Seconds since the process started. |

### System Information

| Field | Meaning |
|-------|---------|
| `OS Version` | Major.Minor + build number via `RtlGetVersion`. |
| `CPU Architecture` | `x64 (AMD64)`, `x86`, `ARM`, `ARM64`, or `unknown`. |
| `Logical CPUs` | `dwNumberOfProcessors`. |
| `Page Size` | Memory page granularity. |
| `Memory Load` | OS-wide memory pressure as a percentage. |
| `Total Physical` / `Available Phys` | RAM totals in MB. |
| `Process Working` | Working set + peak working set in MB. |

### C++ Exception

Present only when `std::current_exception()` was non-null at the start of
`onTerminate`. Reports either the exception's `what()` text (if it derives
from `std::exception`) or simply that an unknown exception was active.

### CPU Registers

x64 (AMD64) builds dump RAX–R15, RIP, RSP, RBP, EFlags. x86 builds dump
EAX–EDI, EIP, ESP, EBP, EFlags. Other architectures emit a single
`(register dump not implemented for this architecture)` line.

### Stack Trace (DbgHelp)

Walked using `StackWalk64` starting from the captured `CONTEXT`, so the
frames belong to the *crashing* code, not the handler that wrote the
report.

Each line has the form:

```
[N] module!symbol + 0xOFFSET (0xADDRESS)
    at <source-file>:<line>
```

The `at <source-file>:<line>` continuation appears when DbgHelp could load
a `.pdb` for the module. The library searches for PDBs in:

1. The executable's directory.
2. The parent directory (e.g. `build/Release` → `build/`).
3. Anything in the `_NT_SYMBOL_PATH` environment variable.

### Stack Trace (StackWatcher)

Generated from in-process per-thread tracking — see
[StackWatcher](StackWatcher.md). Only contains frames from functions that
explicitly opted in with `STACK_WATCHER_FUNC`. Unaffected by stack
corruption.

## Encoding and line endings

- UTF-8 text without a BOM.
- LF line endings (suitable for piping into log aggregators on any OS).
- One file per crash; the file is overwritten on the next crash unless the
  test runner / harness archives it first.
