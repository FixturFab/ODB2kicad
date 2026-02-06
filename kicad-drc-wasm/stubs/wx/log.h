#pragma once

#include "string.h"
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

// Logging macros - no-ops or stderr
#define wxLogError(...) do { fprintf(stderr, "Error: "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while(0)
#define wxLogWarning(...) do {} while(0)
#define wxLogMessage(...) do {} while(0)
#define wxLogVerbose(...) do {} while(0)
#define wxLogDebug(...) do {} while(0)
#define wxLogTrace(...) do {} while(0)
#define wxLogInfo(...) do {} while(0)
#define wxLogStatus(...) do {} while(0)
#define wxLogSysError(...) do {} while(0)
#define wxLogGeneric(...) do {} while(0)
#define wxLogFatalError(...) do {} while(0)
