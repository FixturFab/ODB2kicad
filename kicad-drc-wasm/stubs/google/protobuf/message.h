#pragma once
// Minimal protobuf stub for WASM build
#include <string>
#include <cstdint>
#include <vector>

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
    void CopyFrom(const Message&) {}
};

template<typename T>
class RepeatedField {
public:
    RepeatedField() = default;
    void Add(T val) { data_.push_back(val); }
    int size() const { return (int)data_.size(); }
    T Get(int i) const { return data_[i]; }
    const T& operator[](int i) const { return data_[i]; }
    T& operator[](int i) { return data_[i]; }
    auto begin() const { return data_.begin(); }
    auto end() const { return data_.end(); }
    auto begin() { return data_.begin(); }
    auto end() { return data_.end(); }
    void Clear() { data_.clear(); }
private:
    std::vector<T> data_;
};

template<typename T>
class RepeatedPtrField {
public:
    RepeatedPtrField() = default;
    T* Add() { data_.emplace_back(); return &data_.back(); }
    int size() const { return (int)data_.size(); }
    const T& Get(int i) const { return data_[i]; }
    const T& operator[](int i) const { return data_[i]; }
    T& operator[](int i) { return data_[i]; }
    auto begin() const { return data_.begin(); }
    auto end() const { return data_.end(); }
    auto begin() { return data_.begin(); }
    auto end() { return data_.end(); }
    void Clear() { data_.clear(); }
private:
    std::vector<T> data_;
};

namespace internal {
class AnyMetadata;
}

namespace io {
class CodedInputStream {};
class CodedOutputStream {};
class ZeroCopyOutputStream {
public:
    virtual ~ZeroCopyOutputStream() = default;
    virtual bool Next(void**, int*) { return false; }
    virtual void BackUp(int) {}
    virtual int64_t ByteCount() const { return 0; }
};
class StringOutputStream : public ZeroCopyOutputStream {
public:
    StringOutputStream(std::string*) {}
};
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
