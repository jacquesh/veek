#ifndef _COMMON_H
#define _COMMON_H

#include <stdint.h>
#include <stdio.h>

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

const unsigned char MAX_ROOM_NAME_LENGTH = 128;
const unsigned char MAX_USER_NAME_LENGTH = 128;

#ifdef NDEBUG
#define log(MSG, ...);
#else
static inline const char* __file_baseName(const char* fileName)
{
    const char* baseName = fileName;

    for(const char* currentName=fileName; *currentName != 0; ++currentName)
    {
        char currentChar = *currentName;
        if((currentChar == '/') || (currentChar == '\\'))
        {
            baseName = currentName+1;
        }
    }

    return baseName;
}
#define log(MSG, ...); fprintf(stderr, "(%s:%d) " MSG, __file_baseName(__FILE__), __LINE__, ##__VA_ARGS__);
#endif // NDEBUG

#endif
