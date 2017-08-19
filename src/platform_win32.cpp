#include "platform.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>

#include "common.h"

// SDL uses CriticalSections (https://msdn.microsoft.com/en-us/library/windows/desktop/ms682530%28v=vs.85%29.aspx)
// Apparently SRWs are much faster (https://stoyannk.wordpress.com/2016/04/30/msvc-mutex-is-slower-than-you-might-expect/, and https://msdn.microsoft.com/en-us/library/windows/desktop/aa904937%28v=vs.85%29.aspx)
struct Mutex
{
    CRITICAL_SECTION critSec;
};

Mutex* createMutex()
{
    Mutex* result = (Mutex*)malloc(sizeof(Mutex));
    InitializeCriticalSection(&result->critSec);
    return result;
};

void destroyMutex(Mutex* mutex)
{
    DeleteCriticalSection(&mutex->critSec);
    free(mutex);
}

void lockMutex(Mutex* mutex)
{
    EnterCriticalSection(&mutex->critSec);
}

void unlockMutex(Mutex* mutex)
{
    LeaveCriticalSection(&mutex->critSec);
}

void sleepForMilliseconds(uint32 milliseconds)
{
    Sleep(milliseconds);
}

int64 getClockValue()
{
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);
    return result.QuadPart;
}

int64 getClockFrequency()
{
    LARGE_INTEGER result;
    QueryPerformanceFrequency(&result);
    return result.QuadPart;
}

int getCurrentUserName(size_t bufferLen, char* buffer)
{
    int result = GetUserName(buffer, (LPDWORD)&bufferLen);
    return result;
}

bool isPushToTalkKeyPushed()
{
    uint16_t keyPressed = GetAsyncKeyState(VK_LCONTROL);
    return (keyPressed >> 15) != 0;
}

DateTime getLocalDateTime()
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
