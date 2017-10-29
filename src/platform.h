#ifndef _PLATFORM_H
#define _PLATFORM_H

#include <stddef.h>
#include <stdint.h>

// TODO: Docs, what do these functions return?

namespace Platform
{
    struct DateTime
    {
        uint16_t Year;
        uint16_t Month;
        uint16_t Day;
        uint16_t Hour;
        uint16_t Minute;
        uint16_t Second;
        uint16_t Millisecond;
    };

    struct Thread;
    typedef int ThreadStartFunction(void*);

    bool Setup();
    void Shutdown();

    Thread* CreateThread(ThreadStartFunction* entryPoint, void* data);
    void JoinThread(Thread* thread);

    struct Mutex;
    Mutex* CreateMutex();
    void DestroyMutex(Mutex* mutex);
    void LockMutex(Mutex* mutex);
    void UnlockMutex(Mutex* mutex);

    void SleepForMilliseconds(uint32_t milliseconds);

    double SecondsSinceStartup();

    int GetCurrentUserName(size_t bufferLen, char* buffer);

    bool IsPushToTalkKeyPushed();

    DateTime GetLocalDateTime();
}

#endif
