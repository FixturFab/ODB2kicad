#pragma once
// Stub board_types.pb.h for WASM build
#include <string>
#include <vector>
#include <api/common/types/base_types.pb.h>

#ifndef KIAPI_IMPORTEXPORT
#define KIAPI_IMPORTEXPORT
#endif

namespace kiapi {
namespace board {
namespace types {

enum BoardLayer : int {
    BL_UNDEFINED = 0,
    BL_F_Cu = 1,
    BL_B_Cu = 2
};

enum PadStackType : int { PST_NORMAL = 0 };
enum UnconnectedLayerRemoval : int { ULR_KEEP = 0 };
enum PadStackShape : int { PSS_CIRCLE = 0, PSS_RECT = 1, PSS_OVAL = 2 };
enum ZoneConnectionStyle : int { ZCS_INHERITED = 0 };
enum SolderMaskMode : int { SMM_INHERITED = 0 };
enum ViaCoveringMode : int { VCM_INHERITED = 0 };

class PadStackLayer {
public:
    PadStackLayer() = default;
    void set_shape(PadStackShape s) { shape_ = s; }
    PadStackShape shape() const { return shape_; }
    void set_layer(BoardLayer l) { layer_ = l; }
    BoardLayer layer() const { return layer_; }
    kiapi::common::types::Vector2* mutable_size() { return &size_; }
    const kiapi::common::types::Vector2& size() const { return size_; }
private:
    PadStackShape shape_ = PSS_CIRCLE;
    BoardLayer layer_ = BL_UNDEFINED;
    kiapi::common::types::Vector2 size_;
};

class DrillProperties {
public:
    DrillProperties() = default;
    kiapi::common::types::Vector2* mutable_diameter() { return &diam_; }
    const kiapi::common::types::Vector2& diameter() const { return diam_; }
private:
    kiapi::common::types::Vector2 diam_;
};

class PadStack {
public:
    PadStack() = default;
    PadStackLayer* add_copper_layers() { layers_.emplace_back(); return &layers_.back(); }
    int copper_layers_size() const { return (int)layers_.size(); }
    const std::vector<PadStackLayer>& copper_layers() const { return layers_; }
    DrillProperties* mutable_drill() { return &drill_; }
    const DrillProperties& drill() const { return drill_; }
    bool has_drill() const { return true; }
private:
    std::vector<PadStackLayer> layers_;
    DrillProperties drill_;
};

class Net { public: Net() = default; };
class NetCode { public: NetCode() = default; };
class Track { public: Track() = default; };
class Arc { public: Arc() = default; };
class Via { public: Via() = default; };
class Pad { public: Pad() = default; };
class Zone { public: Zone() = default; };
class FootprintInstance { public: FootprintInstance() = default; };
class Footprint { public: Footprint() = default; };
class Field { public: Field() = default; };
class FieldId { public: FieldId() = default; };
class Group { public: Group() = default; };
class Dimension { public: Dimension() = default; };
class BoardText { public: BoardText() = default; };
class BoardTextBox { public: BoardTextBox() = default; };
class BoardGraphicShape { public: BoardGraphicShape() = default; };
class ReferenceImage { public: ReferenceImage() = default; };
class Barcode { public: Barcode() = default; };

}  // namespace types
}  // namespace board
}  // namespace kiapi
