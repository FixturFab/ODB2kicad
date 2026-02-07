#pragma once
#include "string.h"
#include <functional>

class wxHashTable {
public:
    static unsigned long MakeKey( const wxString& s ) {
        unsigned long hash = 0;
        for( size_t i = 0; i < s.length(); i++ )
            hash = hash * 31 + (unsigned char)s[i];
        return hash;
    }
};
