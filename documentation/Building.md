# Building

The library uses CMake (≥ 3.20) and is set up for the Visual Studio 2022
generator out of the box. Other generators (Ninja, NMake) work as well.

## Prerequisites

| Requirement | Tested with |
|-------------|-------------|
| Windows | 10 build 19045 (should work on 7 SP1+) |
| Visual Studio | 2022 Community / Professional / Enterprise (17.x) |
| CMake | 3.20+ (3.31 ships with VS 2022) |
| Architecture | x64 (x86 also works; ARM64 untested) |

DbgHelp.dll and Psapi.lib are part of Windows; nothing extra to install.

## One-liner

```cmd
cmake -B build -G "Visual Studio 17 2022" -A x64 && cmake --build build --config Release
```

## Step by step

```cmd
:: Configure
cmake -B build -G "Visual Studio 17 2022" -A x64

:: Build (Release)
cmake --build build --config Release

:: Build (Debug)
cmake --build build --config Debug
```

## Output layout

```
build/
  CrashTestRunner.exe       <- test runner binary
  CrashTestRunner.pdb       <- symbols for source/line info in reports
  Release/
    CrashReport.dll         <- shared library
    CrashReport.lib         <- import lib for the shared library
    CrashReport-s.lib       <- static library
    LibraryExample.exe      <- minimal usage example
    ExampleTest.exe         <- unit tests
    ...
```

## Build options

CMake cache options (set with `-D<name>=<value>`):

| Option | Default | Purpose |
|--------|---------|---------|
| `COMPILE_EXAMPLES` | `ON` | Build `examples/` (LibraryExample, CrashTestRunner). |
| `COMPILE_UNITTESTS` | `ON` | Build `unittests/` (ExampleTest). |
| `INSTALL_PREFIX` | `<src>/installation` | Where `cmake --install` puts artefacts. |
| `QT_ENABLE` | `OFF` | Enable Qt-dependent helpers (info widget). |

## Installing

```cmd
cmake --install build --config Release
```

Installs into `installation/` (or wherever `INSTALL_PREFIX` points):

```
installation/
  bin/   <- DLLs and EXEs
  lib/   <- import + static libraries
  include/CrashReport/   <- public headers
```

## Linking against the library from another CMake project

```cmake
find_package(CrashReport REQUIRED CONFIG
    PATHS "C:/path/to/installation"
)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE CrashReport_shared)
```

Both `CrashReport_shared` and `CrashReport_static` targets are exported.
Pick one — they pull in `DbgHelp.lib` and `Psapi.lib` transitively.

## Troubleshooting

### `cmake` not found

CMake ships inside Visual Studio 2022 at:

```
C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
```

Either add this directory to `PATH` or use the full path explicitly.

### "generator does not match the generator used previously"

The dependency cache (`build/dependencies/cache/`) was populated by a
different generator. Wipe it and re-configure:

```cmd
rmdir /S /Q build\dependencies\cache
cmake -B build -G "Visual Studio 17 2022" -A x64
```

### "warning C4100: unreferenced formal parameter" treated as error

The library is built with `/W4 /WX` by default. The codebase silences
unused-parameter warnings explicitly with `(void)param;`. If you see
unrelated warnings-as-errors, check that you are using the supported MSVC
version.

### Symbols show as `<unknown>` in crash reports

DbgHelp could not find the matching `.pdb`. Check:

1. The `.pdb` exists next to or in the parent directory of the `.exe`.
2. The `.exe` was not stripped of debug info.
3. `_NT_SYMBOL_PATH` (if set) does not point at an unreachable symbol
   server.

## Cleaning

A complete reset:

```cmd
rmdir /S /Q build
rmdir /S /Q installation
```

Then re-run the configure + build commands above.
