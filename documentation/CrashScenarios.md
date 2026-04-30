# Crash Scenarios

This document lists every crash type the library is designed to catch,
with the actual test code and notes about what users should expect.

The bundled `CrashTestRunner.exe` exercises all of these. Run
`run_crash_tests.bat` from the project root to verify them on your
machine.

## Summary

| Test name | Crash mechanism | Caught by |
|-----------|-----------------|-----------|
| `null` | Null pointer dereference | SEH (`EXCEPTION_ACCESS_VIOLATION`) |
| `divide` | Integer division by zero | SEH (`EXCEPTION_INT_DIVIDE_BY_ZERO`) |
| `stackoverflow` | Recursive stack overflow | SEH on worker thread |
| `exception` | Uncaught `std::runtime_error` | SEH (`EXCEPTION_CXX`) |
| `abort` | `std::abort()` | `signal(SIGABRT)` re-raised through SEH |
| `purevirtual` | Pure virtual call from destructor | `_set_purecall_handler` |
| `invalidparam` | `fclose(nullptr)` | `_set_invalid_parameter_handler` |
| `accessviolation` | Write to fixed bad address | SEH |
| `doubledelete` | Simulated heap corruption | SEH (`STATUS_HEAP_CORRUPTION`) |
| `bufferoverrun` | Out-of-range stack array write | SEH |
| `multithread` | Three threads crash simultaneously | SEH + atomic crash lock |
| `nested` | Crash deep in a call chain | SEH |

## Detailed scenarios

### null — Null pointer dereference

```cpp
int* ptr = nullptr;
*ptr = 42;
```

The OS raises `EXCEPTION_ACCESS_VIOLATION` (`0xC0000005`). The report shows
`Access Type: write` and `Accessed Address: 0x0 (likely null/invalid pointer)`.

### divide — Integer divide by zero

```cpp
volatile int zero = 0;
volatile int result = 42 / zero;
```

Raises `EXCEPTION_INT_DIVIDE_BY_ZERO` (`0xC0000094`).

### stackoverflow — Recursive stack overflow

```cpp
void recursiveStackOverflow(int depth) {
    volatile char buffer[8192];
    buffer[0] = static_cast<char>(depth);
    recursiveStackOverflow(depth + 1);
}
```

Special handling:

1. `setup()` calls `SetThreadStackGuarantee(256 KB)` so a stack page is
   reserved exclusively for the SEH filter.
2. The SEH filter calls `_resetstkoflw()` to restore the guard page.
3. The minidump is generated on a freshly created worker thread that has
   its own clean stack.

### exception — Uncaught C++ exception

```cpp
throw std::runtime_error("Intentional unhandled exception");
```

MSVC raises `EXCEPTION_CXX` (`0xE06D7363`). The report's
**C++ Exception** section includes the `what()` message when available.

### abort — `std::abort()`

```cpp
std::abort();
```

Triggers `SIGABRT`. Our `signal()` handler re-raises it as a
`CRASH_REPORT_SIGNAL_EXCEPTION` (`0xE0000001`) so the SEH filter receives
proper `EXCEPTION_POINTERS` for the minidump.

### purevirtual — Pure virtual call from destructor

```cpp
class Base {
public:
    virtual ~Base() { callPure(); }    // vptr is now Base's vtable
    virtual void pureMethod() = 0;
    void callPure() { pureMethod(); }  // indirect dispatch -> _purecall
};
class Derived : public Base { void pureMethod() override {} };

delete new Derived();
```

The trick: a non-virtual wrapper (`callPure`) defeats MSVC's
devirtualisation, forcing an indirect virtual dispatch through the base
vtable, which slot still points at the compiler-generated `_purecall`
stub. Caught by `_set_purecall_handler`.

### invalidparam — Invalid CRT parameter

```cpp
FILE* file = nullptr;
fclose(file);
```

Routes through `_set_invalid_parameter_handler`.

### accessviolation — Write to fixed bad address

```cpp
int* ptr = (int*)0xDEADBEEF;
*ptr = 42;
```

Same exception code as `null` but with a different accessed address.
Demonstrates that the report correctly distinguishes "null-ish" from "wild
pointer".

### doubledelete — Simulated heap corruption

```cpp
RaiseException(STATUS_HEAP_CORRUPTION, 0, 0, nullptr);
```

Real double-delete is unreliable on modern Windows: the heap manager either
calls uncatchable `__fastfail`, silently caches the freed block, or hangs
in a corrupted free-list traversal. The test instead raises
`STATUS_HEAP_CORRUPTION` directly so the **handler** is reliably exercised.

### bufferoverrun — Out-of-range stack array write

```cpp
int arr[10];
volatile int index = 1000000;
arr[index] = 42;
```

Triggers an access violation when the OOB write hits unmapped memory.
Note: writes that stay within the 10-page stack region may not crash; this
test uses `1000000` to ensure it leaves the mapped stack.

### multithread — Three threads crash simultaneously

```cpp
std::thread t1(threadCrashFunc), t2(threadCrashFunc), t3(threadCrashFunc);
t1.join(); t2.join(); t3.join();
```

The first thread to crash acquires the atomic crash lock; the other two
block in `Sleep(INFINITE)` until the process exits. Exactly one minidump
and one stack trace are produced.

### nested — Crash deep in a call chain

```cpp
void nestedLevel3() { int* p = nullptr; *p = 42; }
void nestedLevel2() { nestedLevel3(); }
void nestedLevel1() { nestedLevel2(); }
void testNestedCallCrash() { nestedLevel1(); }
```

Demonstrates that StackWatcher captures the full call chain even when the
DbgHelp trace truncates or fails to symbolicate intermediate frames.

## Limitations

The following crash mechanisms are **not** caught — by Windows design:

| Mechanism | Why |
|-----------|-----|
| `__fastfail(FAST_FAIL_*)` | Uses `ud2`; bypasses SEH, vectored handlers, and the unhandled exception filter. |
| `/GS` stack-buffer overrun detection | Calls `__report_gsfailure` which uses `__fastfail`. |
| `HeapEnableTerminationOnCorruption` triggered by real corruption | Same — uses `__fastfail`. |
| Process killed by another process (`TerminateProcess`) | The process never gets a chance to run any code. |

To handle these you would need an out-of-process watcher (e.g. Google
Breakpad / Crashpad) — outside the scope of this library.
