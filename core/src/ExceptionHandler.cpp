#include "ExceptionHandler.h"
#include "StackWatcher.h"
#include "CrashReport_info.h"
#include <fstream>
#include <sstream>
#include <signal.h>
#include <DbgHelp.h>
#include <iomanip> // for std::put_time
#include <ctime>
#include <chrono>


// Define the maximum stack frames to capture
#define MAX_FRAMES 100

namespace CrashReport
{
	static LONG WINAPI ExceptionHandlerCallback(EXCEPTION_POINTERS* pExceptionPointers);
	static int s_abortSignal = 0;

	extern "C" void abortHandle(int signal_number)
	{
		s_abortSignal = signal_number;
		ExceptionHandler::terminate();
	}

	static const char* abortSignalToString(int signal)
	{
		switch (signal)
		{
		case SIGINT: return "SIGINT: interrupt";
		case SIGILL: return "SIGILL: illegal instruction - invalid function image";
		case SIGFPE: return "SIGFPE: floating point exception";
		case SIGSEGV: return "SIGSEGV: segment violation";
		case SIGTERM: return "SIGTERM: Software termination signal from kill";
		case SIGBREAK: return "SIGBREAK: Ctrl-Break sequence";
		case SIGABRT: return "SIGABRT: abnormal termination triggered by abort call";
		case SIGABRT_COMPAT: return "SIGABRT_COMPAT: SIGABRT compatible with other platforms, same as SIGABRT";
		}
		return "Unknown signal";
	}

	static std::string getCurrentDateTime() 
	{
		// Get current time
		auto now = std::chrono::system_clock::now();
		std::time_t currentTime = std::chrono::system_clock::to_time_t(now);

		// Convert current time to local time
		std::tm localTime;
		localtime_s(&localTime, &currentTime);

		// Format the time as a string
		std::ostringstream oss;
		oss << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");

		return oss.str();
	}

	/// <summary>
	/// Calls the private function of the public class
	/// </summary>
	class ExeptionHandlerInternal
	{
	public:
		static void onException(EXCEPTION_POINTERS* pExceptionPointers)
		{
			ExceptionHandler::onException(pExceptionPointers);
		}
	};


	ExceptionHandler::ExceptionHandler()
		: m_exeptionCallback(nullptr)
	{

	}
	ExceptionHandler::~ExceptionHandler()
	{

	}

	ExceptionHandler& ExceptionHandler::instance()
	{
		static ExceptionHandler instance;
		return instance;
	}

	void ExceptionHandler::setup(const std::string& crashExportPath)
	{
		SetUnhandledExceptionFilter(ExceptionHandlerCallback);
		signal(SIGINT, &abortHandle);
		signal(SIGILL, &abortHandle);
		signal(SIGFPE, &abortHandle);
		signal(SIGSEGV, &abortHandle);
		signal(SIGTERM, &abortHandle);
		signal(SIGBREAK, &abortHandle);
		signal(SIGABRT, &abortHandle);
		signal(SIGABRT_COMPAT, &abortHandle);

		ExceptionHandler& handler = ExceptionHandler::instance();
		handler.m_crashExportPath = crashExportPath;

		// Set custom terminate handler
		std::set_terminate(&ExceptionHandler::onTerminate);
	}

	void ExceptionHandler::setExceptionCallback(ExceptionCallback callback)
	{
		ExceptionHandler::instance().m_exeptionCallback = callback;
	}
	void ExceptionHandler::terminate()
	{
		ExceptionHandler::onTerminate();
	}

	void ExceptionHandler::onException(EXCEPTION_POINTERS* pExceptionPointers)
	{
		//STACK_WATCHER_FUNC;
		ExceptionHandler& inst = ExceptionHandler::instance();

		
		inst.generateCrashDump(pExceptionPointers);
		

		onTerminate();
	}
	[[noreturn]]
	void ExceptionHandler::onTerminate()
	{
		//STACK_WATCHER_FUNC;
		ExceptionHandler& inst = ExceptionHandler::instance();
		
		if (inst.m_exeptionCallback)
			(*inst.m_exeptionCallback)();

		std::exception_ptr exptr = std::current_exception();
		try {
			std::rethrow_exception(exptr);
		}
		catch (std::exception& ex) {
			std::fprintf(stderr, "Terminated due to exception: %s\n", ex.what());
		}

		inst.saveStackTrace();

		
		exit(1);
	}

	void ExceptionHandler::generateCrashDump(EXCEPTION_POINTERS* pExceptionPointers)
	{
		std::string exeName = m_crashExportPath+"\\"+getExeName() + ".dmp";
		LPSTR lpSTR_exeName = const_cast<char*>(exeName.c_str());
		std::wstring wPath = std::wstring(m_crashExportPath.begin(), m_crashExportPath.end());
		// Attempt to create the directory
		CreateDirectoryW(wPath.c_str(), NULL);

		// For Windows
		HANDLE hDumpFile = CreateFile(lpSTR_exeName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hDumpFile != INVALID_HANDLE_VALUE) {
			MINIDUMP_EXCEPTION_INFORMATION minidumpInfo;
			minidumpInfo.ThreadId = GetCurrentThreadId();
			minidumpInfo.ExceptionPointers = (EXCEPTION_POINTERS*)pExceptionPointers;
			minidumpInfo.ClientPointers = FALSE;

			// Write the dump
			MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile, MiniDumpNormal, &minidumpInfo, NULL, NULL);

			CloseHandle(hDumpFile);
		}
	}
	void ExceptionHandler::saveStackTrace()
	{
		//STACK_WATCHER_FUNC;
		std::string filePath = m_crashExportPath + "\\" + getExeName() + "_stack.txt";
		std::wstring wPath = std::wstring(m_crashExportPath.begin(), m_crashExportPath.end());
		// Attempt to create the directory
		CreateDirectoryW(wPath.c_str(), NULL);
		std::stringstream stream;

		// Print library information
		LibraryInfo::printInfo(stream);

		stream << "\n\n";
		stream << "Timestamp: " << getCurrentDateTime() << "\n";
		stream << "An exception caused a crash.\n";
		stream << "Crashed thread ID: "<< GetCurrentThreadId() << "\n";
		
		if(s_abortSignal != 0)
			stream << "Abort signal: "<< s_abortSignal << " " << abortSignalToString(s_abortSignal) << "\n";
		stream << "\n";
		

		void* stack[MAX_FRAMES];
		HANDLE process = GetCurrentProcess();
		SymInitialize(process, NULL, TRUE);

		// Capture the current stack
		WORD frames = CaptureStackBackTrace(0, MAX_FRAMES, stack, NULL);

		// Retrieve symbols for each frame
		SYMBOL_INFO* symbol = (SYMBOL_INFO*)malloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char));
		if (symbol)
		{
			symbol->MaxNameLen = 255;
			symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

			stream << "DbgHelp stack content: \n";

			for (WORD i = 0; i < frames; i++) {
				DWORD64 address = (DWORD64)(stack[frames-i-1]);
				DWORD64 returnAddress = 0;
				// On x86, the return address is stored one word (4 bytes) above the current frame pointer (ESP)
				// On x64, the return address is stored in the first 8 bytes above the current frame pointer (RSP)
#ifdef _WIN64
				returnAddress = *((DWORD64*)stack[i] + 1);
#else
				returnAddress = *((DWORD*)stack[i] + 1);
#endif

				SymFromAddr(process, address, 0, symbol);
				int length = 50;
				std::string symName = symbol->Name;
				if (symName.size() < length)
					length = length - symName.size();
				stream << "    [" << i << "]\t" << symName << std::string(length, ' ') << " Return address: " << std::hex << returnAddress << std::dec << std::endl;
				//printf("Frame %d: %s Return address: 0x%llX\n", i, symbol->Name, returnAddress);
			}
		}

		stream << "\nStackWatcher content: \n" << StackWatcher::toString();
		

		free(symbol);
		SymCleanup(process);

		std::cout << stream.str();

		std::ofstream outFile(filePath);
		if (!outFile.is_open())
			return;

		outFile << stream.str();

		outFile.close();
	}
	std::string ExceptionHandler::getExeName() const
	{
		char path[MAX_PATH];
		GetModuleFileName(NULL, path, MAX_PATH);
		std::string executableName = path;
		size_t lastSlashPos = executableName.find_last_of("\\");
		if (lastSlashPos != std::string::npos) {
			executableName = executableName.substr(lastSlashPos + 1);
		}
		return executableName;
	}

	//[[noreturn]]
	LONG WINAPI ExceptionHandlerCallback(EXCEPTION_POINTERS* pExceptionPointers)
	{
		//STACK_WATCHER_FUNC;
		// Generate crash dump
		ExeptionHandlerInternal::onException(pExceptionPointers);
		//exit(1);
		return 1;
	}
}