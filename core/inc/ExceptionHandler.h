#pragma once
#include "CrashReport_base.h"
#include <string>
#include <windows.h>

namespace CrashReport
{
	class ExeptionHandlerInternal;
	class CRASH_REPORT_API ExceptionHandler
	{
		friend ExeptionHandlerInternal;
		ExceptionHandler();
		~ExceptionHandler();

		static ExceptionHandler& instance();
	public:
		typedef void (*ExceptionCallback)(void);

		static void setup(const std::string& crashExportPath);
		static void setExceptionCallback(ExceptionCallback callback);

		static void terminate();
	private:
		static void onException(EXCEPTION_POINTERS* pExceptionPointers);
		static void onTerminate();
		static void onPureCall();
		static void onInvalidParameter(const wchar_t* expression, const wchar_t* function,
			const wchar_t* file, unsigned int line, uintptr_t pReserved);
		static void onNewFailure();

		void generateCrashDump(EXCEPTION_POINTERS* pExceptionPointers);
		void saveStackTrace(EXCEPTION_POINTERS* pExceptionPointers = nullptr);
		std::string getExeName() const;
		static bool tryAcquireCrashLock();

		std::string m_crashExportPath;
		ExceptionCallback m_exeptionCallback = nullptr;
		static volatile long s_crashFlag;
		static volatile long s_insideCrashHandler;
		static EXCEPTION_POINTERS* s_lastExceptionPointers;
	};
}