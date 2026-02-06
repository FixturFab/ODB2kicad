#pragma once
// Stub envelope.pb.h for WASM build
#include <google/protobuf/message.h>

namespace kiapi {
namespace common {
class Envelope : public ::google::protobuf::Message {
public:
    Envelope() = default;
};
}  // namespace common
}  // namespace kiapi
