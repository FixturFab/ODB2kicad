#pragma once

#include <cstring>
#include <cstddef>
#include <memory>

class wxCharBuffer
{
public:
    wxCharBuffer() : m_data(nullptr), m_len(0) {}
    wxCharBuffer(size_t len) : m_len(len) {
        m_data = new char[len + 1];
        m_data[0] = '\0';
    }
    wxCharBuffer(const char* s) {
        m_len = s ? strlen(s) : 0;
        m_data = new char[m_len + 1];
        if(s) memcpy(m_data, s, m_len + 1);
        else m_data[0] = '\0';
    }
    wxCharBuffer(const wxCharBuffer& o) {
        m_len = o.m_len;
        m_data = new char[m_len + 1];
        memcpy(m_data, o.m_data, m_len + 1);
    }
    ~wxCharBuffer() { delete[] m_data; }

    wxCharBuffer& operator=(const wxCharBuffer& o) {
        if(this != &o) {
            delete[] m_data;
            m_len = o.m_len;
            m_data = new char[m_len + 1];
            memcpy(m_data, o.m_data, m_len + 1);
        }
        return *this;
    }

    operator const char*() const { return m_data; }
    char* data() { return m_data; }
    const char* data() const { return m_data; }
    size_t length() const { return m_len; }

    bool extend(size_t len) {
        char* newData = new char[len + 1];
        if(m_data) { memcpy(newData, m_data, std::min(m_len, len)); delete[] m_data; }
        m_data = newData;
        m_len = len;
        return true;
    }

private:
    char* m_data;
    size_t m_len;
};

class wxWCharBuffer
{
public:
    wxWCharBuffer() : m_data(nullptr) {}
    wxWCharBuffer(const wchar_t* s) {
        if(s) {
            size_t len = wcslen(s);
            m_data = new wchar_t[len + 1];
            memcpy(m_data, s, (len + 1) * sizeof(wchar_t));
        } else m_data = nullptr;
    }
    ~wxWCharBuffer() { delete[] m_data; }
    operator const wchar_t*() const { return m_data; }
private:
    wchar_t* m_data;
};

class wxMemoryBuffer
{
public:
    wxMemoryBuffer(size_t size = 0) { if(size) m_data.reserve(size); }
    void AppendData(const void* data, size_t len) {
        const char* p = (const char*)data;
        m_data.insert(m_data.end(), p, p + len);
    }
    void* GetData() { return m_data.data(); }
    const void* GetData() const { return m_data.data(); }
    size_t GetDataLen() const { return m_data.size(); }
    void SetDataLen(size_t len) { m_data.resize(len); }
    size_t GetBufSize() const { return m_data.capacity(); }
    void SetBufSize(size_t size) { m_data.reserve(size); }
    void Clear() { m_data.clear(); }
    bool IsEmpty() const { return m_data.empty(); }
    void* GetWriteBuf(size_t len) { m_data.resize(len); return m_data.data(); }
    void UngetWriteBuf(size_t len) { m_data.resize(len); }
    char* GetAppendBuf(size_t len) { size_t old = m_data.size(); m_data.resize(old + len); return m_data.data() + old; }
    void UngetAppendBuf(size_t len) { /* keep as-is */ }
private:
    std::vector<char> m_data;
};
