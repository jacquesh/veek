#define WIN32_LEAN_AND_MEAN
#include <windows.h>
// NOTE: We undefine these windows macros here so that we can use them for our own symbol names.
//       windows.h specifies some functions twice (for ANSI and Unicode character support) and then
//       uses macros to point to the correct ones at compile time.
#ifdef CreateMutex
#undef CreateMutex
#endif

#include <stdlib.h>

#include "platform.h"

static int64_t clockTicksPerSecond;
static int64_t clockSetupTicks;

struct Platform::Thread
{
    HANDLE handle;
};

// SDL uses CriticalSections (https://msdn.microsoft.com/en-us/library/windows/desktop/ms682530%28v=vs.85%29.aspx)
// Apparently SRWs are much faster (https://stoyannk.wordpress.com/2016/04/30/msvc-mutex-is-slower-than-you-might-expect/, and https://msdn.microsoft.com/en-us/library/windows/desktop/aa904937%28v=vs.85%29.aspx)
struct Platform::Mutex
{
    CRITICAL_SECTION critSec;
};

Platform::Mutex* Platform::CreateMutex()
{
    Mutex* result = (Mutex*)malloc(sizeof(Mutex));
    InitializeCriticalSection(&result->critSec);
    return result;
}

void Platform::DestroyMutex(Platform::Mutex* mutex)
{
    DeleteCriticalSection(&mutex->critSec);
    free(mutex);
}

void Platform::LockMutex(Platform::Mutex* mutex)
{
    EnterCriticalSection(&mutex->critSec);
}

void Platform::UnlockMutex(Platform::Mutex* mutex)
{
    LeaveCriticalSection(&mutex->critSec);
}

Platform::Thread* Platform::CreateThread(Platform::ThreadStartFunction* entryPoint, void* data)
{
    HANDLE threadHandle = ::CreateThread(nullptr, 0,
                                       (LPTHREAD_START_ROUTINE)entryPoint, data,
                                       0, nullptr);

    Thread* result = new Thread();
    result->handle = threadHandle;
    return result;
}

int Platform::JoinThread(Platform::Thread* thread)
{
    // TODO: Check for errors
    WaitForSingleObject(thread->handle, INFINITE);

    DWORD returnCode;
    GetExitCodeThread(thread->handle, &returnCode);
    return returnCode;
}

void Platform::SleepForMilliseconds(uint32_t milliseconds)
{
    Sleep(milliseconds);
}

static int64_t GetClockValue()
{
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);
    return result.QuadPart;
}

double Platform::SecondsSinceStartup()
{
    int64_t ticksSinceStartup = GetClockValue() - clockSetupTicks;
    return ((double)ticksSinceStartup)/((double)clockTicksPerSecond);
}

int Platform::GetCurrentUserName(size_t bufferLen, char* buffer)
{
    int result = GetUserName(buffer, (LPDWORD)&bufferLen);
    return result;
}

bool Platform::IsPushToTalkKeyPushed()
{
    uint16_t keyPressed = GetAsyncKeyState(VK_LCONTROL);
    return (keyPressed >> 15) != 0;
}

Platform::DateTime Platform::GetLocalDateTime()
{
    SYSTEMTIME time;
    GetLocalTime(&time);

    DateTime result = {};
    result.Year = time.wYear;
    result.Month = time.wMonth;
    result.Day = time.wDay;
    result.Hour = time.wHour;
    result.Minute = time.wMinute;
    result.Second = time.wSecond;
    result.Millisecond = time.wMilliseconds;
    return result;
}

bool Platform::Setup()
{
    LARGE_INTEGER clockFrequency;
    QueryPerformanceFrequency(&clockFrequency);
    clockTicksPerSecond = clockFrequency.QuadPart;

    clockSetupTicks = GetClockValue();
    return true;
}

void Platform::Shutdown()
{
}
