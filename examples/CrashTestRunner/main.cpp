#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <windows.h>
#include <heapapi.h>
#include "CrashReport.h"

// Dedicated test runner for crash scenarios
// Usage: CrashTestRunner.exe <test_name>
// Each test will intentionally crash to verify the crash handler works

__declspec(noinline) void exceptionCallback();
__declspec(noinline) void testNullPointer();
__declspec(noinline) void testDivideByZero();
__declspec(noinline) void recursiveStackOverflow(int depth = 0);
__declspec(noinline) void testStackOverflow();
__declspec(noinline) void testUnhandledException();
__declspec(noinline) void testAbort();
__declspec(noinline) void testPureVirtual();
__declspec(noinline) void testInvalidParameter();
__declspec(noinline) void testAccessViolation();
__declspec(noinline) void testDoubleDelete();
__declspec(noinline) void testBufferOverrun();
__declspec(noinline) void threadCrashFunc();
__declspec(noinline) void testMultiThreadCrash();
__declspec(noinline) void nestedLevel3();
__declspec(noinline) void nestedLevel2();
__declspec(noinline) void nestedLevel1();
__declspec(noinline) void testNestedCallCrash();
void printUsage();



int main(int argc, char* argv[])
{
	STACK_WATCHER_FUNC;

	// Setup crash reporting
	CrashReport::ExceptionHandler::setup("test_crashFiles");
	CrashReport::ExceptionHandler::setExceptionCallback(exceptionCallback);

	std::cout << "\n";
	CrashReport::LibraryInfo::printInfo();
	std::cout << "\n";

	if (argc < 2)
	{
		printUsage();
		return 1;
	}

	std::string testName = argv[1];
	std::cout << "\n========================================\n";
	std::cout << "Running crash test: " << testName << "\n";
	std::cout << "========================================\n\n";
	std::cout << "Crash dumps will be saved to: crashFiles_test/\n\n";

	if (testName == "null")
		testNullPointer();
	else if (testName == "divide")
		testDivideByZero();
	else if (testName == "stackoverflow")
		testStackOverflow();
	else if (testName == "exception")
		testUnhandledException();
	else if (testName == "abort")
		testAbort();
	else if (testName == "purevirtual")
		testPureVirtual();
	else if (testName == "invalidparam")
		testInvalidParameter();
	else if (testName == "accessviolation")
		testAccessViolation();
	else if (testName == "doubledelete")
		testDoubleDelete();
	else if (testName == "bufferoverrun")
		testBufferOverrun();
	else if (testName == "multithread")
		testMultiThreadCrash();
	else if (testName == "nested")
		testNestedCallCrash();
	else
	{
		std::cout << "Unknown test: " << testName << "\n";
		printUsage();
		return 1;
	}



	std::cout << "\nTest completed without crashing (some tests may not crash reliably)\n";
	return 1;
}






void exceptionCallback()
{
	std::cout << "\n========================================\n";
	std::cout << "CRASH CALLBACK TRIGGERED - Handler working!\n";
	std::cout << "========================================\n\n";
}

// Test functions
__declspec(noinline) void testNullPointer()
{
	STACK_WATCHER_FUNC;
	std::cout << "Testing NULL pointer dereference...\n";
	int* ptr = nullptr;
	*ptr = 42;  // CRASH: ACCESS_VIOLATION
}

__declspec(noinline) void testDivideByZero()
{
	STACK_WATCHER_FUNC;
	std::cout << "Testing divide by zero...\n";
	volatile int zero = 0;
	volatile int result = 42 / zero;  // CRASH: SIGFPE or exception depending on architecture
	(void)result;
}

__declspec(noinline) void recursiveStackOverflow(int depth)
{
	// Keep minimal: large local buffer to consume stack quickly,
	// no iostream calls (they consume stack heavily).
	volatile char buffer[8192];
	buffer[0] = static_cast<char>(depth);
	recursiveStackOverflow(depth + 1);
}

__declspec(noinline) void testStackOverflow()
{
	STACK_WATCHER_FUNC;
	std::cout << "Testing stack overflow...\n";
	std::cout.flush();
	recursiveStackOverflow();  // CRASH: Stack overflow
}

__declspec(noinline) void testUnhandledException()
{
	STACK_WATCHER_FUNC;
	std::cout << "Testing unhandled exception...\n";
	throw std::runtime_error("Intentional unhandled exception");  // CRASH: Unhandled exception
}

__declspec(noinline) void testAbort()
{
	STACK_WATCHER_FUNC;
	std::cout << "Testing abort signal...\n";
	std::abort();  // CRASH: SIGABRT
}

// Pure virtual call - triggered via destructor + non-virtual wrapper.
// During ~AbstractBase, MSVC has reset the vptr to AbstractBase's vtable.
// Calling pureMethod() through a non-virtual wrapper defeats devirtualization,
// causing an indirect virtual dispatch that hits the _purecall slot.
class AbstractBase
{
public:
	AbstractBase() {}
	virtual ~AbstractBase() { callPure(); }   // vptr now points to AbstractBase vtable
	virtual void pureMethod() = 0;
	void callPure() { pureMethod(); }         // Non-virtual wrapper -> indirect dispatch -> _purecall
};

class CrashyDerived : public AbstractBase
{
public:
	CrashyDerived() : AbstractBase() {}
	virtual void pureMethod() override {}
};

__declspec(noinline) void testPureVirtual()
{
	STACK_WATCHER_FUNC;
	std::cout << "Testing pure virtual call...\n";
	CrashyDerived* obj = new CrashyDerived();
	delete obj;   // ~CrashyDerived runs, then ~AbstractBase runs -> _purecall
	std::cout << "Should not reach here\n";
}

__declspec(noinline) void testInvalidParameter()
{
	STACK_WATCHER_FUNC;
	std::cout << "Testing invalid parameter...\n";
	// Passing invalid parameter to CRT function
	FILE* file = nullptr;
	fclose(file);  // CRASH: Invalid parameter
}

__declspec(noinline) void testAccessViolation()
{
	STACK_WATCHER_FUNC;
	std::cout << "Testing access violation...\n";
	int* ptr = (int*)0xDEADBEEF;
	*ptr = 42;  // CRASH: ACCESS_VIOLATION
}

__declspec(noinline) void testDoubleDelete()
{
	STACK_WATCHER_FUNC;
	std::cout << "Testing double delete (heap corruption simulation)...\n";
	std::cout.flush();

	// Real double-delete on modern Windows is unreliable for testing:
	//  - HeapEnableTerminationOnCorruption + __fastfail bypasses SEH (uncatchable)
	//  - Without it, the heap manager may cache freed blocks (no crash)
	//  - Freelist corruption can cause infinite loops inside the heap manager (freeze)
	//  - Mismatched allocators (_aligned_malloc + free) similarly hang or fastfail
	//
	// To verify the crash handler reliably handles heap corruption, we raise
	// STATUS_HEAP_CORRUPTION directly via SEH. Our SetUnhandledExceptionFilter
	// catches this and produces both a minidump and stack trace.
	RaiseException(STATUS_HEAP_CORRUPTION, 0, 0, nullptr);
}

__declspec(noinline) void testBufferOverrun()
{
	STACK_WATCHER_FUNC;
	std::cout << "Testing buffer overrun...\n";
	int arr[10];
	volatile int index = 1000000;
	arr[index] = 42;  // CRASH: Likely ACCESS_VIOLATION
}

__declspec(noinline) void threadCrashFunc()
{
	STACK_WATCHER_FUNC;
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	std::cout << "Thread " << std::this_thread::get_id() << " about to crash\n";
	int* ptr = nullptr;
	*ptr = 42;  // CRASH in thread
	std::cout << "Thread " << std::this_thread::get_id() << " should not reach here\n";
}

__declspec(noinline) void testMultiThreadCrash()
{
	STACK_WATCHER_FUNC;
	std::cout << "Testing multi-threaded crash...\n";
	std::cout << "Starting 3 threads, first to crash will be caught...\n";

	std::thread t1(threadCrashFunc);
	std::thread t2(threadCrashFunc);
	std::thread t3(threadCrashFunc);

	t1.join();
	t2.join();
	t3.join();
}

__declspec(noinline) void nestedLevel3()
{
	STACK_WATCHER_FUNC;
	std::cout << "In nested level 3\n";
	int* ptr = nullptr;
	*ptr = 42;  // CRASH with deep call stack
}

__declspec(noinline) void nestedLevel2()
{
	STACK_WATCHER_FUNC;
	std::cout << "In nested level 2\n";
	nestedLevel3();
}

__declspec(noinline) void nestedLevel1()
{
	STACK_WATCHER_FUNC;
	std::cout << "In nested level 1\n";
	nestedLevel2();
}

__declspec(noinline) void testNestedCallCrash()
{
	STACK_WATCHER_FUNC;
	std::cout << "Testing crash in nested call stack...\n";
	nestedLevel1();
}

void printUsage()
{
	std::cout << "\nCrash Test Runner - Tests crash handling capabilities\n";
	std::cout << "=====================================================\n\n";
	std::cout << "Usage: CrashTestRunner.exe <test_name>\n\n";
	std::cout << "Available tests:\n";
	std::cout << "  null           - NULL pointer dereference\n";
	std::cout << "  divide         - Divide by zero\n";
	std::cout << "  stackoverflow  - Stack overflow (recursive)\n";
	std::cout << "  exception      - Unhandled C++ exception\n";
	std::cout << "  abort          - Call abort()\n";
	std::cout << "  purevirtual    - Pure virtual function call\n";
	std::cout << "  invalidparam   - Invalid parameter to CRT function\n";
	std::cout << "  accessviolation- Access violation (bad pointer)\n";
	std::cout << "  doubledelete   - Double delete\n";
	std::cout << "  bufferoverrun  - Buffer overrun\n";
	std::cout << "  multithread    - Crash in multiple threads\n";
	std::cout << "  nested         - Crash in nested call stack\n";
	std::cout << "\nExample:\n";
	std::cout << "  CrashTestRunner.exe null\n\n";
}
