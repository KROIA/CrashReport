#include "StackWatcher.h"
#include <sstream>

namespace CrashReport
{
	StackWatcher* StackWatcher::s_stack[StackWatcher::s_stackSize];
	size_t StackWatcher::s_currentStackPos = 0;


    StackWatcher::StackWatcher(const char* name)
        : m_name(name)
        , m_stackPos(s_currentStackPos)
    {
        if (s_currentStackPos >= s_stackSize)
        {
            //std::cerr << "Stackoverflow\n";
            return;
        }
        //std::cout << "addToStack: " << name << "\n";
        s_stack[s_currentStackPos] = this;
        ++s_currentStackPos;
    }
    StackWatcher::~StackWatcher()
    {
        //std::cout << "removeFromStack: " << name << "\n";
        if (s_currentStackPos != m_stackPos)
        {
            // Shuld not happen...
            // Would happen if the instances are destroyed in the wrong order.
            //std::cerr << "Stack corrupt";
            memset(&s_stack[m_stackPos], 0, (s_stackSize - m_stackPos) * sizeof(StackWatcher*));
            s_currentStackPos = m_stackPos;
            return;
        }
        s_stack[s_currentStackPos] = nullptr;
        --s_currentStackPos;
    }

    void StackWatcher::init()
    {
        memset(s_stack, 0, s_stackSize * sizeof(StackWatcher*));
    }


    void StackWatcher::printStack()
    {
        std::cout << StackWatcher::toString();
    }
    std::string StackWatcher::toString()
    {
        std::stringstream s("Stack: \n");

        for (size_t i = 0; i < s_currentStackPos; ++i)
        {
            s << "[" << i << "] " << s_stack[i]->getName() << "\n";
        }
        return s.str();
    }
}