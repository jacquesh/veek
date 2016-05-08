#include "platform.h"

#include <windows.h>

#include "common.h"

// TODO: Implementation


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

void sleep(uint32 milliseconds)
{
    Sleep(milliseconds);
}
