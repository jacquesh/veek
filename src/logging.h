#ifndef _LOGGING_H
#define _LOGGING_H

#include <stdio.h>
#include <stdarg.h>

bool initLogging();
void deinitLogging();

void _logTerm(const char* format, ...);
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

/* Log Levels:
 * Term: Logs only to the terminal, used for very high-frequency or low-impact messages
 * Info: Logs to terminal and file, use for information that could be useful but not a problem
 * Warn: Logs to terminal and file, use for errors/situations that are bad but recoverable
 * Fail: Logs to terminal and file, use for errors/failures that we cannot recover from
 */
#define logTerm(MSG, ...); _logTerm("TERM: %16s:%-3d - " MSG, __file_baseName(__FILE__), __LINE__, ##__VA_ARGS__);
#define logInfo(MSG, ...);     _log("INFO: %16s:%-3d - " MSG, __file_baseName(__FILE__), __LINE__, ##__VA_ARGS__);
#define logWarn(MSG, ...);     _log("WARN: %16s:%-3d - " MSG, __file_baseName(__FILE__), __LINE__, ##__VA_ARGS__);
#define logFail(MSG, ...);     _log("FAIL: %16s:%-3d - " MSG, __file_baseName(__FILE__), __LINE__, ##__VA_ARGS__);
#endif // NDEBUG

#endif
