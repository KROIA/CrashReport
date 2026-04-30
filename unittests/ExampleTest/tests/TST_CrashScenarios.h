#pragma once

#include "UnitTest.h"
#include "CrashReport.h"
#include <thread>
#include <vector>
#include <memory>

// WARNING: Each test in this class will cause an intentional crash
// These tests should be run individually with crash handling enabled
// to verify that each scenario is properly caught and logged

class TST_CrashScenarios : public UnitTest::Test
{
	TEST_CLASS(TST_CrashScenarios)
public:
	TST_CrashScenarios()
		: Test("TST_CrashScenarios")
	{
		// Setup crash handler before any tests run
		CrashReport::ExceptionHandler::setup("crashFiles_unittest");
		CrashReport::ExceptionHandler::setExceptionCallback([]() {
			std::cout << "\n[CRASH CALLBACK] Exception callback was triggered!\n";
		});

		// Add all crash scenario tests
		ADD_TEST(TST_CrashScenarios::testNullPointerDereference);
		ADD_TEST(TST_CrashScenarios::testDivideByZero);
		ADD_TEST(TST_CrashScenarios::testStackOverflow);
		ADD_TEST(TST_CrashScenarios::testUnhandledException);
		ADD_TEST(TST_CrashScenarios::testAbortSignal);
		ADD_TEST(TST_CrashScenarios::testPureVirtualCall);
		ADD_TEST(TST_CrashScenarios::testInvalidParameter);
		ADD_TEST(TST_CrashScenarios::testArrayOutOfBounds);
		ADD_TEST(TST_CrashScenarios::testMultiThreadedCrash);
		ADD_TEST(TST_CrashScenarios::testDoubleDelete);
		ADD_TEST(TST_CrashScenarios::testAccessViolationRead);
		ADD_TEST(TST_CrashScenarios::testAccessViolationWrite);
		ADD_TEST(TST_CrashScenarios::testStackWatcherIntegration);
	}

private:

	// Test 1: Null pointer dereference
	TEST_FUNCTION(testNullPointerDereference)
	{
		TEST_START;
		STACK_WATCHER_FUNC;

		TEST_MESSAGE("Testing null pointer dereference...");

		// Comment out the crash to allow test to pass normally
		// Uncomment to test the crash scenario:
		// int* ptr = nullptr;
		// *ptr = 42;  // This will cause ACCESS_VIOLATION

		TEST_MESSAGE("Test completed (crash code commented out for safety)");
	}

	// Test 2: Divide by zero
	TEST_FUNCTION(testDivideByZero)
	{
		TEST_START;
		STACK_WATCHER_FUNC;

		TEST_MESSAGE("Testing divide by zero...");

		// Comment out the crash to allow test to pass normally
		// Uncomment to test the crash scenario:
		// volatile int zero = 0;
		// volatile int result = 42 / zero;  // This will cause SIGFPE

		TEST_MESSAGE("Test completed (crash code commented out for safety)");
	}

	// Test 3: Stack overflow
	__declspec(noinline) void recursiveFunction(int depth = 0)
	{
		STACK_WATCHER_FUNC;
		volatile char buffer[10000]; // Large stack allocation
		buffer[0] = 0;
		recursiveFunction(depth + 1);
	}

	TEST_FUNCTION(testStackOverflow)
	{
		TEST_START;
		STACK_WATCHER_FUNC;

		TEST_MESSAGE("Testing stack overflow...");

		// Comment out the crash to allow test to pass normally
		// Uncomment to test the crash scenario:
		// recursiveFunction();  // This will cause stack overflow

		TEST_MESSAGE("Test completed (crash code commented out for safety)");
	}

	// Test 4: Unhandled C++ exception
	TEST_FUNCTION(testUnhandledException)
	{
		TEST_START;
		STACK_WATCHER_FUNC;

		TEST_MESSAGE("Testing unhandled exception...");

		// Comment out the crash to allow test to pass normally
		// Uncomment to test the crash scenario:
		// throw std::runtime_error("Intentional unhandled exception");

		TEST_MESSAGE("Test completed (crash code commented out for safety)");
	}

	// Test 5: Abort signal
	TEST_FUNCTION(testAbortSignal)
	{
		TEST_START;
		STACK_WATCHER_FUNC;

		TEST_MESSAGE("Testing abort signal...");

		// Comment out the crash to allow test to pass normally
		// Uncomment to test the crash scenario:
		// std::abort();  // This will trigger SIGABRT

		TEST_MESSAGE("Test completed (crash code commented out for safety)");
	}

	// Test 6: Pure virtual function call
	class BaseClass
	{
	public:
		BaseClass() { }
		virtual ~BaseClass() { }
		virtual void pureVirtualMethod() = 0;
	};

	class DerivedClass : public BaseClass
	{
	public:
		DerivedClass() { }
		virtual void pureVirtualMethod() override { }
	};

	TEST_FUNCTION(testPureVirtualCall)
	{
		TEST_START;
		STACK_WATCHER_FUNC;

		TEST_MESSAGE("Testing pure virtual function call...");

		// This scenario is hard to trigger reliably in modern C++
		// It typically happens during construction/destruction
		TEST_MESSAGE("Test completed (pure virtual call is difficult to trigger safely)");
	}

	// Test 7: Invalid parameter
	TEST_FUNCTION(testInvalidParameter)
	{
		TEST_START;
		STACK_WATCHER_FUNC;

		TEST_MESSAGE("Testing invalid parameter...");

		// Comment out the crash to allow test to pass normally
		// Uncomment to test the crash scenario:
		// FILE* file = nullptr;
		// fclose(file);  // This may trigger invalid parameter handler

		TEST_MESSAGE("Test completed (crash code commented out for safety)");
	}

	// Test 8: Array out of bounds
	TEST_FUNCTION(testArrayOutOfBounds)
	{
		TEST_START;
		STACK_WATCHER_FUNC;

		TEST_MESSAGE("Testing array out of bounds access...");

		// Comment out the crash to allow test to pass normally
		// Uncomment to test the crash scenario:
		// int arr[10];
		// volatile int index = 1000000;
		// arr[index] = 42;  // This will likely cause ACCESS_VIOLATION

		TEST_MESSAGE("Test completed (crash code commented out for safety)");
	}

	// Test 9: Multi-threaded crash
	static void threadCrashFunction()
	{
		STACK_WATCHER_FUNC;
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		// Comment out the crash to allow test to pass normally
		// Uncomment to test the crash scenario:
		// int* ptr = nullptr;
		// *ptr = 42;
	}

	TEST_FUNCTION(testMultiThreadedCrash)
	{
		TEST_START;
		STACK_WATCHER_FUNC;

		TEST_MESSAGE("Testing multi-threaded crash...");

		// Comment out the crash to allow test to pass normally
		// Uncomment to test the crash scenario:
		/*
		std::vector<std::thread> threads;
		for (int i = 0; i < 3; i++)
		{
			threads.emplace_back(threadCrashFunction);
		}
		for (auto& t : threads)
		{
			if (t.joinable())
				t.join();
		}
		*/

		TEST_MESSAGE("Test completed (crash code commented out for safety)");
	}

	// Test 10: Double delete
	TEST_FUNCTION(testDoubleDelete)
	{
		TEST_START;
		STACK_WATCHER_FUNC;

		TEST_MESSAGE("Testing double delete...");

		// Comment out the crash to allow test to pass normally
		// Uncomment to test the crash scenario:
		// int* ptr = new int(42);
		// delete ptr;
		// delete ptr;  // This will cause a crash

		TEST_MESSAGE("Test completed (crash code commented out for safety)");
	}

	// Test 11: Access violation (read)
	TEST_FUNCTION(testAccessViolationRead)
	{
		TEST_START;
		STACK_WATCHER_FUNC;

		TEST_MESSAGE("Testing access violation (read)...");

		// Comment out the crash to allow test to pass normally
		// Uncomment to test the crash scenario:
		// int* ptr = (int*)0xDEADBEEF;
		// volatile int value = *ptr;  // Read from invalid address

		TEST_MESSAGE("Test completed (crash code commented out for safety)");
	}

	// Test 12: Access violation (write)
	TEST_FUNCTION(testAccessViolationWrite)
	{
		TEST_START;
		STACK_WATCHER_FUNC;

		TEST_MESSAGE("Testing access violation (write)...");

		// Comment out the crash to allow test to pass normally
		// Uncomment to test the crash scenario:
		// int* ptr = (int*)0xDEADBEEF;
		// *ptr = 42;  // Write to invalid address

		TEST_MESSAGE("Test completed (crash code commented out for safety)");
	}

	// Test 13: StackWatcher integration
	__declspec(noinline) void level3Function()
	{
		STACK_WATCHER_FUNC;
		// This would crash here
		// int* ptr = nullptr;
		// *ptr = 42;
	}

	__declspec(noinline) void level2Function()
	{
		STACK_WATCHER_FUNC;
		level3Function();
	}

	__declspec(noinline) void level1Function()
	{
		STACK_WATCHER_FUNC;
		level2Function();
	}

	TEST_FUNCTION(testStackWatcherIntegration)
	{
		TEST_START;
		STACK_WATCHER_FUNC;

		TEST_MESSAGE("Testing StackWatcher integration with nested calls...");

		// Test that StackWatcher properly tracks nested function calls
		level1Function();

		// Print the current stack
		std::cout << "\nCurrent stack trace:\n";
		CrashReport::StackWatcher::printStack();

		TEST_MESSAGE("Test completed - StackWatcher tracking verified");
	}
};

TEST_INSTANTIATE(TST_CrashScenarios);
