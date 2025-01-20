#include "timeapi.h"

#include <sys/time.h>
#include <iomanip>
#include <cstring>
#include <sstream>

errno_t ctime_s(char *buf, size_t size, const time_t *time)
{
    const char *t = asctime(localtime(time));
    strncpy(buf, t, size);
    return 0;
}

DWORD timeGetTime()
{
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_usec / 1000;
}

/// Returns the number of ticks since an undefined time (usually system startup).
DWORD GetTickCount()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    const uint64_t ticks = (uint64_t)(ts.tv_nsec / 1000000) + ((uint64_t)ts.tv_sec * 1000ull);
    return ticks;
}

void GetLocalTime(SYSTEMTIME *t)
{
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    time_t tim = ts.tv_sec;

    t->wMilliseconds = ts.tv_nsec / 1000000;

    tm *local = localtime(&tim);

    t->wSecond = local->tm_sec;
    t->wMinute = local->tm_min;
    t->wHour = local->tm_hour;
    t->wDayOfWeek = local->tm_wday;
    t->wDay = local->tm_mday;
    t->wMonth = local->tm_mon + 1;
    t->wYear = local->tm_year;
}

int GetDateFormat(
    LCID /* Locale */, DWORD /* dwFlags */, CONST SYSTEMTIME * /* lpDate */, LPCSTR /* lpFormat */, LPSTR lpDateStr,
    int cchDate)
{
    std::time_t t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);
    std::ostringstream ss;
    ss << std::put_time(&tm, "%F");
    const std::string str = ss.str();
    strncpy(lpDateStr, str.c_str(), cchDate);
    return cchDate; // not 100% sure, but it is never used
}

int GetTimeFormat(
    LCID /* Locale */, DWORD /* dwFlags */, CONST SYSTEMTIME * /* lpTime */, LPCSTR /* lpFormat */, LPSTR lpTimeStr,
    int cchTime)
{
    std::time_t t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);
    std::ostringstream ss;
    ss << std::put_time(&tm, "%T");
    const std::string str = ss.str();
    strncpy(lpTimeStr, str.c_str(), cchTime);
    return cchTime; // not 100% sure, but it is never used
}
