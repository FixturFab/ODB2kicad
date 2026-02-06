#pragma once

#include "defs.h"
#include "string.h"
#include "event.h"
#include <cstdlib>

class wxAppConsole : public wxEvtHandler
{
public:
    virtual ~wxAppConsole() = default;
    virtual bool OnInit() { return true; }
    virtual int OnRun() { return 0; }
    virtual int OnExit() { return 0; }
    virtual void OnInitCmdLine(class wxCmdLineParser&) {}
    virtual bool OnCmdLineParsed(class wxCmdLineParser&) { return true; }
    static wxAppConsole* GetInstance() { return s_instance; }
    static void SetInstance(wxAppConsole* app) { s_instance = app; }
    int argc = 0;
    char** argv = nullptr;
private:
    static inline wxAppConsole* s_instance = nullptr;
};

class wxApp : public wxAppConsole
{
public:
    virtual ~wxApp() = default;
};

inline wxAppConsole* wxTheApp = nullptr;

inline bool wxInitialize(int argc = 0, char** argv = nullptr) { return true; }
inline void wxUninitialize() {}

// Macros
#define wxIMPLEMENT_APP_NO_MAIN(cls)
#define wxIMPLEMENT_APP(cls)
#define wxDECLARE_APP(cls)
#define IMPLEMENT_APP_NO_MAIN(cls)
#define IMPLEMENT_APP(cls)
#define DECLARE_APP(cls)

inline wxApp& wxGetApp() { static wxApp app; return app; }
