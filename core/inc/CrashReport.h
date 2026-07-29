// @file CrashReport.h
// @brief Main public header for the library.
//
// Include this single header to access the entire public API.
// Add your own public headers inside USER_SECTION 2 so that
// consumers only need `#include "CrashReport.h"`.
#pragma once

/// USER_SECTION_START 1

/// USER_SECTION_END

#include "CrashReport_info.h"

/// USER_SECTION_START 2
#include "ExceptionHandler.h"
#include "StackWatcher.h"
/// USER_SECTION_END