#ifndef _PLATFORM_H
#define _PLATFORM_H

#include "common.h"

struct Mutex;
Mutex* createMutex();
void destroyMutex(Mutex* mutex);
void lockMutex(Mutex* mutex);
void unlockMutex(Mutex* mutex);

void sleep(uint32 milliseconds);

#endif