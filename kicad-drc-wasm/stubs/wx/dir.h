#pragma once
#include "string.h"
#include <dirent.h>
#include <sys/stat.h>

// Global constants matching wxDir flags
#define wxDIR_FILES  1
#define wxDIR_DIRS   2
#define wxDIR_HIDDEN 4
#define wxDIR_DOTDOT 8
#define wxDIR_DEFAULT (wxDIR_FILES | wxDIR_DIRS | wxDIR_HIDDEN)

// Dir traversal result
enum wxDirTraverseResult {
    wxDIR_IGNORE = -1,
    wxDIR_STOP = 0,
    wxDIR_CONTINUE = 1
};

// Dir traverser base class
class wxDirTraverser {
public:
    virtual ~wxDirTraverser() = default;
    virtual wxDirTraverseResult OnFile(const wxString& filename) = 0;
    virtual wxDirTraverseResult OnDir(const wxString& dirname) { return wxDIR_CONTINUE; }
    virtual wxDirTraverseResult OnOpenError(const wxString& dirname) { return wxDIR_IGNORE; }
};

class wxDir {
public:
    enum { FILES = wxDIR_FILES, DIRS = wxDIR_DIRS, HIDDEN = wxDIR_HIDDEN, DOTDOT = wxDIR_DOTDOT };
    wxDir() : m_dir(nullptr) {}
    wxDir(const wxString& dir) : m_dir(nullptr) { Open(dir); }
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
    wxString GetNameWithSep() const {
        if(m_path.IsEmpty() || m_path.Last() == '/') return m_path;
        return m_path + "/";
    }
    static bool Exists(const wxString& dir) {
        DIR* d = opendir(dir.c_str());
        if(d) { closedir(d); return true; }
        return false;
    }

    size_t Traverse(wxDirTraverser& sink, const wxString& filespec = wxString(), int flags = wxDIR_DEFAULT) const {
        if(!m_dir) return 0;
        size_t count = 0;
        rewinddir(m_dir);
        struct dirent* entry;
        while((entry = readdir(m_dir))) {
            if(entry->d_name[0] == '.' && (entry->d_name[1] == '\0' || (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))
                continue;
            wxString fullpath = m_path + "/" + wxString(entry->d_name);
            struct stat st;
            if(stat(fullpath.c_str(), &st) != 0) continue;
            wxDirTraverseResult result;
            if(S_ISDIR(st.st_mode)) {
                if(flags & wxDIR_DIRS) {
                    result = sink.OnDir(fullpath);
                    if(result == wxDIR_STOP) break;
                    if(result == wxDIR_CONTINUE) {
                        wxDir subdir(fullpath);
                        if(subdir.IsOpened())
                            count += subdir.Traverse(sink, filespec, flags);
                    }
                }
            } else {
                if(flags & wxDIR_FILES) {
                    result = sink.OnFile(fullpath);
                    count++;
                    if(result == wxDIR_STOP) break;
                }
            }
        }
        return count;
    }

private:
    wxString m_path;
    mutable DIR* m_dir;
};
