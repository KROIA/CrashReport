# LibraryInfo

```cpp
#include "CrashReport_info.h"   // pulled in by CrashReport.h

namespace CrashReport {
    class LibraryInfo;
}
```

`LibraryInfo` exposes compile-time metadata about the library: its name,
version, build type, compiler, and build date. Everything is `constexpr`,
so it can be used in both runtime checks and compile-time assertions.

It is also embedded into every crash report (under the
`------ CrashReport Library ------` section) so consumers of the report know
which exact build of the library produced it.

> **Current library version: 1.0.0** (defined as `LIBRARY_VERSION "1.0.0"`
> in the top-level `CMakeLists.txt`).

## Public API

### Members (all `constexpr`)

| Member | Type | Source |
|--------|------|--------|
| `name` | `const char*` | `LIBRARY_NAME` from `CMakeLists.txt` |
| `versionMajor` / `versionMinor` / `versionPatch` | `int` | `LIBRARY_VERSION` from `CMakeLists.txt` |
| `version` | `Version` | aggregate of the three above |
| `author` | `const char*` | hard-coded |
| `email` / `website` / `license` | `const char*` | hard-coded |
| `compilationDate` / `compilationTime` | `const char*` | `__DATE__` / `__TIME__` |
| `compiler` | `const char*` | `MSVC`, `GCC`, `Clang`, or `Unknown` |
| `compilerVersion` | `const char*` | compiler-defined version macro |
| `buildTypeStr` | `const char*` | `Debug` or `Release` |
| `buildType` | `BuildType` enum | `BuildType::debug` or `BuildType::release` |

### Version struct

```cpp
struct Version {
    int major;
    int minor;
    int patch;

    bool operator<(const Version&) const;
    bool operator==(const Version&) const;
    bool operator!=(const Version&) const;
    bool operator>(const Version&) const;
    bool operator<=(const Version&) const;
    bool operator>=(const Version&) const;
    std::string toString() const;
};
```

A semantic-version triple with comparison operators. `toString()` produces
`"<major>.<minor>.<patch>"`.

### printInfo

```cpp
static void printInfo();
static void printInfo(std::ostream& stream);
```

Print every metadata field to `std::cout` (no-arg overload) or to a custom
stream. The crash handler uses the second overload to embed the same block
at the top of `<exe>_stack.txt`.

### getInfoStr

```cpp
static std::string getInfoStr();
```

Return the same multi-line block as `printInfo` but as a `std::string`.

## Example

```cpp
#include "CrashReport.h"

int main()
{
    CrashReport::LibraryInfo::printInfo();

    constexpr auto v = CrashReport::LibraryInfo::version;
    static_assert(v.major >= 1, "this code requires CrashReport >= 1.x");
}
```

Sample output:

```
Library Name: CrashReport
Author: Alex Krieg
Email:
Website:
License: MIT
Version: 01.00.0000
Compilation Date: May  1 2026
Compilation Time: 00:01:30
```

## Notes

- All members are filled at compile time. Updating
  `LIBRARY_NAME`/`LIBRARY_VERSION` in `CMakeLists.txt` requires a
  re-configure + rebuild for the new values to appear.
- `compilationDate` / `compilationTime` reflect the time the
  `CrashReport_info.cpp` translation unit was compiled, not the time the
  application using the library was compiled.
