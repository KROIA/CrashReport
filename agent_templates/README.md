# Agent Templates - CrashReport Library

This folder contains agent instructions and templates for AI coding agents working on the **CrashReport** library.

## Quick Start

**For AI Agents**: Read `CLAUDE.md` in this folder - it contains everything you need to work on this library.

## Files

- **CLAUDE.md** - Complete instructions for AI agents (library structure, conventions, API reference, build instructions)
- **README.md** - This file (overview of the agent_templates system)

## Purpose

This folder enables AI agents to:
- Work on this library **independently** from other libraries in the parent folder
- Have complete context without loading documentation for all libraries
- Understand the template-based structure and USER_SECTION markers
- Follow library-specific conventions and build procedures

## Template System

This library is based on the **QT_cmake_library_template v1.6.1**. The master template documentation is at:
`C:\Users\Alex\Documents\Visual Studio 2022\Projects\QT_cmake_library_template\AI_Knowledge.md`

## Key Concepts

### USER_SECTION Markers
All custom code must go inside USER_SECTION blocks:
```cpp
/// USER_SECTION_START 1
// Your code here - safe from template upgrades
/// USER_SECTION_END
```

**Never** remove, add, or renumber these markers!

### Build Profiles
This library produces three build variants:
1. **CrashReport_shared** - Shared library (.dll)
2. **CrashReport_static** - Static library
3. **CrashReport_static_profile** - Static with profiling (if easy_profiler is present)

### Auto-Globbed Files
Files in `core/inc/` and `core/src/` are automatically included - no need to edit CMakeLists.txt. Just **reconfigure CMake** after adding new files.

## For More Information

- **This library's complete guide**: Read `CLAUDE.md` in this folder
- **Template documentation**: `../../Projects/QT_cmake_library_template/AI_Knowledge.md`
- **System overview**: `../AGENT_TEMPLATES_README.md` (parent folder)

---

**Created**: 2026-04-30  
**Template Version**: 1.6.1
