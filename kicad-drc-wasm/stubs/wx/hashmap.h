#pragma once
#include <unordered_map>
#define WX_DECLARE_STRING_HASH_MAP(value, name) typedef std::unordered_map<wxString, value, wxString::StdHash> name
#define WX_DECLARE_HASH_MAP(key, value, hash, eq, name) typedef std::unordered_map<key, value> name
