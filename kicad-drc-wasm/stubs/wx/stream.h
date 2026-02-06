#pragma once

#include "string.h"
#include <cstdio>
#include <cstring>

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
};

class wxOutputStream : public wxStreamBase
{
public:
    virtual ~wxOutputStream() = default;
    virtual wxOutputStream& Write(const void* buffer, size_t size) { return *this; }
    virtual size_t LastWrite() const { return 0; }
    virtual void Sync() {}
    virtual size_t OnSysWrite(const void*, size_t) { return 0; }
};

class wxMemoryInputStream : public wxInputStream
{
public:
    wxMemoryInputStream(const void* data, size_t len) : m_data((const char*)data), m_len(len), m_pos(0) {}
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
private:
    std::string m_data;
};

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
