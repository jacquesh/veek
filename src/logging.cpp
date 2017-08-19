#include <stdio.h>
#include <time.h>

#include "happyhttp.h"

#include "assert.h"
#include "logging.h"
#include "platform.h"

static const char* logFileName;
static FILE* logFile;

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

void _log(LogLevel level, const char* filePath, int lineNumber, const char* msgFormat, ...)
{
    assert((level >= LOG_TERM) && (level <= LOG_FAIL));
    const char* logLevelLabels[] = {"TERM", "INFO", "WARN", "FAIL"};

    const char* fileName = getFileNameFromPath(filePath);

    DateTime currentTime = getLocalDateTime();
    char timeBuffer[64];
    snprintf(timeBuffer, sizeof(timeBuffer), "%02u:%02u:%02u.%03u",
             currentTime.Hour, currentTime.Minute, currentTime.Second, currentTime.Millisecond);

    fprintf(stderr, "%s [%s] %16s:%-3d - ", timeBuffer, logLevelLabels[level], fileName, lineNumber);
    fprintf(logFile, "%s [%s] %16s:%-3d - ", timeBuffer, logLevelLabels[level], fileName, lineNumber);

    va_list stderrArgs;
    va_list fileArgs;
    va_start(stderrArgs, msgFormat);
    va_copy(fileArgs, stderrArgs);
    vfprintf(stderr, msgFormat, stderrArgs);
    if(level != LOG_TERM)
    {
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

    // Upload log file to server
    /*
    logFile = fopen(logFileName, "rb");
    if(logFile)
    {
        fseek(logFile, 0, SEEK_END);
        size_t logLength = ftell(logFile);
        rewind(logFile);
        int maxFileLength = 500*1024;
        if(logLength > maxFileLength)
            logLength = maxFileLength;

        unsigned char* logData = new unsigned char[logLength];
        size_t dataRead = 0;
        while(dataRead < logLength)
        {
            size_t newData = fread((void*)(logData+dataRead), 1, logLength-dataRead, logFile);
            dataRead += newData;
        }
        fclose(logFile);

        char username[257];
        getCurrentUserName(257,username);
        const char* headers[] =
        {
            "username", username,
            0
        };
        happyhttp::Connection conn("veek.ddns.net", 80);
        conn.request("POST", "/", headers, logData, logLength);

        delete[] logData;
    }
    */
}
