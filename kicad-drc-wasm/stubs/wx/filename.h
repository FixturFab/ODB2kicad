#pragma once

#include "string.h"
#include "translation.h"
#include <string>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <sys/stat.h>

enum wxPathFormat { wxPATH_NATIVE = 0, wxPATH_UNIX, wxPATH_DOS, wxPATH_MAC };

// File existence flags
#define wxFILE_EXISTS_REGULAR  0x0001
#define wxFILE_EXISTS_DIR      0x0002
#define wxFILE_EXISTS_SYMLINK  0x0004
#define wxFILE_EXISTS_ANY      0xFFFF

// wxFileName flags
#define wxS_DIR_DEFAULT 0777
#define wxPATH_MKDIR_FULL 0x0001
#define wxPATH_GET_VOLUME 0x0001
#define wxPATH_GET_SEPARATOR 0x0002

// wxPATH_NORM flags for Normalize()
enum {
    wxPATH_NORM_ENV_VARS = 0x0001,
    wxPATH_NORM_DOTS     = 0x0002,
    wxPATH_NORM_TILDE    = 0x0004,
    wxPATH_NORM_CASE     = 0x0008,
    wxPATH_NORM_ABSOLUTE = 0x0010,
    wxPATH_NORM_LONG     = 0x0020,
    wxPATH_NORM_SHORTCUT = 0x0040,
    wxPATH_NORM_ALL      = 0x00ff
};

// Standalone file utility functions
inline wxString wxPathOnly(const wxString& path) {
    std::string s = path.c_str();
    auto sep = s.rfind('/');
    return sep != std::string::npos ? wxString(s.substr(0, sep)) : wxString();
}
inline bool wxCopyFile(const wxString& src, const wxString& dst, bool overwrite = true) {
    (void)overwrite;
    // Stub: in WASM we don't need real file copying
    return false;
}

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
    void Assign(const wxString& dir, const wxString& name) {
        m_dir = dir;
        m_name = name;
        m_ext.clear();
        // Parse name for extension
        std::string s = name.c_str();
        auto dot = s.rfind('.');
        if(dot != std::string::npos) {
            m_name = wxString(s.substr(0, dot));
            m_ext = wxString(s.substr(dot + 1));
        }
    }
    void Assign(const wxString& dir, const wxString& name, const wxString& ext) {
        m_dir = dir;
        m_name = name;
        m_ext = ext;
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
    void ClearExt() { m_ext.clear(); }
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

    bool MakeAbsolute(const wxString& cwd = wxString()) {
        if(!m_dir.IsEmpty() && m_dir[0] != '/') {
            wxString base = cwd.IsEmpty() ? wxString("/") : cwd;
            if(!base.IsEmpty() && base.Last() != '/') base += '/';
            m_dir = base + m_dir;
        }
        return true;
    }
    bool MakeRelativeTo(const wxString& pathBase = wxString(), wxPathFormat format = wxPATH_NATIVE) { return true; }
    bool IsAbsolute(wxPathFormat format = wxPATH_NATIVE) const {
        return !m_dir.IsEmpty() && m_dir[0] == '/';
    }
    bool IsRelative(wxPathFormat format = wxPATH_NATIVE) const { return !IsAbsolute(format); }
    bool Normalize(int flags = 0, const wxString& cwd = wxString(), wxPathFormat format = wxPATH_NATIVE) { return true; }

    static bool DirExists(const wxString& path) {
        struct stat st;
        return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
    }
    static wxFileName DirName(const wxString& dir, wxPathFormat format = wxPATH_NATIVE) {
        wxFileName fn;
        fn.AssignDir(dir);
        return fn;
    }
    static wxFileName FileName(const wxString& file, wxPathFormat format = wxPATH_NATIVE) {
        return wxFileName(file);
    }
    static unsigned long long GetSize(const wxString& path) {
        struct stat st;
        if(stat(path.c_str(), &st) == 0) return (unsigned long long)st.st_size;
        return 0;
    }
    unsigned long long GetSize() const {
        return GetSize(GetFullPath());
    }
    bool Mkdir(int perm = wxS_DIR_DEFAULT, int flags = 0) {
        return mkdir(GetFullPath().c_str(), perm) == 0 || errno == EEXIST;
    }
    static bool Mkdir(const wxString& path, int perm = wxS_DIR_DEFAULT, int flags = 0) {
        return mkdir(path.c_str(), perm) == 0 || errno == EEXIST;
    }

    static wxString CreateTempFileName(const wxString& prefix) {
        static int counter = 0;
        char buf[256];
        snprintf(buf, sizeof(buf), "/tmp/%s_%d_%d", prefix.c_str(), (int)getpid(), counter++);
        return wxString(buf);
    }

    void AssignTempFileName(const wxString& prefix) {
        wxString temp = CreateTempFileName(prefix);
        Assign(temp);
    }

    static wxString GetCwd() { return wxString("/"); }
    static wxString GetTempDir() { return wxString("/tmp"); }
    static char GetPathSeparator() { return '/'; }
    static wxString GetPathSeparators() { return wxString("/"); }

    bool HasExt() const { return !m_ext.IsEmpty(); }
    bool HasName() const { return !m_name.IsEmpty(); }

    // Existence check with type flags
    bool Exists(int flags = wxFILE_EXISTS_ANY) const {
        struct stat st;
        if(lstat(GetFullPath().c_str(), &st) != 0) return false;
        if((flags & wxFILE_EXISTS_SYMLINK) && S_ISLNK(st.st_mode)) return true;
        if((flags & wxFILE_EXISTS_DIR) && S_ISDIR(st.st_mode)) return true;
        if((flags & wxFILE_EXISTS_REGULAR) && S_ISREG(st.st_mode)) return true;
        return (flags == wxFILE_EXISTS_ANY);
    }

    static bool Exists(const wxString& path, int flags = wxFILE_EXISTS_ANY) {
        wxFileName fn(path);
        return fn.Exists(flags);
    }

    // Minimal wxDateTime-like wrapper for GetModificationTime
    struct DateTime {
        struct Value { long long GetValue() const { return val; } long long val = 0; };
        bool IsValid() const { return val.val != 0; }
        Value GetValue() const { return val; }
        Value val;
    };

    DateTime GetModificationTime() const {
        struct stat st;
        DateTime dt;
        if(stat(GetFullPath().c_str(), &st) == 0) {
            dt.val.val = (long long)st.st_mtime * 1000LL;
        }
        return dt;
    }

    static wxString GetForbiddenChars(wxPathFormat format = wxPATH_NATIVE) {
        return wxString("*?\"<>|");
    }

    void AppendDir(const wxString& dir) {
        if(!m_dir.IsEmpty() && m_dir.Last() != '/') m_dir += '/';
        m_dir += dir;
    }

    void Clear() { m_dir.Clear(); m_name.Clear(); m_ext.Clear(); m_dirs.clear(); }

    // Directory component accessors
    wxArrayString GetDirs() const {
        parseDirs();
        return m_dirs;
    }
    size_t GetDirCount() const { parseDirs(); return m_dirs.GetCount(); }
    void RemoveDir(size_t pos) { parseDirs(); if(pos < m_dirs.size()) m_dirs.erase(m_dirs.begin() + pos); rebuildDir(); }
    void InsertDir(size_t pos, const wxString& dir) { parseDirs(); m_dirs.insert(m_dirs.begin() + pos, dir); rebuildDir(); }
    void RemoveLastDir() { parseDirs(); if(!m_dirs.empty()) { m_dirs.pop_back(); rebuildDir(); } }

    // Volume (not applicable on WASM/POSIX)
    bool HasVolume() const { return false; }
    wxString GetVolume() const { return wxString(); }
    void SetVolume(const wxString&) {}

    void AssignDir(const wxString& dir) {
        m_dir = dir;
        m_name.Clear();
        m_ext.Clear();
        m_dirsParsed = false;
    }

    wxString GetAbsolutePath(const wxString& cwd = wxString()) const {
        wxFileName fn(*this);
        fn.MakeAbsolute(cwd);
        return fn.GetFullPath();
    }

    wxString GetPathWithSep(wxPathFormat format = wxPATH_NATIVE) const {
        wxString p = GetPath(0, format);
        if(!p.IsEmpty() && p.Last() != '/') p += '/';
        return p;
    }

    static bool IsDirReadable(const wxString& dir) {
        return access(dir.c_str(), R_OK) == 0;
    }

    static bool IsDirWritable(const wxString& dir) {
        return access(dir.c_str(), W_OK) == 0;
    }

    static bool IsCaseSensitive(wxPathFormat format = wxPATH_NATIVE) { return true; }

    static wxArrayString GetAllowedChars() { return wxArrayString(); }

    // Other
    bool IsDirReadable() const { return DirExists(); }
    bool IsDirWritable() const { return access(GetPath().c_str(), W_OK) == 0; }
    bool IsFileReadable() const { return FileExists(); }
    bool IsFileWritable() const { return FileExists(); }
    bool SameAs(const wxFileName& other) const { return GetFullPath() == other.GetFullPath(); }

    // GetLongPath (windows-only concept, identity on posix)
    wxString GetLongPath() const { return GetFullPath(); }
    wxString GetShortPath() const { return GetFullPath(); }

    static wxString GetHomeDir() {
        const char* home = getenv("HOME");
        return home ? wxString(home) : wxString("/");
    }

    // Size (static and instance versions defined earlier)

    bool SetTimes(void*, void*, void*) { return false; }
    bool Touch() { return false; }

private:
    wxString m_dir;
    wxString m_name;
    wxString m_ext;
    mutable wxArrayString m_dirs;
    mutable bool m_dirsParsed = false;

    void parseDirs() const {
        if(m_dirsParsed) return;
        m_dirsParsed = true;
        m_dirs.Clear();
        std::string d = m_dir.c_str();
        std::string token;
        for(size_t i = 0; i < d.size(); i++) {
            if(d[i] == '/') {
                if(!token.empty()) { m_dirs.Add(wxString(token)); token.clear(); }
            } else {
                token += d[i];
            }
        }
        if(!token.empty()) m_dirs.Add(wxString(token));
    }
    void rebuildDir() {
        m_dir.Clear();
        for(size_t i = 0; i < m_dirs.size(); i++) {
            if(i > 0) m_dir += '/';
            m_dir += m_dirs[i];
        }
        m_dirsParsed = true;
    }
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
