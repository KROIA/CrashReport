# StackWatcher

```cpp
#include "StackWatcher.h"

namespace CrashReport {
    class StackWatcher;
}
```

`StackWatcher` is a lightweight, opt-in, per-thread function-call tracker.
Place a `StackWatcher` instance (or, more commonly, the
[`STACK_WATCHER_FUNC`](#stack_watcher_func) macro) at the top of any
function whose name should appear in the crash report.

It does **not** replace the DbgHelp stack walk — both traces appear in the
report side by side. StackWatcher remains accurate even when the OS stack is
corrupted (e.g. after a stack overflow), because it reads from a static
container, not from the call stack.

## How it works

- Each constructor pushes `this` onto a per-thread vector of
  `StackWatcher*`.
- Each destructor pops `this` from the vector.
- The crash handler reads the vector to print the current per-thread call
  trail.

The container is keyed by `GetCurrentThreadId()` so each thread has its own
stack. Access is protected by an internal mutex; during crash handling the
mutex is acquired with `try_lock` to avoid deadlocks.

## STACK_WATCHER_FUNC

```cpp
#define STACK_WATCHER_FUNC \
    static const char *funcName = __FUNCSIG__; \
    CrashReport::StackWatcher stackWatcherElementObj(funcName);
```

The recommended way to register a function. Place it as the first statement
of any function you want to appear in the report.

```cpp
void doWork()
{
    STACK_WATCHER_FUNC;
    // ... function body ...
}
```

The name captured is `__FUNCSIG__` (full MSVC signature) so overloaded and
templated functions are distinguishable.

## Public API

### Constructor

```cpp
StackWatcher(const char* name);
```

Adds `this` to the current thread's stack with the given name. The pointer
is stored as-is (not copied), so `name` must outlive the `StackWatcher`
instance. The macro takes care of this by storing the name in `static`
storage.

### Destructor

```cpp
~StackWatcher();
```

Removes `this` from the current thread's stack. If the destruction order
is wrong (e.g. due to non-local control flow), the stack is repaired by
zeroing entries above this index.

### init

```cpp
static void init();
```

Optional one-time initialisation hook. The library calls this implicitly
when the first watcher is created — manual invocation is not required.

### printStack

```cpp
static void printStack();
```

Print every thread's current stack to `stdout`. Useful for debugging
without crashing.

### toString

```cpp
static std::string toString();
```

Return every thread's current stack as a formatted `std::string`. The
crash handler uses this to produce the **Stack Trace (StackWatcher)**
section of the report.

## Example

```cpp
#include "CrashReport.h"

void leafFn()
{
    STACK_WATCHER_FUNC;
    int* p = nullptr;
    *p = 42;             // crash
}

void middleFn() { STACK_WATCHER_FUNC; leafFn(); }
void outerFn()  { STACK_WATCHER_FUNC; middleFn(); }

int main()
{
    CrashReport::ExceptionHandler::setup("crash_dumps");
    STACK_WATCHER_FUNC;
    outerFn();
}
```

The resulting `<exe>_stack.txt` contains:

```
------ Stack Trace (StackWatcher) ------
  Stack of thread ID: 12345
    [0] int __cdecl main(void)
    [1] void __cdecl outerFn(void)
    [2] void __cdecl middleFn(void)
    [3] void __cdecl leafFn(void)
```

## Cost

| Operation | Cost |
|-----------|------|
| Constructor | One mutex lock + one vector index write |
| Destructor | One mutex lock + one vector index write |
| Crash-time read | One mutex `try_lock` + one string-stream pass |

Default per-thread capacity is 1000 entries; the vector grows past that on
demand. Memory overhead is roughly `1000 × sizeof(void*)` per thread that
ever uses StackWatcher.

## When to add STACK_WATCHER_FUNC

Add it to:

- Functions that appear in your crash reports' DbgHelp section as
  `<unknown>` (typically optimised-away or symbol-stripped functions).
- Hot entry points: `main`, message-loop dispatchers, command handlers.
- Boundaries between subsystems where the trail back to "what was the
  application doing?" matters.

Do **not** add it to:

- Hot inner loops or trivial getters/setters — the lock is cheap but not
  free.
- Constructors/destructors of small RAII helpers.

## Thread safety

All operations are thread-safe. Each thread has its own per-thread stack
container, accessed through the global mutex. During crash handling the
mutex is acquired with `try_lock` plus a short retry, so a thread that
already holds the mutex when it crashes does not deadlock the reporter.
