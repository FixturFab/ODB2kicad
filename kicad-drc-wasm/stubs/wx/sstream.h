#pragma once
#include "stream.h"
#include "string.h"

class wxStringInputStream : public wxInputStream
{
public:
    wxStringInputStream(const wxString& s) : m_str(s), m_pos(0) {}
    wxInputStream& Read(void* buffer, size_t size) override {
        size_t avail = m_str.length() - m_pos;
        size_t toRead = std::min(size, avail);
        memcpy(buffer, m_str.c_str() + m_pos, toRead);
        m_pos += toRead;
        m_lastRead = toRead;
        return *this;
    }
    size_t LastRead() const override { return m_lastRead; }
    bool Eof() const override { return m_pos >= m_str.length(); }
private:
    wxString m_str;
    size_t m_pos;
    size_t m_lastRead = 0;
};
