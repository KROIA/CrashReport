#include "ExceptionHandler.h"
#include "StackWatcher.h"
#include "CrashReport_info.h"
#include <fstream>
#include <sstream>
#include <signal.h>
#include <new>
#include <malloc.h>   // for _resetstkoflw
#include <DbgHelp.h>
#include <heapapi.h>  // for HeapSetInformation
#include <iomanip> // for std::put_time
#include <ctime>
#include <chrono>


// Define the maximum stack frames to capture
#define MAX_FRAMES 100

namespace CrashReport
{
	static LONG WINAPI ExceptionHandlerCallback(EXCEPTION_POINTERS* pExceptionPointers);
	static int s_abortSignal = 0;
	volatile long ExceptionHandler::s_crashFlag = 0;
	volatile long ExceptionHandler::s_insideCrashHandler = 0;

	// Custom exception code for signal-based crashes (so SEH filter can catch them)
	#define CRASH_REPORT_SIGNAL_EXCEPTION 0xE0000001

	// Helper that raises a benign SEH exception so the SEH path captures
	// EXCEPTION_POINTERS for a minidump. Used by onTerminate when we don't
	// already have exception pointers (signal/terminate path).
	static int generateDumpFromCurrentContextFilter(EXCEPTION_POINTERS* p, ExceptionHandler* inst);
	static void generateDumpFromCurrentContext(ExceptionHandler* inst)
	{
		__try {
			RaiseException(CRASH_REPORT_SIGNAL_EXCEPTION, 0, 0, nullptr);
		}
		__except (generateDumpFromCurrentContextFilter(GetExceptionInformation(), inst)) {
			// dump generated in filter
		}
	}

	extern "C" void abortHandle(int signal_number)
	{
		s_abortSignal = signal_number;
		// Re-raise as SEH exception so the unhandled exception filter is called
		// (which captures EXCEPTION_POINTERS for the minidump).
		ULONG_PTR args[1] = { static_cast<ULONG_PTR>(signal_number) };
		RaiseException(CRASH_REPORT_SIGNAL_EXCEPTION, 0, 1, args);
		// If RaiseException returns (it shouldn't for unhandled), fall through to terminate.
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
		static void generateCrashDump(ExceptionHandler* inst, EXCEPTION_POINTERS* p)
		{
			inst->generateCrashDump(p);
		}
		static void markInsideCrashHandler()
		{
			InterlockedExchange(&ExceptionHandler::s_insideCrashHandler, 1);
		}
		static ExceptionHandler& getInstance()
		{
			return ExceptionHandler::instance();
		}
	};

	static int generateDumpFromCurrentContextFilter(EXCEPTION_POINTERS* p, ExceptionHandler* inst)
	{
		ExeptionHandlerInternal::generateCrashDump(inst, p);
		return EXCEPTION_EXECUTE_HANDLER;
	}


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
		// Reserve 256KB of guaranteed stack for the SEH filter so that
		// stack overflow crashes can run the handler (DbgHelp + iostreams
		// are stack-hungry) with sufficient room.
		ULONG stackGuarantee = 256 * 1024;
		SetThreadStackGuarantee(&stackGuarantee);

		// Force termination-on-corruption so heap corruption raises a clean
		// STATUS_HEAP_CORRUPTION exception that the SEH filter can catch.
		HeapSetInformation(GetProcessHeap(),
			HeapEnableTerminationOnCorruption,
			nullptr, 0);

		// Windows Structured Exception Handling.
		// SetUnhandledExceptionFilter runs only when no handler in the chain
		// caught the exception. Some host runtimes (e.g. CLR, certain CRT
		// versions) install handlers that catch stack overflow first, so we
		// also register a Vectored Exception Handler which runs *before*
		// any SEH frame.
		SetUnhandledExceptionFilter(ExceptionHandlerCallback);
		AddVectoredExceptionHandler(1, [](EXCEPTION_POINTERS* p) -> LONG {
			// Only handle stack overflow here; everything else continues
			// through the normal SEH chain to SetUnhandledExceptionFilter.
			if (p && p->ExceptionRecord &&
				p->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW)
			{
				_resetstkoflw();
				ExeptionHandlerInternal::onException(p);
				ExitProcess(1);
			}
			return EXCEPTION_CONTINUE_SEARCH;
		});

		// Signal handlers (work alongside SEH on Windows)
		signal(SIGINT, &abortHandle);
		signal(SIGILL, &abortHandle);
		signal(SIGFPE, &abortHandle);
		signal(SIGSEGV, &abortHandle);
		signal(SIGTERM, &abortHandle);
		signal(SIGBREAK, &abortHandle);
		signal(SIGABRT, &abortHandle);
		signal(SIGABRT_COMPAT, &abortHandle);

		// C++ exception handlers
		std::set_terminate(&ExceptionHandler::onTerminate);

		// Windows-specific handlers
		_set_purecall_handler(&ExceptionHandler::onPureCall);
		_set_invalid_parameter_handler(&ExceptionHandler::onInvalidParameter);
		std::set_new_handler(&ExceptionHandler::onNewFailure);
		// Note: Buffer security checks (/GS) are enabled by default in MSVC

		// Disable Windows error dialog boxes
		SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

		ExceptionHandler& handler = ExceptionHandler::instance();
		handler.m_crashExportPath = crashExportPath;
	}

	void ExceptionHandler::setExceptionCallback(ExceptionCallback callback)
	{
		ExceptionHandler::instance().m_exeptionCallback = callback;
	}
	void ExceptionHandler::terminate()
	{
		ExceptionHandler::onTerminate();
	}

	bool ExceptionHandler::tryAcquireCrashLock()
	{
		// Use InterlockedCompareExchange to ensure only one thread handles the crash
		return InterlockedCompareExchange(&s_crashFlag, 1, 0) == 0;
	}

	// Context shared with worker thread used for stack-overflow handling.
	struct StackOverflowWorkerCtx
	{
		EXCEPTION_POINTERS* pExceptionPointers;
		HANDLE doneEvent;
	};

	static DWORD WINAPI stackOverflowWorker(LPVOID param)
	{
		auto* ctx = static_cast<StackOverflowWorkerCtx*>(param);
		ExceptionHandler& inst = ExeptionHandlerInternal::getInstance();
		ExeptionHandlerInternal::generateCrashDump(&inst, ctx->pExceptionPointers);
		// Mark already-in-handler so onTerminate doesn't try to generate another dump
		ExeptionHandlerInternal::markInsideCrashHandler();
		SetEvent(ctx->doneEvent);
		// onTerminate doesn't return; it calls exit().
		ExceptionHandler::terminate();
		return 0;
	}

	void ExceptionHandler::onException(EXCEPTION_POINTERS* pExceptionPointers)
	{
		//STACK_WATCHER_FUNC;

		// Ensure only one thread processes the crash
		if (!tryAcquireCrashLock())
		{
			// Another thread is already handling a crash, wait indefinitely
			Sleep(INFINITE);
			return;
		}

		// Stack overflow needs a fresh thread with a clean stack — DbgHelp
		// and iostreams use too much stack to run reliably on the faulting thread.
		const bool isStackOverflow = pExceptionPointers &&
			pExceptionPointers->ExceptionRecord &&
			pExceptionPointers->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW;

		if (isStackOverflow)
		{
			StackOverflowWorkerCtx ctx;
			ctx.pExceptionPointers = pExceptionPointers;
			ctx.doneEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
			HANDLE hThread = CreateThread(nullptr, 0, stackOverflowWorker, &ctx, 0, nullptr);
			if (hThread)
			{
				WaitForSingleObject(hThread, INFINITE);
				CloseHandle(hThread);
			}
			if (ctx.doneEvent) CloseHandle(ctx.doneEvent);
			// If the worker exited, it already called exit(). Belt and braces:
			ExitProcess(1);
		}

		ExceptionHandler& inst = ExceptionHandler::instance();
		inst.generateCrashDump(pExceptionPointers);

		// Mark that we're inside crash handler so onTerminate won't try to acquire lock again
		InterlockedExchange(&s_insideCrashHandler, 1);
		onTerminate();
	}
	[[noreturn]]
	void ExceptionHandler::onTerminate()
	{
		//STACK_WATCHER_FUNC;

		// Only try to acquire lock if we're not already inside the crash handler
		bool alreadyInHandler = (InterlockedCompareExchange(&s_insideCrashHandler, 1, 1) == 1);
		if (!alreadyInHandler && !tryAcquireCrashLock())
		{
			// Another thread is already handling a crash, wait indefinitely
			Sleep(INFINITE);
		}

		ExceptionHandler& inst = ExceptionHandler::instance();

		// If we're not coming from onException, generate a dump using current context.
		if (!alreadyInHandler)
		{
			generateDumpFromCurrentContext(&inst);
			InterlockedExchange(&s_insideCrashHandler, 1);
		}

		if (inst.m_exeptionCallback)
		{
			try {
				(*inst.m_exeptionCallback)();
			}
			catch (...) {
				// Prevent callback exceptions from interfering with crash handling
			}
		}

		std::exception_ptr exptr = std::current_exception();
		if (exptr)
		{
			try {
				std::rethrow_exception(exptr);
			}
			catch (std::exception& ex) {
				std::fprintf(stderr, "Terminated due to exception: %s\n", ex.what());
			}
			catch (...) {
				std::fprintf(stderr, "Terminated due to unknown exception\n");
			}
		}

		inst.saveStackTrace();

		// Flush all output streams
		std::fflush(stdout);
		std::fflush(stderr);
		std::cout.flush();
		std::cerr.flush();

		exit(1);
	}

	void ExceptionHandler::onPureCall()
	{
		if (!tryAcquireCrashLock())
		{
			Sleep(INFINITE);
		}

		std::fprintf(stderr, "Pure virtual function call detected\n");
		ExceptionHandler& inst = ExceptionHandler::instance();
		generateDumpFromCurrentContext(&inst);
		InterlockedExchange(&s_insideCrashHandler, 1);
		onTerminate();
	}

	void ExceptionHandler::onInvalidParameter(const wchar_t* expression, const wchar_t* function,
		const wchar_t* file, unsigned int line, uintptr_t pReserved)
	{
		(void)expression;
		(void)file;
		(void)line;
		(void)pReserved;

		if (!tryAcquireCrashLock())
		{
			Sleep(INFINITE);
		}

		std::fprintf(stderr, "Invalid parameter detected in function %S\n", function ? function : L"unknown");
		ExceptionHandler& inst = ExceptionHandler::instance();
		generateDumpFromCurrentContext(&inst);
		InterlockedExchange(&s_insideCrashHandler, 1);
		onTerminate();
	}

	void ExceptionHandler::onNewFailure()
	{
		if (!tryAcquireCrashLock())
		{
			Sleep(INFINITE);
		}

		std::fprintf(stderr, "Memory allocation failure: new handler called\n");
		ExceptionHandler::instance().saveStackTrace();
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

	LONG WINAPI ExceptionHandlerCallback(EXCEPTION_POINTERS* pExceptionPointers)
	{
		// Stack overflow needs special handling: reset the guard page so
		// the handler can use the stack reserved by SetThreadStackGuarantee.
		if (pExceptionPointers &&
			pExceptionPointers->ExceptionRecord &&
			pExceptionPointers->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW)
		{
			_resetstkoflw();
		}

		// Generate crash dump and handle the exception
		ExeptionHandlerInternal::onException(pExceptionPointers);
		// This line is never reached due to exit() in onException
		return EXCEPTION_EXECUTE_HANDLER;
	}
}