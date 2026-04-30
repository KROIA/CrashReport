# CrashReport Library - AI Agent Instructions

> **Agent Context**: This file provides all essential information for AI coding agents working on the CrashReport library. Read this file once to understand the project structure and conventions.

---

## 1. Library Identity

This is the **CrashReport** library, generated from the KROIA QT_cmake_library_template (v1.6.1).

**Key identifiers** (placeholders that were replaced during generation):
- **Library Name**: `CrashReport`
- **Namespace**: (Check `core/inc/CrashReport_base.h` for the actual namespace)
- **Export Macro**: `CRASHREPORT_API`
- **Library Define**: `CRASHREPORT_LIB`
- **Profiling Define**: `CR_PROFILING`
- **Short Prefix**: `CR_` (used in macros like `CR_CONSOLE`, `CR_PROFILING_*`)

---

## 2. Template Reference

This library is based on the **QT_cmake_library_template**. Complete template documentation is available at:
- **Template location**: `C:\Users\Alex\Documents\Visual Studio 2022\Projects\QT_cmake_library_template\AI_Knowledge.md`
- **Template repo**: https://github.com/KROIA/QT_cmake_library_template
- **Template version**: 1.6.1

**IMPORTANT**: Before making significant changes, read the template's AI_Knowledge.md to understand:
- The three-build-profile system (shared, static, static_profile)
- USER_SECTION marker rules
- Dependency management system
- Build system internals

---

## 3. Critical Conventions for AI Agents

### 3.1 USER_SECTION Markers - NEVER BREAK THESE RULES
```cpp
/// USER_SECTION_START 1
// Your code here
/// USER_SECTION_END
```

**Rules**:
1. **NEVER** remove, renumber, or add new USER_SECTION markers
2. **ALL custom code MUST go inside USER_SECTION blocks**
3. Code outside USER_SECTION blocks will be **overwritten** during template upgrades
4. To "delete" template code, use the comment-wrap pattern:
   ```cpp
   /// USER_SECTION_START 4
   /*
   /// USER_SECTION_END
   ... template code you want to disable ...
   /// USER_SECTION_START 5
   */
   /// USER_SECTION_END
   ```
5. **`///` (triple-slash) is RESERVED for USER_SECTION markers only** - use `//` for normal comments

### 3.2 Files Auto-Managed by CMake
- **NEVER** manually add source files to CMakeLists.txt - they're auto-globbed
- After creating new `.h` or `.cpp` files, **reconfigure CMake** to pick them up
- All files in `core/inc/` and `core/src/` are automatically included (recursive)

### 3.3 Editing Version and Settings
- **NEVER** edit `CrashReport_meta.h` (it's auto-generated)
- Change version in root `CMakeLists.txt` line 36: `set(LIBRARY_VERSION "X.Y.Z")`
- Lines marked `# <AUTO_REPLACED>` are managed by CmakeLibCreator tool

---

## 4. Project Structure

```
CrashReport/
├── CMakeLists.txt              ← Root config (LIBRARY_NAME, VERSION, etc.)
├── cmake/                      ← Build utilities
│   ├── utility.cmake           ← Helper functions
│   ├── dependencies.cmake      ← Dependency loader
│   ├── ExampleMaster.cmake     ← Example/test builder
│   └── QtLocator.cmake         ← Qt5/Qt6 detection
├── core/                       ← THE LIBRARY
│   ├── CMakeLists.txt          ← Creates 3 build targets
│   ├── inc/                    ← Public headers (auto-globbed)
│   │   ├── CrashReport.h            ← Single public include
│   │   ├── CrashReport_base.h       ← Include in all your headers
│   │   ├── CrashReport_global.h     ← DLL import/export macros
│   │   ├── CrashReport_debug.h      ← Logging, profiling macros
│   │   ├── CrashReport_info.h       ← LibraryInfo class
│   │   └── CrashReport_meta.h.in    ← Version template (DO NOT EDIT .h)
│   ├── src/                    ← Implementation files (auto-globbed)
│   └── resources/              ← .qrc files for Qt resources
├── dependencies/               ← Dependency .cmake files
│   ├── order.cmake             ← Load order for dependencies
│   └── *.cmake                 ← One file per dependency
├── Examples/                   ← Example applications
└── unitTests/                  ← Unit test executables
```

---

## 5. Common Tasks

### 5.1 Adding a New Class

1. **Create files** in `core/inc/` and `core/src/`:
   ```cpp
   // MyClass.h
   #pragma once
   #include "CrashReport_base.h"

   namespace YOUR_NAMESPACE {
       class CRASHREPORT_API MyClass {
       public:
           MyClass();
           void doSomething();
       };
   }
   ```

2. **Expose in public header** (`core/inc/CrashReport.h`):
   ```cpp
   /// USER_SECTION_START 2
   #include "MyClass.h"
   /// USER_SECTION_END
   ```

3. **Reconfigure CMake** (files are auto-globbed but need refresh)

### 5.2 Adding a Dependency

1. Create `dependencies/NewDep.cmake` (copy from `DependencyTemplate.cmake`)
2. Implement the `dep()` function with Git URL and settings
3. Delete CMake cache and reconfigure
4. Use the availability macro in code:
   ```cpp
   #if NEWDEP_LIBRARY_AVAILABLE == 1
   #include "newdep/header.h"
   #endif
   ```

### 5.3 Building

**Command line**:
```bash
./build.bat                    # Builds Debug + Release
```

**CMake presets**:
```bash
cmake --preset x64-Debug       # or x64-Release, x64-Debug-Profile
cmake --build --preset x64-Debug
```

**Visual Studio**: Open folder → Select configuration → Build → Install

---

## 6. What NOT to Do

❌ **Never** edit `CrashReport_meta.h` directly (edit the `.in` file or change `LIBRARY_VERSION` in root CMakeLists.txt)
❌ **Never** remove or modify `# <AUTO_REPLACED>` comments
❌ **Never** add/remove/renumber USER_SECTION markers
❌ **Never** use `///` for regular comments (reserved for USER_SECTION markers)
❌ **Never** manually add files to CMakeLists.txt (they're auto-globbed)
❌ **Never** modify code outside USER_SECTION blocks (it will be overwritten on upgrades)

---

## 7. Build Targets

This library produces **three build variants**:
1. **`CrashReport_shared`** - Shared library (.dll) - `CRASHREPORT_LIB` + `BUILD_SHARED` defined
2. **`CrashReport_static`** - Static library (-s postfix) - `CRASHREPORT_LIB` + `BUILD_STATIC` defined
3. **`CrashReport_static_profile`** - Static + profiling (-s-p postfix) - Only if easy_profiler dependency exists

Examples and tests link against the static variant.

---

## 8. Available Macros and API

### Debug/Console Macros (active only in Debug builds):
- `CR_CONSOLE(msg)` - Print to console
- `CR_CONSOLE_FUNCTION(msg)` - Print with function name
- `CR_DEBUG` - Defined in Debug builds only

### Profiling Macros (active only in static_profile build):
- `CR_PROFILING_BLOCK(text, colorStage)`
- `CR_PROFILING_FUNCTION(colorStage)`
- `CR_PROFILING_NONSCOPED_BLOCK(text, colorStage)` + `CR_PROFILING_END_BLOCK`
- `CR_PROFILING_VALUE(name, value)`

### LibraryInfo Class (in `CrashReport_info.h`):
```cpp
LibraryInfo::version          // Version struct (major, minor, patch)
LibraryInfo::name, author, email, website, license
LibraryInfo::printInfo()      // Print to console
LibraryInfo::getInfoStr()     // Get as string
LibraryInfo::createInfoWidget(parent)  // Qt widget (if Qt enabled)
```

### Logger Integration (if Logger dependency is present):
```cpp
#if LOGGER_LIBRARY_AVAILABLE == 1
Logger::logInfo("message");
Logger::logWarning("message");
Logger::logError("message");
#endif
```

---

## 9. Agent Working Principles

### Context Management
- This library is **independent** of other libraries in the parent folder
- Each library has its own agent_templates to save context
- Focus only on this library's code in `core/`, `Examples/`, `unitTests/`
- Reference template documentation when needed, but don't load it all into context

### Making Changes
1. **Always** check if code belongs in a USER_SECTION before editing
2. **Always** preserve USER_SECTION markers exactly as they are
3. **Reconfigure** CMake after adding new files
4. **Test** by building all three profiles (shared, static, static_profile)

### Documentation Updates
- Update this CLAUDE.md if you discover library-specific patterns
- Document the actual namespace used in this library (check `CrashReport_base.h`)
- Note any custom dependencies or special build requirements

---

## 10. Quick Reference

| Need to... | Do this... |
|------------|-----------|
| Add new class | Create in `core/inc/` + `core/src/`, add to `CrashReport.h` USER_SECTION 2, reconfigure |
| Change version | Edit `LIBRARY_VERSION` in root CMakeLists.txt line 36 |
| Add dependency | Create `dependencies/NewDep.cmake`, delete cache, reconfigure |
| Build | Run `build.bat` or use CMake presets |
| Add public API | Mark class with `CRASHREPORT_API`, include `CrashReport_base.h` |
| Debug print | Use `CR_CONSOLE(msg)` (Debug only) |
| Profile code | Use `CR_PROFILING_*` macros in static_profile build |
| Check template docs | Read `C:\Users\Alex\Documents\Visual Studio 2022\Projects\QT_cmake_library_template\AI_Knowledge.md` |

---

**Last Updated**: 2026-04-30
**Template Version**: 1.6.1
**Agent**: This file is for AI agents - keep it updated with library-specific discoveries.
