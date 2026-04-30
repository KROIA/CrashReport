#include "StackWatcher.h"
#include <sstream>
#include <thread>
#include <chrono>

namespace CrashReport
{
    std::unordered_map<DWORD, StackWatcher::StackContainer> StackWatcher::s_stack; 
    std::mutex StackWatcher::s_mutex;


    StackWatcher::StackWatcher(const char* name)
        : m_name(name)
    {
        DWORD threadID = GetCurrentThreadId();
        std::lock_guard<std::mutex> lock(s_mutex);
        const auto& it = s_stack.find(threadID);
        if (it == s_stack.end())
        {
            s_stack[threadID].currentStackPos = 1;
            s_stack[threadID].stack = std::vector<StackWatcher*>(s_defaultStackSize, nullptr);
            s_stack[threadID].stack[0] = this;
            m_stackPos = 0;
            return;
        }

        m_stackPos = it->second.currentStackPos;
        if (it->second.stack.size() > m_stackPos)
            it->second.stack[m_stackPos] = this;
        else
            it->second.stack.push_back(this);
        ++it->second.currentStackPos;
    }
    StackWatcher::~StackWatcher()
    {
        DWORD threadID = GetCurrentThreadId();
        std::lock_guard<std::mutex> lock(s_mutex);
        const auto& it = s_stack.find(threadID);
        if (it == s_stack.end())
            return; // shuld not happen
        if (it->second.currentStackPos != m_stackPos)
        {
            // Shuld not happen...
            // Would happen if the instances are destroyed in the wrong order.
            //std::cerr << "Stack corrupt";
            memset(&it->second.stack[m_stackPos], 0, (it->second.stack.size() - m_stackPos) * sizeof(StackWatcher*));
            it->second.currentStackPos = m_stackPos;
            return;
        }
        it->second.stack[m_stackPos] = nullptr;
        --it->second.currentStackPos;
    }

    void StackWatcher::init()
    {
        static bool initDone = false;
        if (initDone)
            return;
        initDone = true;

        DWORD threadID = GetCurrentThreadId();
        s_stack[threadID].stack = std::vector<StackWatcher*>(s_defaultStackSize, nullptr);
        //memset(s_stack, 0, s_stackSize * sizeof(StackWatcher*));
    }


    void StackWatcher::printStack()
    {
        std::cout << StackWatcher::toString();
    }
    std::string StackWatcher::toString()
    {
        // Use try_lock with timeout to avoid deadlock during crash handling
        // (e.g., if stack overflow occurred while a StackWatcher held the mutex).
        std::unique_lock<std::mutex> lock(s_mutex, std::try_to_lock);
        if (!lock.owns_lock())
        {
            // Try once more with a short wait, then proceed without lock.
            // Reading without lock during crash is a calculated risk since
            // the program is about to terminate anyway.
            for (int i = 0; i < 10 && !lock.owns_lock(); ++i)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                lock.try_lock();
            }
        }

        std::stringstream s("Stack: \n");
        if (!lock.owns_lock())
        {
            s << "  (mutex unavailable, reading without lock)\n";
        }

        for (const auto& thread : s_stack)
        {
            s << "  Stack of thread ID: " << thread.first << "\n";
            for (size_t i = 0; i < thread.second.currentStackPos; ++i)
            {
                if (thread.second.stack[i] != nullptr)
                {
                    s << "    [" << i << "] " << thread.second.stack[i]->getName() << "\n";
                }
            }
            s << "\n";
        }
        return s.str();
    }
}