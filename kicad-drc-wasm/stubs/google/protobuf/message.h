#pragma once
// Minimal protobuf stub for WASM build
#include <string>
#include <cstdint>

#define PROTOBUF_NAMESPACE_ID google::protobuf

namespace google {
namespace protobuf {

class MessageLite {
public:
    virtual ~MessageLite() = default;
    virtual std::string GetTypeName() const { return ""; }
};

class Message : public MessageLite {
public:
    virtual ~Message() = default;
};

namespace internal {
class AnyMetadata;
}

namespace io {
class CodedInputStream {};
class CodedOutputStream {};
}

class Arena {
public:
    template <typename T> class InternalHelper {};
};

class Descriptor {};
class Reflection {};
class UnknownFieldSet {};

}  // namespace protobuf
}  // namespace google
