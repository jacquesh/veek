#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctime>

#include "common.h"
#include "platform.h"

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
    int success = pthread_create(&result->handle, nullptr, entryPoint, data);
    if(!success)
    {
        delete result;
        return nullptr;
    }
    return result;
}

int Platform::JoinThread(Platform::Thread* thread)
{
    void* result;
    pthread_join(thread->handle, result);
    return (int)result;
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

DateTime Platform::GetLocalDateTime()
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
