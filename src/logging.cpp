#include <stdio.h>
#include <time.h>

#include "assert.h"
#include "logging.h"
#include "platform.h"

static const char* logFileName;
static FILE* logFile = nullptr;

static const char* getFileNameFromPath(const char* filePath)
{
    const char* baseName = filePath;

    for(const char* currentName=filePath; *currentName != 0; ++currentName)
    {
        char currentChar = *currentName;
        if((currentChar == '/') || (currentChar == '\\'))
        {
            baseName = currentName+1;
        }
    }

    return baseName;
}

void _log(LogLevel level, bool logToTerminal, bool logToFile,
          const char* filePath, int lineNumber, const char* msgFormat, ...)
{
    assert((level >= LOG_DBUG) && (level <= LOG_FAIL));
    const char* logLevelLabels[] = {"DBUG", "INFO", "WARN", "FAIL"};

    const char* fileName = getFileNameFromPath(filePath);

    Platform::DateTime currentTime = Platform::GetLocalDateTime();
    char timeBuffer[64];
    snprintf(timeBuffer, sizeof(timeBuffer), "%02u:%02u:%02u.%03u",
             currentTime.Hour, currentTime.Minute, currentTime.Second, currentTime.Millisecond);

    va_list stderrArgs;
    va_list fileArgs;
    va_start(stderrArgs, msgFormat);
    va_copy(fileArgs, stderrArgs);
    if(logToTerminal)
    {
        fprintf(stderr, "%s [%s] %16s:%-3d - ",
                timeBuffer, logLevelLabels[level], fileName, lineNumber);
        vfprintf(stderr, msgFormat, stderrArgs);
    }
    if(logToFile && (logFile != nullptr))
    {
        fprintf(logFile, "%s [%s] %16s:%-3d - ",
                timeBuffer, logLevelLabels[level], fileName, lineNumber);
        vfprintf(logFile, msgFormat, fileArgs);
    }
    va_end(fileArgs);
    va_end(stderrArgs);

    fflush(logFile);
}


bool initLogging(const char* filename)
{
    logFileName = filename;
    logFile = fopen(logFileName, "w");
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
