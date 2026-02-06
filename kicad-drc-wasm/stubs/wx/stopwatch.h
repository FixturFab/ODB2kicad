#pragma once
#include <chrono>

class wxStopWatch {
public:
    wxStopWatch() { Start(); }
    void Start(long ms = 0) { m_start = std::chrono::steady_clock::now(); }
    void Pause() {}
    void Resume() {}
    long Time() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - m_start).count();
    }
    long TimeInMicro() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(now - m_start).count();
    }
private:
    std::chrono::steady_clock::time_point m_start;
};

inline long wxGetLocalTimeMillis() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}
