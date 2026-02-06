#pragma once

#include "string.h"
#include <ctime>

// Forward declare
class wxTimeSpan;

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
    bool IsShorterThan(const wxTimeSpan& other) const { return m_ms < other.m_ms; }
    bool IsLongerThan(const wxTimeSpan& other) const { return m_ms > other.m_ms; }
    bool IsNegative() const { return m_ms < 0; }
    bool IsPositive() const { return m_ms > 0; }
    wxTimeSpan Abs() const { return wxTimeSpan(0, 0, 0, m_ms >= 0 ? m_ms : -m_ms); }
private:
    long long m_ms;
};

class wxDateTime
{
public:
    enum TZ { Local, GMT, UTC = GMT };
    enum WeekDay { Sun, Mon, Tue, Wed, Thu, Fri, Sat };
    enum Month { Jan, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec };

    wxDateTime() : m_time(0) {}
    wxDateTime(time_t t) : m_time(t) {}
    wxDateTime(int day, Month month, int year, int hour = 0, int min = 0, int sec = 0, int ms = 0) {
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

    static int GetNumberOfDays(Month month, int year) {
        static const int days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        if (month == Feb && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
            return 29;
        return days[month];
    }

    bool IsValid() const { return m_time != 0; }
    time_t GetTicks() const { return m_time; }

    wxString Format(const wxString& fmt = wxString("%c")) const {
        char buf[256];
        struct tm* t = localtime(&m_time);
        if(t) strftime(buf, sizeof(buf), fmt.c_str(), t);
        else buf[0] = '\0';
        return wxString(buf);
    }

    wxString FormatDate() const { return Format("%x"); }
    wxString FormatISODate() const { return Format("%Y-%m-%d"); }
    wxString FormatISOTime() const { return Format("%H:%M:%S"); }
    wxString FormatISOCombined(char sep = 'T') const {
        return FormatISODate() + wxString(1, sep) + FormatISOTime();
    }

    int GetYear() const { struct tm* t = localtime(&m_time); return t ? t->tm_year + 1900 : 0; }
    Month GetMonth() const { struct tm* t = localtime(&m_time); return t ? (Month)t->tm_mon : Jan; }
    int GetDay() const { struct tm* t = localtime(&m_time); return t ? t->tm_mday : 0; }
    int GetHour() const { struct tm* t = localtime(&m_time); return t ? t->tm_hour : 0; }
    int GetMinute() const { struct tm* t = localtime(&m_time); return t ? t->tm_min : 0; }
    int GetSecond() const { struct tm* t = localtime(&m_time); return t ? t->tm_sec : 0; }

    bool operator<(const wxDateTime& o) const { return m_time < o.m_time; }
    bool operator>(const wxDateTime& o) const { return m_time > o.m_time; }
    bool operator<=(const wxDateTime& o) const { return m_time <= o.m_time; }
    bool operator>=(const wxDateTime& o) const { return m_time >= o.m_time; }
    bool operator==(const wxDateTime& o) const { return m_time == o.m_time; }
    bool operator!=(const wxDateTime& o) const { return m_time != o.m_time; }

    wxTimeSpan operator-(const wxDateTime& o) const {
        long diff = (long)difftime(m_time, o.m_time);
        return wxTimeSpan::Seconds(diff);
    }

    wxDateTime operator-(const wxTimeSpan& span) const {
        return wxDateTime(m_time - span.GetSeconds());
    }
    wxDateTime operator+(const wxTimeSpan& span) const {
        return wxDateTime(m_time + span.GetSeconds());
    }

    bool IsSameDate(const wxDateTime& o) const {
        struct tm a_tm, b_tm;
        struct tm* a = localtime_r(&m_time, &a_tm);
        struct tm* b = localtime_r(&o.m_time, &b_tm);
        if(!a || !b) return false;
        return a->tm_year == b->tm_year && a->tm_mon == b->tm_mon && a->tm_mday == b->tm_mday;
    }

    const char* ParseFormat(const wxString& str, const wxString& format = wxString("%c")) {
        struct tm t = {};
        const char* result = strptime(str.c_str(), format.c_str(), &t);
        if(result) m_time = mktime(&t);
        return result;
    }

private:
    time_t m_time;
};
