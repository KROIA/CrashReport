#pragma once
#include "CrashReport_base.h"
#include <string>
#include <windows.h>

namespace CrashReport
{
	class ExeptionHandlerInternal;
	class CRASH_REPORT_EXPORT ExceptionHandler
	{
		friend ExeptionHandlerInternal;
		ExceptionHandler();
		~ExceptionHandler();

		static ExceptionHandler& instance();
	public:
		typedef void (*ExceptionCallback)(void);

		static void setup(const std::string& crashExportPath);
		static void setExceptionCallback(ExceptionCallback callback);

		
	private:
		static void onException(EXCEPTION_POINTERS* pExceptionPointers);
		static void onTerminate();

		

		void generateCrashDump(EXCEPTION_POINTERS* pExceptionPointers);
		void saveStackTrace();
		std::string getExeName() const;

		std::string m_crashExportPath;
		ExceptionCallback m_exeptionCallback = nullptr;
	};
}