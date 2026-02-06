#pragma once
#include <google/protobuf/message.h>

namespace google {
namespace protobuf {
class Empty : public Message {
public:
    Empty() = default;
};
}  // namespace protobuf
}  // namespace google
