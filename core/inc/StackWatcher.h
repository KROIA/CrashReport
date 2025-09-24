#pragma once
#include "CrashReport_base.h"
#include <iostream>
#include <string>
#include <windows.h>
#include <mutex>
#include <unordered_map>

namespace CrashReport
{

#ifndef __FUNCSIG__
#define __FUNCSIG__ __PRETTY_FUNCTION__
#endif
#define STACK_WATCHER_FUNC \
    static const char *funcName = __FUNCSIG__; \
    CrashReport::StackWatcher stackWatcherElementObj(funcName);


    /// <summary>
    /// The constructor adds the this pointer to a static array.
    /// The destructor removes the pointer from the static array.
    /// 
    /// Each constructor call increments the index used to add the this pointer for the array.
    /// The destructor decrements the index.
    /// 
    /// This creates a simple stack like structure with a fixed depth for the stack trace.
    /// 
    /// Example:
    /// void foo()
    /// {
    ///     StackWatcher stackWatcherElementObj(__FUNCSIG__);
    ///     // do stuff ...
    /// 
    ///     bar();
    /// }
    /// 
    /// void bar()
    /// {
    ///     StackWatcher stackWatcherElementObj(__FUNCSIG__);
    /// 
    ///     // do stuff ...
    /// }
    /// </summary>
    class CRASH_REPORT_API StackWatcher
    {
    public:
        StackWatcher(const char* name);
        ~StackWatcher();

        const char* getName() const
        {
            return m_name;
        }
        size_t getStackIndex() const
        {
            return m_stackPos;
        }

        static void init();
        static void printStack();
        static std::string toString();


    private:

        const char* m_name;
        size_t m_stackPos;

        struct StackContainer
        {
            size_t currentStackPos = 0;
            std::vector<StackWatcher*> stack;
        };

        // Key is the Thread ID
        static std::unordered_map<DWORD , StackContainer> s_stack; // list to hold the current living stack instances.
        static std::mutex s_mutex;
        static const size_t s_defaultStackSize = 1000;
    };
}