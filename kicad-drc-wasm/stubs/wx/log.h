#pragma once

#include "string.h"
#include "strconv.h"
#include <cstdio>

// wxLog stub - redirect to stderr
class wxLog
{
public:
    virtual ~wxLog() = default;
    static wxLog* GetActiveTarget() { return nullptr; }
    static wxLog* SetActiveTarget(wxLog* log) { return nullptr; }
    static void FlushActive() {}
    static void SetLogLevel(unsigned long level) {}
    static unsigned long GetLogLevel() { return 0; }
    static void EnableLogging(bool enable = true) {}
    static bool IsEnabled() { return false; }
    static void SetVerbose(bool verbose = true) {}
    static bool GetVerbose() { return false; }
    static void AddTraceMask(const wxString&) {}
    static void RemoveTraceMask(const wxString&) {}
    static bool IsAllowedTraceMask(const wxString&) { return false; }
    static void SetComponentLevel(const wxString&, unsigned long) {}
    static bool IsLevelEnabled(unsigned long, const wxString&) { return false; }
    void Flush() {}
};

class wxLogNull
{
public:
    wxLogNull() {}
    ~wxLogNull() {}
};

class wxLogStderr : public wxLog
{
public:
    wxLogStderr(FILE* = nullptr) {}
};

typedef unsigned long wxLogLevel;

struct wxLogRecordInfo {
    const char* filename = "";
    int line = 0;
    const char* func = "";
    const char* component = "";
};

class wxLogInterposerTemp : public wxLog
{
public:
    wxLogInterposerTemp() {}
protected:
    virtual void DoLogRecord(wxLogLevel, const wxString&, const wxLogRecordInfo&) {}
};

// Log level constants
enum {
    wxLOG_FatalError = 0,
    wxLOG_Error,
    wxLOG_Warning,
    wxLOG_Message,
    wxLOG_Status,
    wxLOG_Info,
    wxLOG_Debug,
    wxLOG_Trace,
    wxLOG_Progress,
    wxLOG_User = 100,
    wxLOG_Max = 10000
};

#ifndef wxLOG_COMPONENT
#define wxLOG_COMPONENT "default"
#endif

// Variadic logging functions (no-ops)
inline void wxVLogWarning(const char*, va_list) {}
inline void wxVLogError(const char*, va_list) {}
inline void wxVLogDebug(const char*, va_list) {}
inline void wxVLogTrace(const char*, va_list) {}

// Helper to convert wxString or const char* to const char* for fprintf
inline const char* _wxLogStr(const wxString& s) { return s.c_str(); }
inline const char* _wxLogStr(const char* s) { return s; }

// Logging macros - no-ops or stderr
// Use inline function overloads to handle both wxString and const char* args
#define wxLogError(fmt, ...) do { (void)(fmt); } while(0)
#define wxLogWarning(fmt, ...) do { (void)(fmt); } while(0)
#define wxLogMessage(fmt, ...) do { (void)(fmt); } while(0)
#define wxLogVerbose(fmt, ...) do { (void)(fmt); } while(0)
#define wxLogDebug(fmt, ...) do { (void)(fmt); } while(0)
#define wxLogTrace(fmt, ...) do { (void)(fmt); } while(0)
#define wxLogInfo(fmt, ...) do { (void)(fmt); } while(0)
#define wxLogStatus(fmt, ...) do { (void)(fmt); } while(0)
#define wxLogSysError(fmt, ...) do { (void)(fmt); } while(0)
#define wxLogGeneric(fmt, ...) do { (void)(fmt); } while(0)
#define wxLogFatalError(fmt, ...) do { (void)(fmt); } while(0)
