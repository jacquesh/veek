#include "platform.h"

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctime>

#include "common.h"

struct Mutex
{
    pthread_mutex_t mutex;
};

Mutex* createMutex()
{
    Mutex* result = (Mutex*)malloc(sizeof(Mutex));
    pthread_mutex_init(&result->mutex, NULL);
    return result;
};

void destroyMutex(Mutex* mutex)
{
    pthread_mutex_destroy(&mutex->mutex);
    free(mutex);
}

void lockMutex(Mutex* mutex)
{
    pthread_mutex_lock(&mutex->mutex);
}

void unlockMutex(Mutex* mutex)
{
    pthread_mutex_unlock(&mutex->mutex);
}

void sleepForMilliseconds(uint32 milliseconds)
{
    usleep(1000*milliseconds);
}

int getCurrentUserName(size_t bufferLen, char* buffer)
{
    int result = getlogin_r(buffer, bufferLen);
    return result;
}

int64 getClockFrequency()
{
    int64 nsec_count, nsec_per_tick;
    /*
     * clock_gettime() returns the number of secs. We translate that to number of nanosecs.
     * clock_getres() returns number of seconds per tick. We translate that to number of nanosecs per tick.
     * Number of nanosecs divided by number of nanosecs per tick - will give the number of ticks.
     */
     struct timespec ts1, ts2;

     if (clock_gettime(CLOCK_MONOTONIC, &ts1) != 0) {
         return -1;
     }

     nsec_count = ts1.tv_nsec + ts1.tv_sec * 1000000000;

     if (clock_getres(CLOCK_MONOTONIC, &ts2) != 0) {
         return -1;
     }

     nsec_per_tick = ts2.tv_nsec + ts2.tv_sec * 1000000000;

     return (nsec_count / nsec_per_tick);
}

int64 getClockValue()
{
  return getClockFrequency();
}

bool isPushToTalkKeyPushed()
{
    return false;
}

DateTime getLocalDateTime()
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
