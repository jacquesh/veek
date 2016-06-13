#include "logging.h"

#include <stdio.h>

static FILE* logFile;

void _logTerm(const char* format, ...)
{
    va_list stderrArgs;
    va_start(stderrArgs, format);
    vfprintf(stderr, format, stderrArgs);
    va_end(stderrArgs);
}

void _log(const char* format, ...)
{
    va_list stderrArgs;
    va_list fileArgs;
    va_start(stderrArgs, format);
    va_copy(fileArgs, stderrArgs);
    vfprintf(stderr, format, stderrArgs);
    vfprintf(logFile, format, fileArgs);
    va_end(fileArgs);
    va_end(stderrArgs);

    fflush(logFile);
}


bool initLogging()
{
    logFile = fopen("output.log", "w");
    if(!logFile)
    {
        logFile = stderr;
        printf("Error: Unable to create log file\n");
        // NOTE: We don't necessarily need to fail here, we can probably survive without a log file
    }
    return true;
}

void deinitLogging()
{
    fflush(stderr);
    fflush(logFile);
    fclose(logFile);
}
