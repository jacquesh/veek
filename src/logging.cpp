#include "logging.h"

#include <stdio.h>

static FILE* logFile;

void _log(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(logFile, format, args);
    va_end(args);

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
    fflush(logFile);
    fclose(logFile);
}
