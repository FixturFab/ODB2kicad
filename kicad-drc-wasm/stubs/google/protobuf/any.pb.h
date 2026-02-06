#pragma once
// Minimal protobuf Any stub for WASM build
#include <google/protobuf/message.h>
#include <string>

namespace google {
namespace protobuf {

class Any : public Message {
public:
    Any() = default;
    virtual ~Any() = default;

    const std::string& type_url() const { return type_url_; }
    void set_type_url(const std::string& url) { type_url_ = url; }

    const std::string& value() const { return value_; }
    void set_value(const std::string& v) { value_ = v; }

    bool PackFrom(const Message&) { return false; }
    bool UnpackTo(Message*) const { return false; }
    template<typename T> bool Is() const { return false; }

private:
    std::string type_url_;
    std::string value_;
};

}  // namespace protobuf
}  // namespace google
