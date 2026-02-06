#pragma once
#include "string.h"
#include <dirent.h>

class wxDir {
public:
    enum { FILES = 1, DIRS = 2, HIDDEN = 4, DOTDOT = 8 };
    wxDir() : m_dir(nullptr) {}
    wxDir(const wxString& dir) { Open(dir); }
    ~wxDir() { Close(); }
    bool Open(const wxString& dir) {
        Close();
        m_path = dir;
        m_dir = opendir(dir.c_str());
        return m_dir != nullptr;
    }
    void Close() { if(m_dir) { closedir(m_dir); m_dir = nullptr; } }
    bool IsOpened() const { return m_dir != nullptr; }
    bool GetFirst(wxString* filename, const wxString& = wxString(), int = FILES) const {
        if(!m_dir) return false;
        rewinddir(m_dir);
        return GetNext(filename);
    }
    bool GetNext(wxString* filename) const {
        if(!m_dir) return false;
        struct dirent* entry;
        while((entry = readdir(m_dir))) {
            if(entry->d_name[0] == '.' && (entry->d_name[1] == '\0' || (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))
                continue;
            *filename = wxString(entry->d_name);
            return true;
        }
        return false;
    }
    bool HasFiles(const wxString& = wxString()) const { return true; }
    bool HasSubDirs(const wxString& = wxString()) const { return true; }
    wxString GetName() const { return m_path; }
    static bool Exists(const wxString& dir) {
        DIR* d = opendir(dir.c_str());
        if(d) { closedir(d); return true; }
        return false;
    }
private:
    wxString m_path;
    mutable DIR* m_dir;
};
