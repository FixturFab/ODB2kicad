#pragma once
// Stub for WASM build
#include <string>

namespace google {
namespace protobuf {

class EnumDescriptor;

inline const std::string& internal_EnumName(const void*, int) {
    static const std::string empty;
    return empty;
}

template <typename T>
const std::string& EnumName(T) {
    static const std::string empty;
    return empty;
}

inline const EnumDescriptor* GetEnumDescriptor(const void*) { return nullptr; }

}  // namespace protobuf
}  // namespace google
