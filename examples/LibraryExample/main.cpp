#include <iostream>
#include "CrashReport.h"

#include <windows.h>


void crashTest();
void exceptionCallback();


int main(int argc, char* argv[])
{
	STACK_WATCHER_FUNC;

	CrashReport::Profiler::start();
	CrashReport::LibraryInfo::printInfo();
	CrashReport::ExceptionHandler::setup("crashFiles");
	CrashReport::ExceptionHandler::setExceptionCallback(exceptionCallback);

	crashTest();
	std::cout << "End of program\n";
	return 0;
}

__declspec(noinline)
void crashTest()
{
	STACK_WATCHER_FUNC;

	// Cause a segmentation fault (access violation) intentionally
	int* ptr = (int*)0;
	throw std::exception("Test exception");
	//*ptr = 42;      // <<-- Crashes here
}

void exceptionCallback()
{
	std::cout << "\nCallback reached\n\n";
}