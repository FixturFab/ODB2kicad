#pragma once
#include "event.h"
class wxTimer : public wxEvtHandler {
public:
    wxTimer() = default;
    wxTimer(wxEvtHandler*, int = wxID_ANY) {}
    virtual ~wxTimer() = default;
    bool Start(int milliseconds = -1, bool oneShot = false) { return true; }
    void Stop() {}
    bool IsRunning() const { return false; }
    int GetInterval() const { return 0; }
    bool IsOneShot() const { return false; }
};
// wxTimerEvent defined in event.h
#define wxEVT_TIMER 100
