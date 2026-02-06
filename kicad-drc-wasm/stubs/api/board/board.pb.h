#pragma once
// Stub for WASM build
#include <api/board/board_types.pb.h>
#include <api/common/types/base_types.pb.h>
#include <string>
#include <vector>

namespace kiapi {
namespace board {

// ── Enums ──────────────────────────────────────────────────────────────
enum BoardStackupLayerType : int {
    BSLT_UNKNOWN = 0,
    BSLT_COPPER = 1,
    BSLT_DIELECTRIC = 2,
    BSLT_SILKSCREEN = 3,
    BSLT_SOLDERMASK = 4,
    BSLT_SOLDERPASTE = 5,
    BSLT_UNDEFINED = 7,
};

// ── Stub repeated field helper ─────────────────────────────────────────
template<typename T>
class StubRepeatedField {
public:
    T* Add() { items_.emplace_back(); return &items_.back(); }
    int size() const { return (int)items_.size(); }
private:
    std::vector<T> items_;
};

// ── BoardStackupDielectricProperties ───────────────────────────────────
class BoardStackupDielectricProperties {
public:
    void set_epsilon_r(double) {}
    void set_loss_tangent(double) {}
    void set_material_name(const std::string&) {}
    void set_material_name(const char*) {}
    kiapi::common::types::Distance* mutable_thickness() {
        static kiapi::common::types::Distance d;
        return &d;
    }
};

// ── BoardStackupDielectricLayer ────────────────────────────────────────
class BoardStackupDielectricLayer {
public:
    BoardStackupDielectricLayer* New() { return this; }
    StubRepeatedField<BoardStackupDielectricProperties>* mutable_layer() { return &layers_; }
private:
    StubRepeatedField<BoardStackupDielectricProperties> layers_;
};

// ── BoardStackupCopperLayer ────────────────────────────────────────────
class BoardStackupCopperLayer {
public:
};

// ── BoardStackupLayer ──────────────────────────────────────────────────
class BoardStackupLayer {
public:
    kiapi::common::types::Distance* mutable_thickness() {
        static kiapi::common::types::Distance d;
        return &d;
    }
    void set_layer(kiapi::board::types::BoardLayer) {}
    void set_layer(int) {}
    void set_type(BoardStackupLayerType) {}
    void set_type(int) {}
    void set_material_name(const std::string&) {}
    void set_material_name(const char*) {}
    BoardStackupDielectricLayer* mutable_dielectric() { return &dielectric_; }
    BoardStackupCopperLayer* mutable_copper() { return &copper_; }
private:
    BoardStackupDielectricLayer dielectric_;
    BoardStackupCopperLayer copper_;
};

// ── BoardStackup ───────────────────────────────────────────────────────
class BoardStackup : public google::protobuf::Message {
public:
    StubRepeatedField<BoardStackupLayer>* mutable_layers() { return &layers_; }
private:
    StubRepeatedField<BoardStackupLayer> layers_;
};

// ── GraphicsDefaults (forward declared in real header) ──────────────────
class GraphicsDefaults : public google::protobuf::Message {
public:
};

} // namespace board
} // namespace kiapi
