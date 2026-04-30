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
#include <psapi.h>    // for GetProcessMemoryInfo
#include <iomanip> // for std::put_time
#include <ctime>
#include <chrono>

#pragma comment(lib, "Psapi.lib")


// Define the maximum stack frames to capture
#define MAX_FRAMES 100

namespace CrashReport
{
	static LONG WINAPI ExceptionHandlerCallback(EXCEPTION_POINTERS* pExceptionPointers);
	static int s_abortSignal = 0;
	volatile long ExceptionHandler::s_crashFlag = 0;
	volatile long ExceptionHandler::s_insideCrashHandler = 0;
	EXCEPTION_POINTERS* ExceptionHandler::s_lastExceptionPointers = nullptr;

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

	// Convert Windows exception code to human-readable string.
	static const char* exceptionCodeToString(DWORD code)
	{
		switch (code)
		{
		case EXCEPTION_ACCESS_VIOLATION:         return "EXCEPTION_ACCESS_VIOLATION (memory access fault)";
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
		case EXCEPTION_BREAKPOINT:               return "EXCEPTION_BREAKPOINT";
		case EXCEPTION_DATATYPE_MISALIGNMENT:    return "EXCEPTION_DATATYPE_MISALIGNMENT";
		case EXCEPTION_FLT_DENORMAL_OPERAND:     return "EXCEPTION_FLT_DENORMAL_OPERAND";
		case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
		case EXCEPTION_FLT_INEXACT_RESULT:       return "EXCEPTION_FLT_INEXACT_RESULT";
		case EXCEPTION_FLT_INVALID_OPERATION:    return "EXCEPTION_FLT_INVALID_OPERATION";
		case EXCEPTION_FLT_OVERFLOW:             return "EXCEPTION_FLT_OVERFLOW";
		case EXCEPTION_FLT_STACK_CHECK:          return "EXCEPTION_FLT_STACK_CHECK";
		case EXCEPTION_FLT_UNDERFLOW:            return "EXCEPTION_FLT_UNDERFLOW";
		case EXCEPTION_ILLEGAL_INSTRUCTION:      return "EXCEPTION_ILLEGAL_INSTRUCTION";
		case EXCEPTION_IN_PAGE_ERROR:            return "EXCEPTION_IN_PAGE_ERROR (page fault)";
		case EXCEPTION_INT_DIVIDE_BY_ZERO:       return "EXCEPTION_INT_DIVIDE_BY_ZERO";
		case EXCEPTION_INT_OVERFLOW:             return "EXCEPTION_INT_OVERFLOW";
		case EXCEPTION_INVALID_DISPOSITION:      return "EXCEPTION_INVALID_DISPOSITION";
		case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
		case EXCEPTION_PRIV_INSTRUCTION:         return "EXCEPTION_PRIV_INSTRUCTION";
		case EXCEPTION_SINGLE_STEP:              return "EXCEPTION_SINGLE_STEP";
		case EXCEPTION_STACK_OVERFLOW:           return "EXCEPTION_STACK_OVERFLOW";
		case 0xC0000374:                         return "STATUS_HEAP_CORRUPTION";
		case 0xC0000409:                         return "STATUS_STACK_BUFFER_OVERRUN (/GS check failed)";
		case 0xE06D7363:                         return "EXCEPTION_CXX (unhandled C++ exception)";
		case 0xE0434352:                         return "EXCEPTION_CLR (.NET runtime exception)";
		case 0xE0000001:                         return "CRASH_REPORT_SIGNAL_EXCEPTION (re-raised from signal)";
		default:                                 return "Unknown exception code";
		}
	}

	// Format the access type for an EXCEPTION_ACCESS_VIOLATION.
	static const char* accessTypeToString(ULONG_PTR type)
	{
		switch (type)
		{
		case 0:  return "read";
		case 1:  return "write";
		case 8:  return "DEP (data execution prevention)";
		default: return "unknown";
		}
	}

	// Print exception details (code, address, access info, etc.) to a stream.
	static void writeExceptionInfo(std::ostream& s, EXCEPTION_POINTERS* p)
	{
		s << "------ Exception Information ------\n";
		if (!p || !p->ExceptionRecord)
		{
			s << "  No exception record available (terminate-handler path)\n";
			return;
		}

		EXCEPTION_RECORD* rec = p->ExceptionRecord;
		s << "  Exception Code:    0x" << std::hex << std::uppercase
		  << rec->ExceptionCode << std::dec << " (" << exceptionCodeToString(rec->ExceptionCode) << ")\n";
		s << "  Exception Flags:   0x" << std::hex << rec->ExceptionFlags << std::dec
		  << (rec->ExceptionFlags & EXCEPTION_NONCONTINUABLE ? " (non-continuable)" : "") << "\n";
		s << "  Exception Address: 0x" << std::hex << reinterpret_cast<uintptr_t>(rec->ExceptionAddress) << std::dec << "\n";
		s << "  Parameters:        " << rec->NumberParameters << "\n";

		// Decode exception-specific parameters.
		if (rec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && rec->NumberParameters >= 2)
		{
			ULONG_PTR accessType = rec->ExceptionInformation[0];
			ULONG_PTR accessAddr = rec->ExceptionInformation[1];
			s << "  Access Type:       " << accessTypeToString(accessType) << "\n";
			s << "  Accessed Address:  0x" << std::hex << accessAddr << std::dec;
			if (accessAddr < 0x10000)
				s << " (likely null/invalid pointer)";
			s << "\n";
		}
		else if (rec->ExceptionCode == EXCEPTION_IN_PAGE_ERROR && rec->NumberParameters >= 3)
		{
			s << "  Access Type:       " << accessTypeToString(rec->ExceptionInformation[0]) << "\n";
			s << "  Accessed Address:  0x" << std::hex << rec->ExceptionInformation[1] << std::dec << "\n";
			s << "  NTSTATUS:          0x" << std::hex << rec->ExceptionInformation[2] << std::dec << "\n";
		}
		else if (rec->ExceptionCode == 0xE06D7363 && rec->NumberParameters >= 2)
		{
			// MSVC C++ exception: parameters are [magic, obj_ptr, throw_info, base_addr (x64)]
			s << "  C++ Object Addr:   0x" << std::hex << rec->ExceptionInformation[1] << std::dec << "\n";
			s << "  (See \"C++ Exception\" section below for what() message if available)\n";
		}

		// If chained inner exception is present, note it.
		if (rec->ExceptionRecord)
		{
			s << "  (Chained inner exception present)\n";
		}
		s << "\n";
	}

	// Print CPU register values from a CONTEXT structure (x64 only).
	static void writeRegisters(std::ostream& s, CONTEXT* ctx)
	{
		if (!ctx) return;
		s << "------ CPU Registers ------\n";
#if defined(_M_X64) || defined(_M_AMD64)
		s << std::hex << std::uppercase;
		s << "  RAX = 0x" << ctx->Rax << "    RBX = 0x" << ctx->Rbx << "\n";
		s << "  RCX = 0x" << ctx->Rcx << "    RDX = 0x" << ctx->Rdx << "\n";
		s << "  RSI = 0x" << ctx->Rsi << "    RDI = 0x" << ctx->Rdi << "\n";
		s << "  RBP = 0x" << ctx->Rbp << "    RSP = 0x" << ctx->Rsp << "\n";
		s << "  R8  = 0x" << ctx->R8  << "    R9  = 0x" << ctx->R9  << "\n";
		s << "  R10 = 0x" << ctx->R10 << "    R11 = 0x" << ctx->R11 << "\n";
		s << "  R12 = 0x" << ctx->R12 << "    R13 = 0x" << ctx->R13 << "\n";
		s << "  R14 = 0x" << ctx->R14 << "    R15 = 0x" << ctx->R15 << "\n";
		s << "  RIP = 0x" << ctx->Rip << "    EFlags = 0x" << ctx->EFlags << "\n";
		s << std::dec;
#elif defined(_M_IX86)
		s << std::hex << std::uppercase;
		s << "  EAX = 0x" << ctx->Eax << "    EBX = 0x" << ctx->Ebx << "\n";
		s << "  ECX = 0x" << ctx->Ecx << "    EDX = 0x" << ctx->Edx << "\n";
		s << "  ESI = 0x" << ctx->Esi << "    EDI = 0x" << ctx->Edi << "\n";
		s << "  EBP = 0x" << ctx->Ebp << "    ESP = 0x" << ctx->Esp << "\n";
		s << "  EIP = 0x" << ctx->Eip << "    EFlags = 0x" << ctx->EFlags << "\n";
		s << std::dec;
#else
		s << "  (register dump not implemented for this architecture)\n";
#endif
		s << "\n";
	}

	// Print OS version, CPU, memory, etc.
	static void writeSystemInfo(std::ostream& s)
	{
		s << "------ System Information ------\n";

		// OS version via RtlGetVersion (works for Windows 10+, ignoring app compat shims).
		typedef LONG (WINAPI* RtlGetVersionFn)(PRTL_OSVERSIONINFOW);
		HMODULE hNt = GetModuleHandleW(L"ntdll.dll");
		if (hNt)
		{
			RtlGetVersionFn fn = (RtlGetVersionFn)GetProcAddress(hNt, "RtlGetVersion");
			if (fn)
			{
				RTL_OSVERSIONINFOW v = { sizeof(v) };
				if (fn(&v) == 0)
				{
					s << "  OS Version:        Windows " << v.dwMajorVersion << "." << v.dwMinorVersion
					  << " (build " << v.dwBuildNumber << ")\n";
				}
			}
		}

		// CPU/processor info.
		SYSTEM_INFO si = {};
		GetNativeSystemInfo(&si);
		const char* arch = "unknown";
		switch (si.wProcessorArchitecture)
		{
		case PROCESSOR_ARCHITECTURE_AMD64: arch = "x64 (AMD64)"; break;
		case PROCESSOR_ARCHITECTURE_INTEL: arch = "x86"; break;
		case PROCESSOR_ARCHITECTURE_ARM:   arch = "ARM"; break;
		case PROCESSOR_ARCHITECTURE_ARM64: arch = "ARM64"; break;
		}
		s << "  CPU Architecture:  " << arch << "\n";
		s << "  Logical CPUs:      " << si.dwNumberOfProcessors << "\n";
		s << "  Page Size:         " << si.dwPageSize << " bytes\n";

		// Memory info.
		MEMORYSTATUSEX ms = { sizeof(ms) };
		if (GlobalMemoryStatusEx(&ms))
		{
			const ULONGLONG MB = 1024ULL * 1024ULL;
			s << "  Memory Load:       " << ms.dwMemoryLoad << "%\n";
			s << "  Total Physical:    " << (ms.ullTotalPhys / MB) << " MB\n";
			s << "  Available Phys:    " << (ms.ullAvailPhys / MB) << " MB\n";
			s << "  Process Working:   ";
			PROCESS_MEMORY_COUNTERS pmc = {};
			if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
			{
				s << (pmc.WorkingSetSize / MB) << " MB (peak " << (pmc.PeakWorkingSetSize / MB) << " MB)\n";
			}
			else
			{
				s << "(unavailable)\n";
			}
		}
		s << "\n";
	}

	// Print process info: PID, command line, working directory, uptime.
	static void writeProcessInfo(std::ostream& s)
	{
		s << "------ Process Information ------\n";
		s << "  Process ID:        " << GetCurrentProcessId() << "\n";

		char path[MAX_PATH] = {};
		if (GetModuleFileNameA(NULL, path, MAX_PATH))
		{
			s << "  Executable:        " << path << "\n";
		}

		LPSTR cmdLine = GetCommandLineA();
		if (cmdLine)
		{
			s << "  Command Line:      " << cmdLine << "\n";
		}

		char cwd[MAX_PATH] = {};
		if (GetCurrentDirectoryA(MAX_PATH, cwd))
		{
			s << "  Working Dir:       " << cwd << "\n";
		}

		// Process uptime.
		FILETIME creation, exit, kernel, user;
		if (GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user))
		{
			FILETIME nowFt;
			GetSystemTimeAsFileTime(&nowFt);
			ULARGE_INTEGER cs, ns;
			cs.LowPart = creation.dwLowDateTime;  cs.HighPart = creation.dwHighDateTime;
			ns.LowPart = nowFt.dwLowDateTime;     ns.HighPart = nowFt.dwHighDateTime;
			ULONGLONG uptime100ns = ns.QuadPart - cs.QuadPart;
			ULONGLONG uptimeSec = uptime100ns / 10000000ULL;
			s << "  Process Uptime:    " << uptimeSec << " seconds\n";
		}
		s << "\n";
	}

	// Print info for a single stack frame: symbol, module, source line.
	static void writeStackFrame(std::ostream& s, HANDLE process, DWORD64 address, int index)
	{
		// Symbol name.
		alignas(SYMBOL_INFO) char buf[sizeof(SYMBOL_INFO) + 256];
		SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(buf);
		symbol->MaxNameLen = 255;
		symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
		DWORD64 displacement = 0;
		const char* name = "<unknown>";
		if (SymFromAddr(process, address, &displacement, symbol))
		{
			name = symbol->Name;
		}

		// Module name.
		IMAGEHLP_MODULE64 mod = { sizeof(mod) };
		const char* moduleName = "?";
		if (SymGetModuleInfo64(process, address, &mod))
		{
			moduleName = mod.ModuleName;
		}

		// Source file + line.
		IMAGEHLP_LINE64 line = { sizeof(line) };
		DWORD lineDisp = 0;
		bool hasLine = SymGetLineFromAddr64(process, address, &lineDisp, &line) != FALSE;

		s << "    [" << index << "] " << moduleName << "!" << name
		  << " + 0x" << std::hex << displacement << std::dec
		  << " (0x" << std::hex << address << std::dec << ")";
		if (hasLine)
		{
			s << "\n        at " << line.FileName << ":" << line.LineNumber;
		}
		s << "\n";
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
		static void setLastExceptionPointers(EXCEPTION_POINTERS* p)
		{
			ExceptionHandler::s_lastExceptionPointers = p;
		}
	};

	// Storage for a copy of exception info captured inside the SEH filter.
	// EXCEPTION_POINTERS stack memory becomes invalid after the __except block
	// returns, so we must copy the EXCEPTION_RECORD and CONTEXT.
	static EXCEPTION_RECORD  s_capturedRecord;
	static CONTEXT           s_capturedContext;
	static EXCEPTION_POINTERS s_capturedPointers;

	// Last C++ exception captured at the std::terminate entry point.
	static std::exception_ptr s_capturedCxxException;

	static int generateDumpFromCurrentContextFilter(EXCEPTION_POINTERS* p, ExceptionHandler* inst)
	{
		ExeptionHandlerInternal::generateCrashDump(inst, p);

		// Copy the exception info so it survives past the __except return.
		if (p && p->ExceptionRecord) s_capturedRecord  = *p->ExceptionRecord;
		if (p && p->ContextRecord)   s_capturedContext = *p->ContextRecord;
		s_capturedPointers.ExceptionRecord = (p && p->ExceptionRecord) ? &s_capturedRecord  : nullptr;
		s_capturedPointers.ContextRecord   = (p && p->ContextRecord)   ? &s_capturedContext : nullptr;
		ExeptionHandlerInternal::setLastExceptionPointers(&s_capturedPointers);

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

		// Signal handlers — only for signals that aren't naturally handled by SEH.
		// On MSVC, registering handlers for SIGSEGV/SIGILL/SIGFPE causes the CRT
		// to install a SEH-to-signal translator that runs BEFORE our SEH filter,
		// losing the original EXCEPTION_RECORD information. Let SEH handle those
		// natively. Keep handlers for the explicitly-raised signals.
		signal(SIGINT, &abortHandle);
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

		// Save exception pointers so saveStackTrace can include the full
		// crash context (registers, exception cause, etc.).
		s_lastExceptionPointers = pExceptionPointers;

		// Mark that we're inside crash handler so onTerminate won't try to acquire lock again
		InterlockedExchange(&s_insideCrashHandler, 1);
		onTerminate();
	}
	[[noreturn]]
	void ExceptionHandler::onTerminate()
	{
		//STACK_WATCHER_FUNC;

		// Capture the active C++ exception immediately so we can include its
		// what() message in the crash report. It must be done before any
		// other call that could disturb the exception state.
		s_capturedCxxException = std::current_exception();

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

		// Note: don't consume std::current_exception() here — saveStackTrace()
		// reads it to include the what() message in the report.

		inst.saveStackTrace(s_lastExceptionPointers);

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
	void ExceptionHandler::saveStackTrace(EXCEPTION_POINTERS* pExceptionPointers)
	{
		//STACK_WATCHER_FUNC;
		std::string filePath = m_crashExportPath + "\\" + getExeName() + "_stack.txt";
		std::wstring wPath = std::wstring(m_crashExportPath.begin(), m_crashExportPath.end());
		// Attempt to create the directory
		CreateDirectoryW(wPath.c_str(), NULL);
		std::stringstream stream;

		// === Header: library + timestamp ===
		LibraryInfo::printInfo(stream);
		stream << "\n";
		stream << "================================================================\n";
		stream << "                       CRASH REPORT\n";
		stream << "================================================================\n";
		stream << "Timestamp:           " << getCurrentDateTime() << "\n";
		stream << "Crashed Thread ID:   " << GetCurrentThreadId() << "\n";

		if (s_abortSignal != 0)
			stream << "Signal:              " << s_abortSignal
				   << " (" << abortSignalToString(s_abortSignal) << ")\n";
		stream << "\n";

		// === Exception details (cause of crash) ===
		writeExceptionInfo(stream, pExceptionPointers);

		// === Process info ===
		writeProcessInfo(stream);

		// === System info ===
		writeSystemInfo(stream);

		// === Last C++ exception (if applicable) ===
		// Use the exception captured at the start of onTerminate, since
		// std::current_exception() may be empty by the time we get here.
		std::exception_ptr exptr = s_capturedCxxException
			? s_capturedCxxException
			: std::current_exception();
		if (exptr)
		{
			stream << "------ C++ Exception ------\n";
			try {
				std::rethrow_exception(exptr);
			}
			catch (const std::exception& ex) {
				stream << "  Type:    std::exception (or derived)\n";
				stream << "  what():  " << ex.what() << "\n";
			}
			catch (...) {
				stream << "  Type:    unknown (non-std::exception)\n";
			}
			stream << "\n";
		}

		// === CPU registers at crash ===
		if (pExceptionPointers && pExceptionPointers->ContextRecord)
		{
			writeRegisters(stream, pExceptionPointers->ContextRecord);
		}

		// === DbgHelp stack trace ===
		HANDLE process = GetCurrentProcess();
		SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);

		// Build a symbol search path that includes the EXE directory, the
		// build directory (where MSVC writes PDBs by default), and any path
		// from the _NT_SYMBOL_PATH environment variable.
		std::string searchPath;
		char exePath[MAX_PATH] = {};
		if (GetModuleFileNameA(NULL, exePath, MAX_PATH))
		{
			std::string dir = exePath;
			size_t slash = dir.find_last_of('\\');
			if (slash != std::string::npos)
			{
				searchPath = dir.substr(0, slash);
				// Also try parent directory (build/Release -> build/)
				size_t parentSlash = searchPath.find_last_of('\\');
				if (parentSlash != std::string::npos)
				{
					searchPath += ";" + searchPath.substr(0, parentSlash);
				}
			}
		}
		char ntSymPath[1024] = {};
		if (GetEnvironmentVariableA("_NT_SYMBOL_PATH", ntSymPath, sizeof(ntSymPath)) > 0)
		{
			if (!searchPath.empty()) searchPath += ";";
			searchPath += ntSymPath;
		}
		SymInitialize(process, searchPath.empty() ? NULL : searchPath.c_str(), TRUE);

		stream << "------ Stack Trace (DbgHelp) ------\n";

		if (pExceptionPointers && pExceptionPointers->ContextRecord)
		{
			// Walk the stack starting from the exception's CONTEXT (more accurate
			// for SEH crashes — gives the original frame, not the handler's).
			CONTEXT ctx = *pExceptionPointers->ContextRecord;
			STACKFRAME64 frame = {};
			DWORD machine;
#if defined(_M_X64) || defined(_M_AMD64)
			machine = IMAGE_FILE_MACHINE_AMD64;
			frame.AddrPC.Offset    = ctx.Rip;
			frame.AddrFrame.Offset = ctx.Rbp;
			frame.AddrStack.Offset = ctx.Rsp;
#elif defined(_M_IX86)
			machine = IMAGE_FILE_MACHINE_I386;
			frame.AddrPC.Offset    = ctx.Eip;
			frame.AddrFrame.Offset = ctx.Ebp;
			frame.AddrStack.Offset = ctx.Esp;
#else
			machine = IMAGE_FILE_MACHINE_UNKNOWN;
#endif
			frame.AddrPC.Mode    = AddrModeFlat;
			frame.AddrFrame.Mode = AddrModeFlat;
			frame.AddrStack.Mode = AddrModeFlat;

			HANDLE thread = GetCurrentThread();
			int idx = 0;
			while (idx < MAX_FRAMES &&
				StackWalk64(machine, process, thread, &frame, &ctx,
					NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL))
			{
				if (frame.AddrPC.Offset == 0) break;
				writeStackFrame(stream, process, frame.AddrPC.Offset, idx);
				++idx;
			}
		}
		else
		{
			// Fallback: capture from current location.
			void* stack[MAX_FRAMES];
			WORD frames = CaptureStackBackTrace(0, MAX_FRAMES, stack, NULL);
			for (WORD i = 0; i < frames; i++)
			{
				writeStackFrame(stream, process, (DWORD64)stack[i], i);
			}
		}

		SymCleanup(process);

		// === StackWatcher trace ===
		stream << "\n------ Stack Trace (StackWatcher) ------\n"
			   << StackWatcher::toString();

		stream << "================================================================\n";
		stream << "                    END OF CRASH REPORT\n";
		stream << "================================================================\n";

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