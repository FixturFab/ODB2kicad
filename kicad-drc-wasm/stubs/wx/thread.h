#pragma once
#include <mutex>
#include <thread>
#include <condition_variable>

class wxMutex {
public:
    void Lock() { m_mutex.lock(); }
    void Unlock() { m_mutex.unlock(); }
    bool TryLock() { return m_mutex.try_lock(); }
    std::mutex& GetMutex() { return m_mutex; }
private:
    std::mutex m_mutex;
};

class wxMutexLocker {
public:
    wxMutexLocker(wxMutex& mutex) : m_lock(mutex.GetMutex()) {}
    bool IsOk() const { return true; }
private:
    std::lock_guard<std::mutex> m_lock;
};

class wxCriticalSection {
public:
    void Enter() { m_mutex.lock(); }
    void Leave() { m_mutex.unlock(); }
private:
    std::mutex m_mutex;
};

class wxCriticalSectionLocker {
public:
    wxCriticalSectionLocker(wxCriticalSection& cs) : m_cs(cs) { m_cs.Enter(); }
    ~wxCriticalSectionLocker() { m_cs.Leave(); }
private:
    wxCriticalSection& m_cs;
};

class wxThread {
public:
    enum { JOINABLE = 0, DETACHED = 1 };
    static bool IsMain() { return true; }
    static void Sleep(unsigned long ms) {}
    static unsigned long GetCurrentId() { return 1; }
};

class wxSemaphore {
public:
    wxSemaphore(int initialCount = 0, int maxCount = 0) {}
    int Wait() { return 0; }
    int TryWait() { return 0; }
    int WaitTimeout(unsigned long) { return 0; }
    int Post() { return 0; }
};

class wxCondition {
public:
    wxCondition(wxMutex&) {}
    int Wait() { return 0; }
    int WaitTimeout(unsigned long) { return 0; }
    int Signal() { return 0; }
    int Broadcast() { return 0; }
};
