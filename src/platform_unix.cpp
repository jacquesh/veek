#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "platform.h"

typedef void* UnixThreadEntryPoint(void*);
static double clockSetupTime;

struct Platform::Thread
{
    pthread_t handle;
};

struct Platform::Mutex
{
    pthread_mutex_t mutex;
};

Platform::Mutex* Platform::CreateMutex()
{
    Mutex* result = (Mutex*)malloc(sizeof(Mutex));
    pthread_mutex_init(&result->mutex, NULL);
    return result;
};

void Platform::DestroyMutex(Mutex* mutex)
{
    pthread_mutex_destroy(&mutex->mutex);
    free(mutex);
}

void Platform::LockMutex(Mutex* mutex)
{
    pthread_mutex_lock(&mutex->mutex);
}

void Platform::UnlockMutex(Mutex* mutex)
{
    pthread_mutex_unlock(&mutex->mutex);
}

Platform::Thread* Platform::CreateThread(Platform::ThreadStartFunction* entryPoint, void* data)
{
    Thread* result = new Thread();
    int error = pthread_create(&result->handle, nullptr, (UnixThreadEntryPoint*)entryPoint, data);
    if(error)
    {
        delete result;
        return nullptr;
    }
    return result;
}

void Platform::JoinThread(Platform::Thread* thread)
{
    // TODO: Check for errors
    pthread_join(thread->handle, nullptr);
}

void Platform::SleepForMilliseconds(uint32 milliseconds)
{
    usleep(1000*milliseconds);
}

int Platform::GetCurrentUserName(size_t bufferLen, char* buffer)
{
    int result = getlogin_r(buffer, bufferLen);
    return result;
}

static double GetClockSeconds()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ((double)ts.tv_sec) + (((double)ts.tv_nsec)/1000000000.0);
}
double Platform::SecondsSinceStartup()
{
    return GetClockSeconds() - clockSetupTime;
}

bool Platform::IsPushToTalkKeyPushed()
{
    return false;
}

Platform::DateTime Platform::GetLocalDateTime()
{
    timespec now;
    clock_gettime(CLOCK_REALTIME, &now);

    uint16_t nowMillis = (uint16_t)(now.tv_nsec/1000000);
    struct tm nowSplit = {};
    localtime_r(&now.tv_sec, &nowSplit);

    DateTime result = {};
    result.Year = nowSplit.tm_year + 1990;
    result.Month = nowSplit.tm_mon;
    result.Day = nowSplit.tm_mday;
    result.Hour = nowSplit.tm_hour;
    result.Minute = nowSplit.tm_min;
    result.Second = nowSplit.tm_sec;
    result.Millisecond = nowMillis;
    return result;
}

bool Platform::Setup()
{
    timespec startupTs;
    if(clock_gettime(CLOCK_MONOTONIC, &startupTs) != 0)
    {
        return false;
    }

    clockSetupTime = GetClockSeconds();
    return true;
}

void Platform::Shutdown()
{
}
