#ifndef _LOGGING_H
#define _LOGGING_H

#include <stdio.h>
#include <stdarg.h>


/* Log Levels:
 * Term: Logs only to the terminal, used for very high-frequency or low-impact messages
 * Info: Logs to terminal and file, use for information that could be useful but not a problem
 * Warn: Logs to terminal and file, use for errors/situations that are bad but recoverable
 * Fail: Logs to terminal and file, use for errors/failures that we cannot recover from
 */
enum LogLevel
{
    LOG_TERM,
    LOG_INFO,
    LOG_WARN,
    LOG_FAIL
};

bool initLogging(const char* filename);
void deinitLogging();

void _log(LogLevel level, const char* filePath, int lineNumber, const char* msgFormat, ...);

#ifdef NDEBUG
#define logTerm(MSG, ...);
#define logInfo(MSG, ...);
#define logWarn(MSG, ...);
#define logFail(MSG, ...);
#else
#define logTerm(MSG, ...);     _log(LOG_TERM, __FILE__, __LINE__, MSG, ##__VA_ARGS__);
#define logInfo(MSG, ...);     _log(LOG_INFO, __FILE__, __LINE__, MSG, ##__VA_ARGS__);
#define logWarn(MSG, ...);     _log(LOG_WARN, __FILE__, __LINE__, MSG, ##__VA_ARGS__);
#define logFail(MSG, ...);     _log(LOG_FAIL, __FILE__, __LINE__, MSG, ##__VA_ARGS__);
#endif // NDEBUG

#endif
