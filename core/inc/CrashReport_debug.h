#pragma once
#include "CrashReport_global.h"

// The Logger library is automaticly included if the logger dependency .cmake file is available
#if defined(LOGGER) and not defined(LOGGER_LIB)
	#include "Logger.h"
#endif

/// USER_SECTION_START 1

/// USER_SECTION_END

// Debugging
#ifdef NDEBUG
	#define CR_CONSOLE(msg)
	#define CR_CONSOLE_FUNCTION(msg)
#else
	#include <iostream>

	#define CR_DEBUG
	#define CR_CONSOLE_STREAM std::cout

	#define CR_CONSOLE(msg) CR_CONSOLE_STREAM << msg;
	#define CR_CONSOLE_FUNCTION(msg) CR_CONSOLE_STREAM << __PRETTY_FUNCTION__ << " " << msg;
#endif

/// USER_SECTION_START 2

/// USER_SECTION_END

#ifdef CR_PROFILING
	#include "easy/profiler.h"
	#include <easy/arbitrary_value.h> // EASY_VALUE, EASY_ARRAY are defined here

	#define CR_PROFILING_BLOCK_C(text, color) EASY_BLOCK(text, color)
	#define CR_PROFILING_NONSCOPED_BLOCK_C(text, color) EASY_NONSCOPED_BLOCK(text, color)
	#define CR_PROFILING_END_BLOCK EASY_END_BLOCK
	#define CR_PROFILING_FUNCTION_C(color) EASY_FUNCTION(color)
	#define CR_PROFILING_BLOCK(text, colorStage) CR_PROFILING_BLOCK_C(text,profiler::colors::  colorStage)
	#define CR_PROFILING_NONSCOPED_BLOCK(text, colorStage) CR_PROFILING_NONSCOPED_BLOCK_C(text,profiler::colors::  colorStage)
	#define CR_PROFILING_FUNCTION(colorStage) CR_PROFILING_FUNCTION_C(profiler::colors:: colorStage)
	#define CR_PROFILING_THREAD(name) EASY_THREAD(name)

	#define CR_PROFILING_VALUE(name, value) EASY_VALUE(name, value)
	#define CR_PROFILING_TEXT(name, value) EASY_TEXT(name, value)

#else
	#define CR_PROFILING_BLOCK_C(text, color)
	#define CR_PROFILING_NONSCOPED_BLOCK_C(text, color)
	#define CR_PROFILING_END_BLOCK
	#define CR_PROFILING_FUNCTION_C(color)
	#define CR_PROFILING_BLOCK(text, colorStage)
	#define CR_PROFILING_NONSCOPED_BLOCK(text, colorStage)
	#define CR_PROFILING_FUNCTION(colorStage)
	#define CR_PROFILING_THREAD(name)

	#define CR_PROFILING_VALUE(name, value)
	#define CR_PROFILING_TEXT(name, value)
#endif

// Special expantion tecniques are required to combine the color name
#define CONCAT_SYMBOLS_IMPL(x, y) x##y
#define CONCAT_SYMBOLS(x, y) CONCAT_SYMBOLS_IMPL(x, y)



// Different color stages
#define CR_COLOR_STAGE_1 50
#define CR_COLOR_STAGE_2 100
#define CR_COLOR_STAGE_3 200
#define CR_COLOR_STAGE_4 300
#define CR_COLOR_STAGE_5 400
#define CR_COLOR_STAGE_6 500
#define CR_COLOR_STAGE_7 600
#define CR_COLOR_STAGE_8 700
#define CR_COLOR_STAGE_9 800
#define CR_COLOR_STAGE_10 900
#define CR_COLOR_STAGE_11 A100 
#define CR_COLOR_STAGE_12 A200 
#define CR_COLOR_STAGE_13 A400 
#define CR_COLOR_STAGE_14 A700 

namespace CrashReport
{
/// USER_SECTION_START 4

/// USER_SECTION_END
	class CRASH_REPORT_EXPORT Profiler
	{
	public:
		/// USER_SECTION_START 5

		/// USER_SECTION_END

		// Implementation defined in LibraryName_info.cpp to save files.
		static void start();
		static void stop();
		static void stop(const char* profilerOutputFile);

		/// USER_SECTION_START 6

		/// USER_SECTION_END
	};

/// USER_SECTION_START 7

/// USER_SECTION_END


#if defined(LOGGER) and not defined(LOGGER_LIB)
	class CRASH_REPORT_EXPORT Logger 
	{
		/// USER_SECTION_START 8

		/// USER_SECTION_END
		Logger();
		static Logger& instance();
		public:
		/// USER_SECTION_START 9

		/// USER_SECTION_END

		static void setEnabled(bool enable);
		static bool isEnabled();
		static void setName(const std::string& name);
		static std::string getName();
		static void setColor(const Log::Color& col);
		static Log::Color getColor();
		static Log::DateTime getCreationDateTime();
		static Log::LoggerID getID();
		static void setParentID(Log::LoggerID parentID);
		static Log::LoggerID getParentID();



		static void log(const Log::Message& msg);

		static void log(const std::string& msg);
		static void log(const std::string& msg, Log::Level level);
		static void log(const std::string& msg, Log::Level level, const Log::Color& col);

		static void logTrace(const std::string& msg);
		static void logDebug(const std::string& msg);
		static void logInfo(const std::string& msg);
		static void logWarning(const std::string& msg);
		static void logError(const std::string& msg);
		static void logCustom(const std::string& msg);

		/// USER_SECTION_START 10

		/// USER_SECTION_END

		private:
		Log::LogObject m_logObject;

		/// USER_SECTION_START 11

		/// USER_SECTION_END
	};
/// USER_SECTION_START 12

/// USER_SECTION_END
#endif
}


// General
#define CR_GENERAL_PROFILING_COLORBASE Cyan
#define CR_GENERAL_PROFILING_BLOCK_C(text, color) CR_PROFILING_BLOCK_C(text, color)
#define CR_GENERAL_PROFILING_NONSCOPED_BLOCK_C(text, color) CR_PROFILING_NONSCOPED_BLOCK_C(text, color)
#define CR_GENERAL_PROFILING_END_BLOCK CR_PROFILING_END_BLOCK;
#define CR_GENERAL_PROFILING_FUNCTION_C(color) CR_PROFILING_FUNCTION_C(color)
#define CR_GENERAL_PROFILING_BLOCK(text, colorStage) CR_PROFILING_BLOCK(text, CONCAT_SYMBOLS(CR_GENERAL_PROFILING_COLORBASE, colorStage))
#define CR_GENERAL_PROFILING_NONSCOPED_BLOCK(text, colorStage) CR_PROFILING_NONSCOPED_BLOCK(text, CONCAT_SYMBOLS(CR_GENERAL_PROFILING_COLORBASE, colorStage))
#define CR_GENERAL_PROFILING_FUNCTION(colorStage) CR_PROFILING_FUNCTION(CONCAT_SYMBOLS(CR_GENERAL_PROFILING_COLORBASE, colorStage))
#define CR_GENERAL_PROFILING_VALUE(name, value) CR_PROFILING_VALUE(name, value)
#define CR_GENERAL_PROFILING_TEXT(name, value) CR_PROFILING_TEXT(name, value)


/// USER_SECTION_START 3

/// USER_SECTION_END