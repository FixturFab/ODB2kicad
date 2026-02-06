#pragma once

#include "string.h"
#include <ctime>

class wxDateTime
{
public:
    enum TZ { Local, GMT, UTC = GMT };
    enum WeekDay { Sun, Mon, Tue, Wed, Thu, Fri, Sat };
    enum Month { Jan, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec };

    wxDateTime() : m_time(0) {}
    wxDateTime(time_t t) : m_time(t) {}
    wxDateTime(int day, Month month, int year, int hour = 0, int min = 0, int sec = 0) {
        struct tm t = {};
        t.tm_year = year - 1900;
        t.tm_mon = month;
        t.tm_mday = day;
        t.tm_hour = hour;
        t.tm_min = min;
        t.tm_sec = sec;
        m_time = mktime(&t);
    }

    static wxDateTime Now() { return wxDateTime(time(nullptr)); }
    static wxDateTime Today() { return Now(); }
    static wxDateTime UNow() { return Now(); }

    bool IsValid() const { return m_time != 0; }
    time_t GetTicks() const { return m_time; }

    wxString Format(const wxString& fmt = wxString("%c")) const {
        char buf[256];
        struct tm* t = localtime(&m_time);
        strftime(buf, sizeof(buf), fmt.c_str(), t);
        return wxString(buf);
    }

    wxString FormatISODate() const { return Format("%Y-%m-%d"); }
    wxString FormatISOTime() const { return Format("%H:%M:%S"); }
    wxString FormatISOCombined(char sep = 'T') const {
        return FormatISODate() + wxString(1, sep) + FormatISOTime();
    }

    int GetYear() const { struct tm* t = localtime(&m_time); return t->tm_year + 1900; }
    Month GetMonth() const { struct tm* t = localtime(&m_time); return (Month)t->tm_mon; }
    int GetDay() const { struct tm* t = localtime(&m_time); return t->tm_mday; }
    int GetHour() const { struct tm* t = localtime(&m_time); return t->tm_hour; }
    int GetMinute() const { struct tm* t = localtime(&m_time); return t->tm_min; }
    int GetSecond() const { struct tm* t = localtime(&m_time); return t->tm_sec; }

    bool operator<(const wxDateTime& o) const { return m_time < o.m_time; }
    bool operator>(const wxDateTime& o) const { return m_time > o.m_time; }
    bool operator==(const wxDateTime& o) const { return m_time == o.m_time; }
    bool operator!=(const wxDateTime& o) const { return m_time != o.m_time; }

private:
    time_t m_time;
};

class wxTimeSpan
{
public:
    wxTimeSpan() : m_ms(0) {}
    wxTimeSpan(long hours, long mins = 0, long secs = 0, long ms = 0)
        : m_ms(hours*3600000LL + mins*60000LL + secs*1000LL + ms) {}
    static wxTimeSpan Seconds(long s) { return wxTimeSpan(0, 0, s); }
    static wxTimeSpan Minutes(long m) { return wxTimeSpan(0, m); }
    static wxTimeSpan Hours(long h) { return wxTimeSpan(h); }
    long GetMilliseconds() const { return m_ms; }
    long GetSeconds() const { return m_ms / 1000; }
private:
    long long m_ms;
};
