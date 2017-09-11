#ifndef _PLATFORM_H
#define _PLATFORM_H

#include <stdint.h>

#ifdef __linux__
#include <stddef.h>
#endif

// TODO: Docs, what do these functions return?

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

struct Mutex;
Mutex* createMutex();
void destroyMutex(Mutex* mutex);
void lockMutex(Mutex* mutex);
void unlockMutex(Mutex* mutex);

void sleepForMilliseconds(uint32_t milliseconds);

int64_t getClockValue();
int64_t getClockFrequency();

int getCurrentUserName(size_t bufferLen, char* buffer);

bool isPushToTalkKeyPushed();

DateTime getLocalDateTime();

#endif
