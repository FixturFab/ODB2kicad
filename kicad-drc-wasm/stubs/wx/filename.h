#pragma once

#include "string.h"
#include <string>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <sys/stat.h>

enum wxPathFormat { wxPATH_NATIVE = 0, wxPATH_UNIX, wxPATH_DOS, wxPATH_MAC };

class wxFileName
{
public:
    wxFileName() = default;
    wxFileName(const wxString& fullpath) { Assign(fullpath); }
    wxFileName(const wxString& dir, const wxString& name) {
        m_dir = dir;
        m_name = name;
    }
    wxFileName(const wxString& dir, const wxString& name, const wxString& ext) {
        m_dir = dir;
        m_name = name;
        m_ext = ext;
    }

    void Assign(const wxString& fullpath) {
        std::string s = fullpath.c_str();
        auto sep = s.rfind('/');
        if(sep != std::string::npos) {
            m_dir = wxString(s.substr(0, sep));
            s = s.substr(sep + 1);
        }
        auto dot = s.rfind('.');
        if(dot != std::string::npos) {
            m_name = wxString(s.substr(0, dot));
            m_ext = wxString(s.substr(dot + 1));
        } else {
            m_name = wxString(s);
        }
    }

    bool IsOk() const { return !m_name.IsEmpty() || !m_dir.IsEmpty(); }
    bool FileExists() const {
        struct stat st;
        return stat(GetFullPath().c_str(), &st) == 0;
    }
    bool DirExists() const {
        struct stat st;
        return stat(GetPath().c_str(), &st) == 0 && S_ISDIR(st.st_mode);
    }
    static bool FileExists(const wxString& path) {
        struct stat st;
        return stat(path.c_str(), &st) == 0;
    }

    wxString GetFullPath(wxPathFormat format = wxPATH_NATIVE) const {
        wxString result;
        if(!m_dir.IsEmpty()) {
            result = m_dir;
            if(result.Last() != '/') result += '/';
        }
        result += m_name;
        if(!m_ext.IsEmpty()) { result += '.'; result += m_ext; }
        return result;
    }

    wxString GetPath(int flags = 0, wxPathFormat format = wxPATH_NATIVE) const { return m_dir; }
    wxString GetName() const { return m_name; }
    wxString GetExt() const { return m_ext; }
    wxString GetFullName() const {
        if(m_ext.IsEmpty()) return m_name;
        return m_name + "." + m_ext;
    }

    void SetPath(const wxString& path) { m_dir = path; }
    void SetName(const wxString& name) { m_name = name; }
    void SetExt(const wxString& ext) { m_ext = ext; }
    void SetFullName(const wxString& fullname) {
        std::string s = fullname.c_str();
        auto dot = s.rfind('.');
        if(dot != std::string::npos) {
            m_name = wxString(s.substr(0, dot));
            m_ext = wxString(s.substr(dot + 1));
        } else {
            m_name = fullname;
            m_ext.Clear();
        }
    }

    void MakeAbsolute(const wxString& cwd = wxString()) { /* no-op in WASM */ }
    bool Normalize(int flags = 0, const wxString& cwd = wxString()) { return true; }

    static wxString CreateTempFileName(const wxString& prefix) {
        static int counter = 0;
        char buf[256];
        snprintf(buf, sizeof(buf), "/tmp/%s_%d_%d", prefix.c_str(), (int)getpid(), counter++);
        return wxString(buf);
    }

    static wxString GetCwd() { return wxString("/"); }
    static wxString GetTempDir() { return wxString("/tmp"); }
    static char GetPathSeparator() { return '/'; }
    static wxString GetPathSeparators() { return wxString("/"); }

    bool HasExt() const { return !m_ext.IsEmpty(); }
    bool HasName() const { return !m_name.IsEmpty(); }

    static wxString GetForbiddenChars(wxPathFormat format = wxPATH_NATIVE) {
        return wxString("*?\"<>|");
    }

    void AppendDir(const wxString& dir) {
        if(!m_dir.IsEmpty() && m_dir.Last() != '/') m_dir += '/';
        m_dir += dir;
    }

private:
    wxString m_dir;
    wxString m_name;
    wxString m_ext;
};

inline bool wxRemoveFile(const wxString& path) { return remove(path.c_str()) == 0; }
inline bool wxFileExists(const wxString& path) { return wxFileName::FileExists(path); }
inline bool wxDirExists(const wxString& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}
inline bool wxMkdir(const wxString& path, int perm = 0777) {
    return mkdir(path.c_str(), perm) == 0;
}
inline bool wxRmdir(const wxString& path) { return rmdir(path.c_str()) == 0; }
inline bool wxRenameFile(const wxString& from, const wxString& to) {
    return rename(from.c_str(), to.c_str()) == 0;
}
