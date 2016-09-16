#include "logging.h"

#include <stdio.h>

#include "happyhttp.h"

#include "platform.h"

static const char* logFileName;
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
}
