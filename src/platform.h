#ifndef _PLATFORM_H
#define _PLATFORM_H

#include "common.h"

// TODO: Docs, what do these functions return?

struct Mutex;
Mutex* createMutex();
void destroyMutex(Mutex* mutex);
void lockMutex(Mutex* mutex);
void unlockMutex(Mutex* mutex);

void sleepForMilliseconds(uint32 milliseconds);

int64 getClockValue();
int64 getClockFrequency();

int getCurrentUserName(size_t bufferLen, char* buffer);

#endif
