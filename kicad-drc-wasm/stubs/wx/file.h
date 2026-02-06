#pragma once

#include "string.h"
#include "strconv.h"
#include <cstdio>

class wxFile
{
public:
    enum OpenMode { read, write, read_write, write_append, write_excl };

    wxFile() : m_fp(nullptr) {}
    wxFile(const wxString& filename, OpenMode mode = read) {
        Open(filename, mode);
    }
    ~wxFile() { Close(); }

    bool Open(const wxString& filename, OpenMode mode = read) {
        const char* m = "r";
        switch(mode) {
            case read: m = "r"; break;
            case write: m = "w"; break;
            case read_write: m = "r+"; break;
            case write_append: m = "a"; break;
            case write_excl: m = "wx"; break;
        }
        m_fp = fopen(filename.c_str(), m);
        return m_fp != nullptr;
    }
    void Close() { if(m_fp) { fclose(m_fp); m_fp = nullptr; } }
    bool IsOpened() const { return m_fp != nullptr; }

    ssize_t Read(void* buffer, size_t count) {
        return m_fp ? fread(buffer, 1, count, m_fp) : -1;
    }
    size_t Write(const void* buffer, size_t count) {
        return m_fp ? fwrite(buffer, 1, count, m_fp) : 0;
    }
    bool Write(const wxString& s) {
        return Write(s.c_str(), s.length()) == s.length();
    }

    bool Flush() { return m_fp && fflush(m_fp) == 0; }
    long Tell() const { return m_fp ? ftell(m_fp) : -1; }
    long Seek(long offset, int origin = SEEK_SET) {
        return m_fp ? fseek(m_fp, offset, origin) : -1;
    }
    long Length() const {
        if(!m_fp) return -1;
        long cur = ftell(m_fp);
        fseek(m_fp, 0, SEEK_END);
        long len = ftell(m_fp);
        fseek(m_fp, cur, SEEK_SET);
        return len;
    }
    bool Eof() const { return !m_fp || feof(m_fp); }

    static bool Exists(const wxString& name) {
        FILE* f = fopen(name.c_str(), "r");
        if(f) { fclose(f); return true; }
        return false;
    }

private:
    FILE* m_fp;
};

class wxTempFile
{
public:
    wxTempFile() = default;
    wxTempFile(const wxString& name) : m_name(name) { Open(name); }
    bool Open(const wxString& name) {
        m_name = name;
        m_tempName = name + ".tmp";
        m_fp = fopen(m_tempName.c_str(), "w");
        return m_fp != nullptr;
    }
    bool Write(const wxString& s) {
        return m_fp && fwrite(s.c_str(), 1, s.length(), m_fp) == s.length();
    }
    bool Commit() {
        if(m_fp) { fclose(m_fp); m_fp = nullptr; }
        return rename(m_tempName.c_str(), m_name.c_str()) == 0;
    }
    void Discard() {
        if(m_fp) { fclose(m_fp); m_fp = nullptr; }
        remove(m_tempName.c_str());
    }
    ~wxTempFile() { if(m_fp) Discard(); }
private:
    FILE* m_fp = nullptr;
    wxString m_name;
    wxString m_tempName;
};

class wxFFile
{
public:
    wxFFile() : m_fp(nullptr) {}
    wxFFile(const wxString& filename, const wxString& mode = wxString("r")) {
        Open(filename, mode);
    }
    ~wxFFile() { Close(); }

    bool Open(const wxString& filename, const wxString& mode = wxString("r")) {
        m_fp = fopen(filename.c_str(), mode.c_str());
        return m_fp != nullptr;
    }
    void Close() { if(m_fp) { fclose(m_fp); m_fp = nullptr; } }
    bool IsOpened() const { return m_fp != nullptr; }
    FILE* fp() const { return m_fp; }

    size_t Read(void* buffer, size_t count) {
        return m_fp ? fread(buffer, 1, count, m_fp) : 0;
    }
    bool ReadAll(wxString* str) {
        if(!m_fp || !str) return false;
        fseek(m_fp, 0, SEEK_END);
        long len = ftell(m_fp);
        fseek(m_fp, 0, SEEK_SET);
        std::string buf(len, '\0');
        fread(&buf[0], 1, len, m_fp);
        *str = wxString(buf);
        return true;
    }
    bool ReadAll(wxString* str, const wxMBConv&) {
        return ReadAll(str);
    }
    bool Seek(long offset, int origin = SEEK_SET) {
        return m_fp && fseek(m_fp, offset, origin) == 0;
    }
    size_t Write(const wxString& s) {
        return m_fp ? fwrite(s.c_str(), 1, s.length(), m_fp) : 0;
    }
    size_t Write(const void* buf, size_t count) {
        return m_fp ? fwrite(buf, 1, count, m_fp) : 0;
    }
    long Length() const {
        if(!m_fp) return -1;
        long cur = ftell(m_fp);
        fseek(m_fp, 0, SEEK_END);
        long len = ftell(m_fp);
        fseek(m_fp, cur, SEEK_SET);
        return len;
    }
    bool Eof() const { return !m_fp || feof(m_fp); }
    long Tell() const { return m_fp ? ftell(m_fp) : -1; }

private:
    FILE* m_fp;
};

inline FILE* wxFopen(const wxString& path, const wxString& mode) {
    return fopen(path.c_str(), mode.c_str());
}
