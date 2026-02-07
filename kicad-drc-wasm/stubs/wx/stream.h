#pragma once

#include "string.h"
#include <cstdio>
#include <cstring>

typedef long long wxFileOffset;

class wxStreamBase
{
public:
    virtual ~wxStreamBase() = default;
    virtual bool IsOk() const { return true; }
    virtual size_t GetSize() const { return 0; }
};

class wxInputStream : public wxStreamBase
{
public:
    virtual ~wxInputStream() = default;
    virtual wxInputStream& Read(void* buffer, size_t size) { return *this; }
    virtual size_t LastRead() const { return 0; }
    virtual bool Eof() const { return true; }
    virtual char GetC() { return 0; }
    virtual size_t OnSysRead(void*, size_t) { return 0; }
    virtual wxFileOffset GetLength() const { return (wxFileOffset)GetSize(); }
};

class wxOutputStream : public wxStreamBase
{
public:
    virtual ~wxOutputStream() = default;
    virtual wxOutputStream& Write(const void* buffer, size_t size) { return *this; }
    virtual size_t LastWrite() const { return 0; }
    virtual void Sync() {}
    virtual size_t OnSysWrite(const void*, size_t) { return 0; }
    void PutC(char c) { Write(&c, 1); }
};

class wxMemoryOutputStream;

class wxMemoryInputStream : public wxInputStream
{
public:
    wxMemoryInputStream(const void* data, size_t len) : m_data((const char*)data), m_len(len), m_pos(0) {}
    // Construct from wxMemoryOutputStream (copies data)
    wxMemoryInputStream(wxMemoryOutputStream& stream);
    wxMemoryInputStream(const wxMemoryOutputStream& stream);
    wxInputStream& Read(void* buffer, size_t size) override {
        size_t toRead = std::min(size, m_len - m_pos);
        memcpy(buffer, m_data + m_pos, toRead);
        m_pos += toRead;
        m_lastRead = toRead;
        return *this;
    }
    size_t LastRead() const override { return m_lastRead; }
    bool Eof() const override { return m_pos >= m_len; }
    size_t GetSize() const override { return m_len; }
private:
    const char* m_data;
    size_t m_len;
    size_t m_pos;
    size_t m_lastRead = 0;
};

class wxStreamBuffer;

class wxMemoryOutputStream : public wxOutputStream
{
public:
    wxMemoryOutputStream() = default;
    wxOutputStream& Write(const void* buffer, size_t size) override {
        m_data.append((const char*)buffer, size);
        return *this;
    }
    size_t GetSize() const override { return m_data.size(); }
    size_t CopyTo(void* buffer, size_t len) const {
        size_t toCopy = std::min(len, m_data.size());
        memcpy(buffer, m_data.data(), toCopy);
        return toCopy;
    }
    const char* GetData() const { return m_data.data(); }
    wxFileOffset GetLength() const { return (wxFileOffset)m_data.size(); }
    inline wxStreamBuffer* GetOutputStreamBuffer() const;
private:
    std::string m_data;
    friend class wxMemoryInputStream;
};

// Out-of-line constructors for wxMemoryInputStream from wxMemoryOutputStream
inline wxMemoryInputStream::wxMemoryInputStream(wxMemoryOutputStream& stream)
    : m_data(nullptr), m_len(0), m_pos(0) {
    m_len = stream.GetSize();
    if (m_len > 0) {
        char* buf = new char[m_len];
        stream.CopyTo(buf, m_len);
        m_data = buf;
    }
}
inline wxMemoryInputStream::wxMemoryInputStream(const wxMemoryOutputStream& stream)
    : m_data(nullptr), m_len(0), m_pos(0) {
    m_len = stream.GetSize();
    if (m_len > 0) {
        char* buf = new char[m_len];
        stream.CopyTo(buf, m_len);
        m_data = buf;
    }
}

class wxFileInputStream : public wxInputStream
{
public:
    wxFileInputStream(const wxString& filename) {
        m_fp = fopen(filename.c_str(), "rb");
    }
    ~wxFileInputStream() { if(m_fp) fclose(m_fp); }
    bool IsOk() const override { return m_fp != nullptr; }
    wxInputStream& Read(void* buffer, size_t size) override {
        if(m_fp) m_lastRead = fread(buffer, 1, size, m_fp);
        else m_lastRead = 0;
        return *this;
    }
    size_t LastRead() const override { return m_lastRead; }
    bool Eof() const override { return !m_fp || feof(m_fp); }
private:
    FILE* m_fp = nullptr;
    size_t m_lastRead = 0;
};

class wxFileOutputStream : public wxOutputStream
{
public:
    wxFileOutputStream(const wxString& filename) {
        m_fp = fopen(filename.c_str(), "wb");
    }
    ~wxFileOutputStream() { if(m_fp) fclose(m_fp); }
    bool IsOk() const override { return m_fp != nullptr; }
    wxOutputStream& Write(const void* buffer, size_t size) override {
        if(m_fp) fwrite(buffer, 1, size, m_fp);
        return *this;
    }
private:
    FILE* m_fp = nullptr;
};

// wxFFileInputStream - same as wxFileInputStream in our stub
class wxFFileInputStream : public wxInputStream
{
public:
    wxFFileInputStream(const wxString& filename) {
        m_fp = fopen(filename.c_str(), "rb");
    }
    wxFFileInputStream(const wxString& filename, const wxString& mode) {
        m_fp = fopen(filename.c_str(), mode.c_str());
    }
    wxFFileInputStream(FILE* fp) : m_fp(fp) {}
    ~wxFFileInputStream() { if(m_fp) fclose(m_fp); }
    bool IsOk() const override { return m_fp != nullptr; }
    wxInputStream& Read(void* buffer, size_t size) override {
        if(m_fp) m_lastRead = fread(buffer, 1, size, m_fp);
        else m_lastRead = 0;
        return *this;
    }
    size_t LastRead() const override { return m_lastRead; }
    bool Eof() const override { return !m_fp || feof(m_fp); }
    size_t GetSize() const override {
        if(!m_fp) return 0;
        long cur = ftell(m_fp);
        fseek(m_fp, 0, SEEK_END);
        long sz = ftell(m_fp);
        fseek(m_fp, cur, SEEK_SET);
        return (size_t)sz;
    }
    wxFileOffset GetLength() const {
        if(!m_fp) return -1;
        long cur = ftell(m_fp);
        fseek(m_fp, 0, SEEK_END);
        long sz = ftell(m_fp);
        fseek(m_fp, cur, SEEK_SET);
        return (wxFileOffset)sz;
    }
    bool ReadAll(void* buffer, size_t size) {
        if(!m_fp) return false;
        size_t read = fread(buffer, 1, size, m_fp);
        return read == size;
    }
private:
    FILE* m_fp = nullptr;
    size_t m_lastRead = 0;
};

// wxFFileOutputStream - similar to wxFileOutputStream but uses FILE*
class wxFFileOutputStream : public wxOutputStream
{
public:
    wxFFileOutputStream(const wxString& filename) {
        m_fp = fopen(filename.c_str(), "wb");
    }
    wxFFileOutputStream(const wxString& filename, const wxString& mode) {
        m_fp = fopen(filename.c_str(), mode.c_str());
    }
    wxFFileOutputStream(const wxString& filename, const char* mode) {
        m_fp = fopen(filename.c_str(), mode);
    }
    wxFFileOutputStream(FILE* fp) : m_fp(fp) {}
    ~wxFFileOutputStream() { if(m_fp) fclose(m_fp); }
    bool IsOk() const override { return m_fp != nullptr; }
    wxOutputStream& Write(const void* buffer, size_t size) override {
        if(m_fp) fwrite(buffer, 1, size, m_fp);
        return *this;
    }
    bool WriteAll(const void* buffer, size_t size) {
        if(!m_fp) return false;
        return fwrite(buffer, 1, size, m_fp) == size;
    }
private:
    FILE* m_fp = nullptr;
};

class wxBufferedInputStream : public wxInputStream
{
public:
    wxBufferedInputStream(wxInputStream& stream) : m_stream(stream) {}
    wxInputStream& Read(void* buffer, size_t size) override { return m_stream.Read(buffer, size); }
    size_t LastRead() const override { return m_stream.LastRead(); }
    bool Eof() const override { return m_stream.Eof(); }
private:
    wxInputStream& m_stream;
};

class wxStreamBuffer
{
public:
    enum BufMode { read, write, read_write };
    wxStreamBuffer(wxInputStream& stream, BufMode mode = read) {}
    wxStreamBuffer(wxOutputStream& stream, BufMode mode = write) {}
    wxStreamBuffer(BufMode mode = read) {}
    virtual ~wxStreamBuffer() = default;

    void SetBufferIO(size_t) {}
    void* GetBufferStart() const { return nullptr; }
    void* GetBufferEnd() const { return nullptr; }
    void* GetBufferPos() const { return nullptr; }
    size_t GetBufferSize() const { return 0; }
    size_t GetDataLeft() const { return 0; }
    void ResetBuffer() {}
    void Truncate() {}
    bool FillBuffer() { return false; }
    bool FlushBuffer() { return false; }
};

// Out-of-line definition for wxMemoryOutputStream::GetOutputStreamBuffer
inline wxStreamBuffer* wxMemoryOutputStream::GetOutputStreamBuffer() const {
    // Return nullptr - stub only; DRC code paths should not depend on the actual buffer
    return nullptr;
}

// wxStdInputStream - C++ std::istream wrapper around wxInputStream
// Used by nlohmann::json::parse() in json_settings.cpp
class wxStdInputStreamBuffer : public std::streambuf {
public:
    wxStdInputStreamBuffer(wxInputStream& stream) : m_stream(stream) {}
protected:
    int_type underflow() override {
        if(m_stream.Eof()) return traits_type::eof();
        char c;
        m_stream.Read(&c, 1);
        if(m_stream.LastRead() == 0) return traits_type::eof();
        m_putback = c;
        setg(&m_putback, &m_putback, &m_putback + 1);
        return traits_type::to_int_type(c);
    }
private:
    wxInputStream& m_stream;
    char m_putback = 0;
};

class wxStdInputStream : public std::istream {
public:
    wxStdInputStream(wxInputStream& stream) : std::istream(&m_buf), m_buf(stream) {}
private:
    wxStdInputStreamBuffer m_buf;
};
