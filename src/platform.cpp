#if defined(_WIN32) && !defined(__unix__)
#include "platform_win32.cpp"

#elif !defined(_WIN32) && defined(__unix__)
#include "platform_unix.cpp"

#endif
