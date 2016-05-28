#ifndef _LOGGING_H
#define _LOGGING_H

#include <stdio.h>
#include <stdarg.h>

void _log(const char* format, ...);

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
#define log(MSG, ...); _log("(%s:%d) " MSG, __file_baseName(__FILE__), __LINE__, ##__VA_ARGS__);
#endif // NDEBUG

#endif
