// 09 12 2026
/* purpose
* Implements the repository-wide UTC and monotonic time helpers.
* Owns gmtime conversion, millisecond formatting, and folder/filename stamps.
* Does NOT open files, choose retention, or log anything itself.
*/
#include "utils/time-format.h"

#include <chrono>
#include <cstdio>
#include <ctime>

namespace MiMitaTime {

namespace {

void utcBrokenDown(std::tm& out, std::chrono::system_clock::time_point& now)
{
    now = std::chrono::system_clock::now();
    const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
    gmtime_s(&out, &seconds);
}

} // namespace

std::string utcIso8601Millis()
{
    std::chrono::system_clock::time_point now;
    std::tm utc{};
    utcBrokenDown(utc, now);
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count() % 1000;
    char buf[40]{};
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
                  utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
                  utc.tm_hour, utc.tm_min, utc.tm_sec,
                  static_cast<int>(millis < 0 ? millis + 1000 : millis));
    return buf;
}

std::string utcIso8601Seconds()
{
    std::chrono::system_clock::time_point now;
    std::tm utc{};
    utcBrokenDown(utc, now);
    char buf[32]{};
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &utc);
    return buf;
}

std::string utcDateFolder()
{
    std::chrono::system_clock::time_point now;
    std::tm utc{};
    utcBrokenDown(utc, now);
    char buf[16]{};
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
                  utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday);
    return buf;
}

std::string utcCompactStamp()
{
    std::chrono::system_clock::time_point now;
    std::tm utc{};
    utcBrokenDown(utc, now);
    char buf[24]{};
    std::snprintf(buf, sizeof(buf), "%04d%02d%02d_%02d%02d%02d",
                  utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
                  utc.tm_hour, utc.tm_min, utc.tm_sec);
    return buf;
}

std::uint64_t monotonicMillis()
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

} // namespace MiMitaTime
