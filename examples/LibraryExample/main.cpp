#include <iostream>
#include "CrashReport.h"

#include <windows.h>
#include <thread>


void crashTest();
void exceptionCallback();
void threadFunc();


int main(int argc, char* argv[])
{
	STACK_WATCHER_FUNC;

	CrashReport::Profiler::start();
	CrashReport::LibraryInfo::printInfo();
	CrashReport::ExceptionHandler::setup("crashFiles");
	CrashReport::ExceptionHandler::setExceptionCallback(exceptionCallback);

	std::thread t1([]() {
		STACK_WATCHER_FUNC;
		// sleep for 1 second
		std::this_thread::sleep_for(std::chrono::seconds(1));
		crashTest();
	});
	//std::thread t2(threadFunc);

	std::this_thread::sleep_for(std::chrono::seconds(4));
	//crashTest();
	//t2.join();
	std::cout << "End of program\n";

	return 0;
}

__declspec(noinline)
void crashTest()
{
	STACK_WATCHER_FUNC;

	// Cause a segmentation fault (access violation) intentionally
	int* ptr = (int*)0;
	//throw std::exception("Test exception");
	*ptr = 42;      // <<-- Crashes here
}

void exceptionCallback()
{
	std::cout << "\nCallback reached\n\n";
}
void threadFunc()
{
	STACK_WATCHER_FUNC;
	std::this_thread::sleep_for(std::chrono::seconds(2));
	crashTest();
}