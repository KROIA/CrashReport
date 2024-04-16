#include "ExceptionHandler.h"
#include "StackWatcher.h"
#include <fstream>
#include <DbgHelp.h>

// Define the maximum stack frames to capture
#define MAX_FRAMES 100

namespace CrashReport
{
	static LONG WINAPI ExceptionHandlerCallback(EXCEPTION_POINTERS* pExceptionPointers);

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

		ExceptionHandler& handler = ExceptionHandler::instance();
		handler.m_crashExportPath = crashExportPath;
	}

	void ExceptionHandler::setExceptionCallback(ExceptionCallback callback)
	{
		ExceptionHandler::instance().m_exeptionCallback = callback;
	}

	void ExceptionHandler::onException(EXCEPTION_POINTERS* pExceptionPointers)
	{
		ExceptionHandler& inst = ExceptionHandler::instance();

		std::wstring wPath = std::wstring(inst.m_crashExportPath.begin(), inst.m_crashExportPath.end());
		// Attempt to create the directory
		CreateDirectoryW(wPath.c_str(), NULL);

		inst.generateCrashDump(pExceptionPointers);
		inst.saveStackTrace();
		if (inst.m_exeptionCallback)
			(*inst.m_exeptionCallback)(pExceptionPointers);
	}

	void ExceptionHandler::generateCrashDump(EXCEPTION_POINTERS* pExceptionPointers)
	{
		std::string exeName = m_crashExportPath+"\\"+getExeName() + ".dmp";
		LPSTR lpSTR_exeName = const_cast<char*>(exeName.c_str());

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
		std::string filePath = m_crashExportPath + "\\" + getExeName() + "_stack.txt";
		std::ofstream outFile(filePath);
		if (!outFile.is_open())
			return;

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

			outFile << "DbgHelp stack content: \n";

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
				outFile << "Frame: [" << i << "]\t" << symName << std::string(length, ' ') << " Return address: " << std::hex << returnAddress << std::dec << std::endl;
				printf("Frame %d: %s Return address: 0x%llX\n", frames, symbol->Name, returnAddress);
			}
		}

		outFile << "\nStackWatcher content: \n" << StackWatcher::toString();
		outFile.close();


		free(symbol);
		SymCleanup(process);
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

	[[noreturn]]
	LONG WINAPI ExceptionHandlerCallback(EXCEPTION_POINTERS* pExceptionPointers)
	{
		// Generate crash dump
		ExeptionHandlerInternal::onException(pExceptionPointers);
		exit(1);
	}
}