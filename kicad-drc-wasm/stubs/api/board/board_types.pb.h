#pragma once
// Stub board_types.pb.h for WASM build
// All protobuf message types are stubs that compile but don't serialize
#include <string>
#include <vector>
#include <cstdint>
#include <api/common/types/base_types.pb.h>

#ifndef KIAPI_IMPORTEXPORT
#define KIAPI_IMPORTEXPORT
#endif

namespace kiapi {
namespace board {
namespace types {

// Use type aliases instead of 'using namespace' to avoid ambiguity with global ::KIID
using kiapi::common::types::Vector2;
using kiapi::common::types::Vector3D;
using kiapi::common::types::Distance;
using kiapi::common::types::Angle;
using kiapi::common::types::Ratio;
using kiapi::common::types::PolySet;
using kiapi::common::types::Text;
using kiapi::common::types::TextBox;
using kiapi::common::types::GraphicShape;
using kiapi::common::types::LibraryIdentifier;
using kiapi::common::types::SheetPath;
using kiapi::common::types::LockedState;
using kiapi::common::types::LS_UNLOCKED;
using kiapi::common::types::LS_LOCKED;
using kiapi::common::types::AxisAlignment;
using kiapi::common::types::AA_HORIZONTAL;
using kiapi::common::types::AA_VERTICAL;
using kiapi::common::types::AA_X_AXIS;
using kiapi::common::types::AA_Y_AXIS;
using kiapi::common::types::ElectricalPinType;
using kiapi::common::types::EPT_INPUT;
using kiapi::common::types::GraphicFillType;
using kiapi::common::types::GFT_NONE;
using kiapi::common::types::GFT_FILLED;
using kiapi::common::types::StrokeLineStyle;
using kiapi::common::types::HorizontalAlignment;
using kiapi::common::types::VerticalAlignment;
using kiapi::common::types::GraphicShapeType;

// ── Enums ──────────────────────────────────────────────────────────────
enum BoardLayer : int {
    BL_UNKNOWN = 0, BL_UNDEFINED = 0, BL_F_Cu = 1, BL_B_Cu = 2
};

enum PadStackType : int { PST_NORMAL = 0, PST_FRONT_BACK = 1, PST_CUSTOM = 2 };
enum UnconnectedLayerRemoval : int { ULR_KEEP = 0, ULR_REMOVE = 1, ULR_REMOVE_EXCEPT_START_AND_END = 2 };
enum PadStackShape : int { PSS_CIRCLE = 0, PSS_RECT = 1, PSS_OVAL = 2, PSS_TRAPEZOID = 3, PSS_ROUNDRECT = 4, PSS_CHAMFEREDRECT = 5, PSS_CUSTOM = 6 };
enum ZoneConnectionStyle : int { ZCS_INHERITED = 0, ZCS_NONE = 1, ZCS_THERMAL = 2, ZCS_FULL = 3 };
enum SolderMaskMode : int { SMM_INHERITED = 0, SMM_CUSTOM = 1, SMM_MASKED = 2, SMM_UNMASKED = 3 };
enum SolderPasteMode : int { SPM_INHERITED = 0, SPM_PASTE = 1, SPM_NO_PASTE = 2 };
enum ViaCoveringMode : int { VCM_INHERITED = 0, VCM_COVERED = 1, VCM_UNCOVERED = 2 };
enum ViaPluggingMode : int { VPM_UNKNOWN = 0, VPM_PLUGGED = 1, VPM_UNPLUGGED = 2 };
enum DrillShape : int { DS_CIRCLE = 0, DS_OBLONG = 1 };
enum ViaDrillCappingMode : int { VDCM_UNKNOWN = 0, VDCM_CAPPED = 1, VDCM_UNCAPPED = 2 };
enum ViaDrillFillingMode : int { VDFM_UNKNOWN = 0, VDFM_FILLED = 1, VDFM_UNFILLED = 2 };
enum ViaDrillPostMachiningMode : int { VDPMM_UNKNOWN = 0, VDPM_NOT_POST_MACHINED = 1, VDPM_COUNTERBORE = 2, VDPM_COUNTERSINK = 3 };
enum ViaType : int { VT_UNKNOWN = 0, VT_THROUGH = 1, VT_BLIND_BURIED = 2, VT_MICRO = 3, VT_BLIND = 4, VT_BURIED = 5 };
enum PadType : int { PT_UNKNOWN = 0, PT_PTH = 1, PT_SMD = 2, PT_EDGE_CONNECTOR = 3, PT_NPTH = 4 };
enum CustomPadShapeZoneFillStrategy : int { CPSZ_UNKNOWN = 0, CPSZ_OUTLINE = 1, CPSZ_CONVEXHULL = 2 };
enum FootprintMountingStyle : int { FMS_UNKNOWN = 0, FMS_THROUGH_HOLE = 1, FMS_SMD = 2, FMS_UNSPECIFIED = 3 };
enum ZoneType : int { ZT_UNKNOWN = 0, ZT_COPPER = 1, ZT_GRAPHICAL = 2, ZT_RULE_AREA = 3, ZT_TEARDROP = 4 };
enum TeardropType : int { TDT_UNKNOWN = 0, TDT_NONE = 1 };
enum IslandRemovalMode : int { IRM_UNKNOWN = 0, IRM_ALWAYS = 1, IRM_NEVER = 2, IRM_AREA = 3 };
enum ZoneFillMode : int { ZFM_UNKNOWN = 0, ZFM_SOLID = 1, ZFM_HATCHED = 2 };
enum ZoneHatchSmoothing : int { ZHS_UNKNOWN = 0 };
enum ZoneHatchFillBorderMode : int { ZHFBM_UNKNOWN = 0, ZHFBM_USE_MIN_ZONE_THICKNESS = 1, ZHFBM_USE_HATCH_THICKNESS = 2 };
enum PlacementRuleSourceType : int { PRST_UNKNOWN = 0 };
enum ZoneBorderStyle : int { ZBS_UNKNOWN = 0, ZBS_SOLID = 1 };
enum DimensionTextBorderStyle : int { DTBS_UNKNOWN = 0, DTBS_NONE = 1, DTBS_RECTANGLE = 2, DTBS_CIRCLE = 3, DTBS_ROUNDRECT = 4 };
enum DimensionUnitFormat : int { DUF_UNKNOWN = 0 };
enum DimensionArrowDirection : int { DAD_UNKNOWN = 0, DAD_INWARD = 1, DAD_OUTWARD = 2 };
enum DimensionPrecision : int { DP_UNKNOWN = 0 };
enum DimensionTextPosition : int { DTP_UNKNOWN = 0, DTP_OUTSIDE = 1, DTP_INLINE = 2, DTP_MANUAL = 3 };
enum DimensionUnit : int { DU_UNKNOWN = 0, DU_INCHES = 1, DU_MILS = 2, DU_MILLIMETERS = 3, DU_AUTOMATIC = 4 };
enum CopperZoneSmoothingMode : int { CZSM_UNKNOWN = 0 };
enum BarcodeType : int { BCT_UNKNOWN = 0 };

// ── Forward declarations ───────────────────────────────────────────────
class FootprintInstance;
class Footprint;
class FootprintAttributes;
class Field;
class FieldId;
class BoardText;
class BoardTextBox;

class BoardGraphicShape;

// ── Helper: Chamfer positions ──────────────────────────────────────────
class ChamferedRectParams {
public:
    ChamferedRectParams() = default;
    double chamfer_ratio() const { return 0; }
    void set_chamfer_ratio(double) {}
    int corners_size() const { return 0; }
    void add_corners(int) {}
    bool top_left() const { return false; }
    void set_top_left(bool) {}
    bool top_right() const { return false; }
    void set_top_right(bool) {}
    bool bottom_left() const { return false; }
    void set_bottom_left(bool) {}
    bool bottom_right() const { return false; }
    void set_bottom_right(bool) {}
};

// ── PadStackLayer ──────────────────────────────────────────────────────
class PadStackLayer {
public:
    PadStackLayer() = default;
    void set_shape(PadStackShape s) { shape_ = s; }
    PadStackShape shape() const { return shape_; }
    void set_layer(BoardLayer l) { layer_ = l; }
    BoardLayer layer() const { return layer_; }
    Vector2* mutable_size() { return &size_; }
    const Vector2& size() const { return size_; }
    // corner radius ratio (double accessor for Deserialize, Ratio for Serialize)
    Ratio* mutable_corner_rounding_ratio() { return &ratio_; }
    const Ratio& corner_rounding_ratio() const { return ratio_; }
    bool has_corner_rounding_ratio() const { return false; }
    void set_corner_rounding_ratio(double) {}
    // chamfer ratio (direct double)
    double chamfer_ratio() const { return 0; }
    void set_chamfer_ratio(double) {}
    // chamfered rect params
    ChamferedRectParams* mutable_chamfered_rect_params() { return &chamfer_; }
    const ChamferedRectParams& chamfered_rect_params() const { return chamfer_; }
    bool has_chamfered_rect_params() const { return false; }
    // chamfered corners (direct access)
    ChamferedRectParams* mutable_chamfered_corners() { return &chamfer_; }
    const ChamferedRectParams& chamfered_corners() const { return chamfer_; }
    // custom anchor shape
    PadStackShape custom_anchor_shape() const { return PSS_CIRCLE; }
    void set_custom_anchor_shape(PadStackShape) {}
    // trapezoid delta
    bool has_trapezoid_delta() const { return false; }
    Vector2* mutable_trapezoid_delta() { return &trap_delta_; }
    const Vector2& trapezoid_delta() const { return trap_delta_; }
    // custom shapes (BoardGraphicShape list) - defined out-of-line after BoardGraphicShape
    BoardGraphicShape* add_custom_shapes();
    const std::vector<BoardGraphicShape>& custom_shapes() const;
    int custom_shapes_size() const { return 0; }
    bool has_custom_shapes() const { return false; }
    // thermal spokes
    Distance* mutable_thermal_spoke_width() { return &spoke_w_; }
    Angle* mutable_thermal_spoke_angle() { return &spoke_a_; }
    Distance* mutable_thermal_gap() { return &spoke_g_; }
    const Distance& thermal_spoke_width() const { return spoke_w_; }
    const Angle& thermal_spoke_angle() const { return spoke_a_; }
    const Distance& thermal_gap() const { return spoke_g_; }
    void set_zone_connection(ZoneConnectionStyle) {}
    ZoneConnectionStyle zone_connection() const { return ZCS_INHERITED; }
    google::protobuf::Any* add_graphic_shapes() { shapes_.emplace_back(); return &shapes_.back(); }
    int graphic_shapes_size() const { return (int)shapes_.size(); }
    const google::protobuf::Any& graphic_shapes(int i) const { return shapes_[i]; }
    void set_custom_zone_fill_strategy(CustomPadShapeZoneFillStrategy) {}
    CustomPadShapeZoneFillStrategy custom_zone_fill_strategy() const { return CPSZ_UNKNOWN; }
    Vector2* mutable_offset() { return &offset_; }
    const Vector2& offset() const { return offset_; }
    bool has_offset() const { return false; }
private:
    PadStackShape shape_ = PSS_CIRCLE;
    BoardLayer layer_ = BL_UNDEFINED;
    Vector2 size_, offset_, trap_delta_;
    Ratio ratio_;
    ChamferedRectParams chamfer_;
    Distance spoke_w_, spoke_g_;
    Angle spoke_a_;
    std::vector<google::protobuf::Any> shapes_;
};

class SolderMaskOverrides {
public:
    SolderMaskOverrides() = default;
    void set_solder_mask_mode(SolderMaskMode) {}
    SolderMaskMode solder_mask_mode() const { return SMM_INHERITED; }
    Distance* mutable_solder_mask_margin() { return &margin_; }
    const Distance& solder_mask_margin() const { return margin_; }
    bool has_solder_mask_margin() const { return false; }
private:
    Distance margin_;
};

class SolderPasteOverrides {
public:
    SolderPasteOverrides() = default;
    Distance* mutable_solder_paste_margin() { return &margin_; }
    const Distance& solder_paste_margin() const { return margin_; }
    bool has_solder_paste_margin() const { return false; }
    Ratio* mutable_solder_paste_margin_ratio() { return &ratio_; }
    const Ratio& solder_paste_margin_ratio() const { return ratio_; }
    bool has_solder_paste_margin_ratio() const { return false; }
private:
    Distance margin_;
    Ratio ratio_;
};

class PadStackOuterLayer {
public:
    PadStackOuterLayer() = default;
    SolderMaskOverrides* mutable_solder_mask() { return &sm_; }
    const SolderMaskOverrides& solder_mask() const { return sm_; }
    bool has_solder_mask() const { return false; }
    SolderPasteOverrides* mutable_solder_paste() { return &sp_; }
    const SolderPasteOverrides& solder_paste() const { return sp_; }
    bool has_solder_paste() const { return false; }
    // solder mask mode, via covering/plugging mode, solder paste mode
    void set_solder_mask_mode(SolderMaskMode) {}
    SolderMaskMode solder_mask_mode() const { return SMM_INHERITED; }
    void set_covering_mode(ViaCoveringMode) {}
    ViaCoveringMode covering_mode() const { return VCM_INHERITED; }
    void set_plugging_mode(ViaPluggingMode) {}
    ViaPluggingMode plugging_mode() const { return VPM_UNKNOWN; }
    void set_solder_paste_mode(SolderPasteMode) {}
    SolderPasteMode solder_paste_mode() const { return SPM_INHERITED; }
    // solder mask/paste settings sub-messages
    SolderMaskOverrides* mutable_solder_mask_settings() { return &sm_; }
    const SolderMaskOverrides& solder_mask_settings() const { return sm_; }
    bool has_solder_mask_settings() const { return false; }
    SolderPasteOverrides* mutable_solder_paste_settings() { return &sp_; }
    const SolderPasteOverrides& solder_paste_settings() const { return sp_; }
    bool has_solder_paste_settings() const { return false; }
private:
    SolderMaskOverrides sm_;
    SolderPasteOverrides sp_;
};

class ThermalSpokeSettings {
public:
    ThermalSpokeSettings() = default;
    Distance* mutable_width() { return &w_; }
    const Distance& width() const { return w_; }
    bool has_width() const { return false; }
    Angle* mutable_angle() { return &a_; }
    const Angle& angle() const { return a_; }
    bool has_angle() const { return false; }
    Distance* mutable_gap() { return &g_; }
    const Distance& gap() const { return g_; }
    bool has_gap() const { return false; }
private:
    Distance w_, g_; Angle a_;
};

class ZoneConnectionSettings {
public:
    ZoneConnectionSettings() = default;
    void set_zone_connection(ZoneConnectionStyle) {}
    ZoneConnectionStyle zone_connection() const { return ZCS_INHERITED; }
    Distance* mutable_thermal_gap() { return &gap_; }
    Distance* mutable_thermal_spoke_width() { return &width_; }
    Angle* mutable_thermal_spoke_angle() { return &angle_; }
    const Distance& thermal_gap() const { return gap_; }
    const Distance& thermal_spoke_width() const { return width_; }
    const Angle& thermal_spoke_angle() const { return angle_; }
    bool has_thermal_gap() const { return false; }
    bool has_thermal_spoke_width() const { return false; }
    // thermal spokes sub-message
    ThermalSpokeSettings* mutable_thermal_spokes() { return &spokes_; }
    const ThermalSpokeSettings& thermal_spokes() const { return spokes_; }
    bool has_thermal_spokes() const { return false; }
private:
    Distance gap_, width_;
    Angle angle_;
    ThermalSpokeSettings spokes_;
};

class PostMachiningProperties {
public:
    PostMachiningProperties() = default;
    void set_mode(ViaDrillPostMachiningMode) {}
    ViaDrillPostMachiningMode mode() const { return VDPMM_UNKNOWN; }
    void set_size(int) {}
    int size() const { return 0; }
    void set_depth(int) {}
    int depth() const { return 0; }
    void set_angle(int) {}
    int angle() const { return 0; }
};

class DrillProperties {
public:
    DrillProperties() = default;
    Vector2* mutable_diameter() { return &diam_; }
    const Vector2& diameter() const { return diam_; }
    void set_start_layer(BoardLayer) {}
    BoardLayer start_layer() const { return BL_F_Cu; }
    void set_end_layer(BoardLayer) {}
    BoardLayer end_layer() const { return BL_B_Cu; }
    void set_shape(DrillShape) {}
    DrillShape shape() const { return DS_CIRCLE; }
    void set_capped(ViaDrillCappingMode) {}
    ViaDrillCappingMode capped() const { return VDCM_UNKNOWN; }
    void set_filled(ViaDrillFillingMode) {}
    ViaDrillFillingMode filled() const { return VDFM_UNKNOWN; }
private:
    Vector2 diam_;
};

class PadStack : public google::protobuf::Message {
public:
    PadStack() = default;
    void set_type(PadStackType) {}
    PadStackType type() const { return PST_NORMAL; }
    PadStackLayer* add_copper_layers() { layers_.emplace_back(); return &layers_.back(); }
    int copper_layers_size() const { return (int)layers_.size(); }
    const PadStackLayer& copper_layers(int i) const { return layers_[i]; }
    const std::vector<PadStackLayer>& copper_layers() const { return layers_; }
    google::protobuf::RepeatedPtrField<PadStackLayer>* mutable_copper_layers() {
        return nullptr; // not used directly
    }
    DrillProperties* mutable_drill() { return &drill_; }
    const DrillProperties& drill() const { return drill_; }
    bool has_drill() const { return true; }
    DrillProperties* mutable_secondary_drill() { return &sdrill_; }
    const DrillProperties& secondary_drill() const { return sdrill_; }
    DrillProperties* mutable_tertiary_drill() { return &tdrill_; }
    const DrillProperties& tertiary_drill() const { return tdrill_; }
    bool has_secondary_drill() const { return false; }
    bool has_tertiary_drill() const { return false; }
    PadStackOuterLayer* mutable_front_outer_layers() { return &front_; }
    PadStackOuterLayer* mutable_back_outer_layers() { return &back_; }
    const PadStackOuterLayer& front_outer_layers() const { return front_; }
    const PadStackOuterLayer& back_outer_layers() const { return back_; }
    bool has_front_outer_layers() const { return false; }
    bool has_back_outer_layers() const { return false; }
    ZoneConnectionSettings* mutable_zone_settings() { return &zone_; }
    const ZoneConnectionSettings& zone_settings() const { return zone_; }
    bool has_zone_settings() const { return false; }
    void set_unconnected_layer_removal(UnconnectedLayerRemoval) {}
    UnconnectedLayerRemoval unconnected_layer_removal() const { return ULR_KEEP; }
    Angle* mutable_angle() { return &angle_; }
    const Angle& angle() const { return angle_; }
    bool has_angle() const { return false; }
    // layers() returning RepeatedField<int> for UnpackLayerSet
    void add_layers(BoardLayer l) { blayers_.Add((int)l); }
    int layers_size() const { return blayers_.size(); }
    BoardLayer layers(int i) const { return (BoardLayer)blayers_[i]; }
    const google::protobuf::RepeatedField<int>& layers() const { return blayers_; }
    google::protobuf::RepeatedField<int>* mutable_layers() { return &blayers_; }
    void clear_layers() { blayers_.Clear(); }
    PostMachiningProperties* mutable_front_post_machining() { return &fpm_; }
    const PostMachiningProperties& front_post_machining() const { return fpm_; }
    bool has_front_post_machining() const { return false; }
    PostMachiningProperties* mutable_back_post_machining() { return &bpm_; }
    const PostMachiningProperties& back_post_machining() const { return bpm_; }
    bool has_back_post_machining() const { return false; }
private:
    std::vector<PadStackLayer> layers_;
    google::protobuf::RepeatedField<int> blayers_;
    DrillProperties drill_, sdrill_, tdrill_;
    PadStackOuterLayer front_, back_;
    ZoneConnectionSettings zone_;
    Angle angle_;
    PostMachiningProperties fpm_, bpm_;
};

// ── Net ────────────────────────────────────────────────────────────────
class Net {
public:
    Net() = default;
    const std::string& name() const { return name_; }
    void set_name(const std::string& n) { name_ = n; }
    void set_name(const char* n) { name_ = n ? n : ""; }
    int code() const { return 0; }
    void set_code(int) {}
private:
    std::string name_;
};

class NetCode { public: NetCode() = default; };

// ── Track/Arc/Via ──────────────────────────────────────────────────────
class Track : public google::protobuf::Message {
public:
    Track() = default;
    kiapi::common::types::KIID* mutable_id() { return &id_; }
    const kiapi::common::types::KIID& id() const { return id_; }
    Vector2* mutable_start() { return &start_; }
    Vector2* mutable_end() { return &end_; }
    const Vector2& start() const { return start_; }
    const Vector2& end() const { return end_; }
    Distance* mutable_width() { return &width_; }
    const Distance& width() const { return width_; }
    void set_layer(BoardLayer) {}
    BoardLayer layer() const { return BL_UNDEFINED; }
    void set_locked(LockedState) {}
    LockedState locked() const { return LS_UNLOCKED; }
    Net* mutable_net() { return &net_; }
    const Net& net() const { return net_; }
    bool has_net() const { return false; }
private:
    kiapi::common::types::KIID id_; Vector2 start_, end_; Distance width_; Net net_;
};

class Arc : public google::protobuf::Message {
public:
    Arc() = default;
    kiapi::common::types::KIID* mutable_id() { return &id_; }
    const kiapi::common::types::KIID& id() const { return id_; }
    Vector2* mutable_start() { return &start_; }
    Vector2* mutable_mid() { return &mid_; }
    Vector2* mutable_end() { return &end_; }
    const Vector2& start() const { return start_; }
    const Vector2& mid() const { return mid_; }
    const Vector2& end() const { return end_; }
    Distance* mutable_width() { return &width_; }
    const Distance& width() const { return width_; }
    void set_layer(BoardLayer) {}
    BoardLayer layer() const { return BL_UNDEFINED; }
    void set_locked(LockedState) {}
    LockedState locked() const { return LS_UNLOCKED; }
    Net* mutable_net() { return &net_; }
    const Net& net() const { return net_; }
    bool has_net() const { return false; }
private:
    kiapi::common::types::KIID id_; Vector2 start_, mid_, end_; Distance width_; Net net_;
};

class Via : public google::protobuf::Message {
public:
    Via() = default;
    kiapi::common::types::KIID* mutable_id() { return &id_; }
    const kiapi::common::types::KIID& id() const { return id_; }
    Vector2* mutable_position() { return &pos_; }
    const Vector2& position() const { return pos_; }
    PadStack* mutable_pad_stack() { return &ps_; }
    const PadStack& pad_stack() const { return ps_; }
    bool has_pad_stack() const { return false; }
    void set_locked(LockedState) {}
    LockedState locked() const { return LS_UNLOCKED; }
    Net* mutable_net() { return &net_; }
    const Net& net() const { return net_; }
    bool has_net() const { return false; }
    void set_type(ViaType) {}
    ViaType type() const { return VT_UNKNOWN; }
private:
    kiapi::common::types::KIID id_; Vector2 pos_; PadStack ps_; Net net_;
};

// ── ThermalSpokeSettings ──────────────────────────────────────────────
// ── SymbolPinInfo ─────────────────────────────────────────────────────
class SymbolPinInfo {
public:
    SymbolPinInfo() = default;
    const std::string& name() const { static std::string s; return s; }
    void set_name(const std::string&) {}
    void set_type(ElectricalPinType) {}
    ElectricalPinType type() const { return EPT_INPUT; }
    void set_no_connect(bool) {}
    bool no_connect() const { return false; }
};

// ── Pad ────────────────────────────────────────────────────────────────
class Pad : public google::protobuf::Message {
public:
    Pad() = default;
    kiapi::common::types::KIID* mutable_id() { return &id_; }
    const kiapi::common::types::KIID& id() const { return id_; }
    void set_locked(LockedState) {}
    LockedState locked() const { return LS_UNLOCKED; }
    void set_number(const std::string& n) { number_ = n; }
    const std::string& number() const { return number_; }
    Net* mutable_net() { return &net_; }
    const Net& net() const { return net_; }
    bool has_net() const { return false; }
    void set_type(PadType) {}
    PadType type() const { return PT_UNKNOWN; }
    PadStack* mutable_pad_stack() { return &ps_; }
    const PadStack& pad_stack() const { return ps_; }
    bool has_pad_stack() const { return false; }
    Vector2* mutable_position() { return &pos_; }
    const Vector2& position() const { return pos_; }
    Distance* mutable_copper_clearance_override() { return &clearance_; }
    const Distance& copper_clearance_override() const { return clearance_; }
    bool has_copper_clearance_override() const { return false; }
    Distance* mutable_pad_to_die_length() { return &p2d_; }
    const Distance& pad_to_die_length() const { return p2d_; }
    bool has_pad_to_die_length() const { return false; }
    SymbolPinInfo* mutable_symbol_pin() { return &pin_; }
    const SymbolPinInfo& symbol_pin() const { return pin_; }
    bool has_symbol_pin() const { return false; }
    // pad_to_die_delay (Time type)
    Distance* mutable_pad_to_die_delay() { return &p2d_delay_; }
    const Distance& pad_to_die_delay() const { return p2d_delay_; }
    bool has_pad_to_die_delay() const { return false; }
private:
    kiapi::common::types::KIID id_; std::string number_; Net net_; PadStack ps_; Vector2 pos_;
    Distance clearance_, p2d_, p2d_delay_; SymbolPinInfo pin_;
};

// ── BoardText ──────────────────────────────────────────────────────────
class BoardText : public google::protobuf::Message {
public:
    BoardText() = default;
    kiapi::common::types::KIID* mutable_id() { return &id_; }
    const kiapi::common::types::KIID& id() const { return id_; }
    Text* mutable_text() { return &text_; }
    const Text& text() const { return text_; }
    bool has_text() const { return false; }
    void set_layer(BoardLayer) {}
    BoardLayer layer() const { return BL_UNDEFINED; }
    void set_knockout(bool) {}
    bool knockout() const { return false; }
    void set_locked(LockedState) {}
    LockedState locked() const { return LS_UNLOCKED; }
private:
    kiapi::common::types::KIID id_; Text text_;
};

class BoardTextBox : public google::protobuf::Message {
public:
    BoardTextBox() = default;
    kiapi::common::types::KIID* mutable_id() { return &id_; }
    const kiapi::common::types::KIID& id() const { return id_; }
    TextBox* mutable_textbox() { return &tb_; }
    const TextBox& textbox() const { return tb_; }
    bool has_textbox() const { return false; }
    void set_layer(BoardLayer) {}
    BoardLayer layer() const { return BL_UNDEFINED; }
    void set_locked(LockedState) {}
    LockedState locked() const { return LS_UNLOCKED; }
private:
    kiapi::common::types::KIID id_; TextBox tb_;
};

class BoardGraphicShape : public google::protobuf::Message {
public:
    BoardGraphicShape() = default;
    kiapi::common::types::KIID* mutable_id() { return &id_; }
    const kiapi::common::types::KIID& id() const { return id_; }
    GraphicShape* mutable_shape() { return &shape_; }
    const GraphicShape& shape() const { return shape_; }
    bool has_shape() const { return false; }
    void set_layer(BoardLayer) {}
    BoardLayer layer() const { return BL_UNDEFINED; }
    Net* mutable_net() { return &net_; }
    const Net& net() const { return net_; }
    bool has_net() const { return false; }
    void set_locked(LockedState) {}
    LockedState locked() const { return LS_UNLOCKED; }
private:
    kiapi::common::types::KIID id_; GraphicShape shape_; Net net_;
};

// Out-of-line PadStackLayer methods that need BoardGraphicShape to be complete
inline BoardGraphicShape* PadStackLayer::add_custom_shapes() {
    static std::vector<BoardGraphicShape> s_shapes;
    s_shapes.emplace_back();
    return &s_shapes.back();
}
inline const std::vector<BoardGraphicShape>& PadStackLayer::custom_shapes() const {
    static std::vector<BoardGraphicShape> s_empty;
    return s_empty;
}

class ReferenceImage : public google::protobuf::Message {
public:
    ReferenceImage() = default;
};

class Barcode : public google::protobuf::Message {
public:
    Barcode() = default;
    kiapi::common::types::KIID* mutable_id() { return &id_; }
    const kiapi::common::types::KIID& id() const { return id_; }
    Vector2* mutable_position() { return &pos_; }
    const Vector2& position() const { return pos_; }
    void set_layer(BoardLayer) {}
    BoardLayer layer() const { return BL_UNDEFINED; }
    void set_locked(LockedState) {}
    LockedState locked() const { return LS_UNLOCKED; }
    void set_type(BarcodeType) {}
    BarcodeType type() const { return BCT_UNKNOWN; }
    void set_content(const std::string&) {}
    const std::string& content() const { static std::string s; return s; }
    Distance* mutable_size() { return &size_; }
    const Distance& size() const { return size_; }
private:
    kiapi::common::types::KIID id_; Vector2 pos_; Distance size_;
};

// ── FieldId / Field ────────────────────────────────────────────────────
class FieldId {
public:
    FieldId() = default;
    int id() const { return id_; }
    void set_id(int v) { id_ = v; }
private:
    int id_ = 0;
};

class Field : public google::protobuf::Message {
public:
    Field() = default;
    FieldId* mutable_id() { return &id_; }
    const FieldId& id() const { return id_; }
    bool has_id() const { return false; }
    void set_name(const std::string& n) { name_ = n; }
    const std::string& name() const { return name_; }
    BoardText* mutable_text() { return &text_; }
    const BoardText& text() const { return text_; }
    bool has_text() const { return false; }
    void set_visible(bool) {}
    bool visible() const { return true; }
private:
    FieldId id_; std::string name_; BoardText text_;
};

// ── Zone types ─────────────────────────────────────────────────────────
class HatchFillSettings {
public:
    HatchFillSettings() = default;
    Distance* mutable_thickness() { return &thick_; }
    Distance* mutable_gap() { return &gap_; }
    Angle* mutable_orientation() { return &orient_; }
    void set_hatch_smoothing_ratio(double) {}
    double hatch_smoothing_ratio() const { return 0; }
    void set_hatch_hole_min_area_ratio(double) {}
    double hatch_hole_min_area_ratio() const { return 0; }
    void set_border_mode(ZoneHatchFillBorderMode) {}
    ZoneHatchFillBorderMode border_mode() const { return ZHFBM_UNKNOWN; }
    void set_smoothing(ZoneHatchSmoothing) {}
    ZoneHatchSmoothing smoothing() const { return ZHS_UNKNOWN; }
    const Distance& thickness() const { return thick_; }
    const Distance& gap() const { return gap_; }
    const Angle& orientation() const { return orient_; }
private:
    Distance thick_, gap_; Angle orient_;
};

class TeardropSettings {
public:
    TeardropSettings() = default;
    void set_type(TeardropType) {}
    TeardropType type() const { return TDT_UNKNOWN; }
};

class CopperZoneSettings : public google::protobuf::Message {
public:
    CopperZoneSettings() = default;
    ZoneConnectionSettings* mutable_connection() { return &conn_; }
    const ZoneConnectionSettings& connection() const { return conn_; }
    bool has_connection() const { return false; }
    Distance* mutable_clearance() { return &clear_; }
    const Distance& clearance() const { return clear_; }
    Distance* mutable_min_thickness() { return &minT_; }
    const Distance& min_thickness() const { return minT_; }
    void set_island_mode(IslandRemovalMode) {}
    IslandRemovalMode island_mode() const { return IRM_UNKNOWN; }
    void set_min_island_area(uint64_t) {}
    uint64_t min_island_area() const { return 0; }
    void set_fill_mode(ZoneFillMode) {}
    ZoneFillMode fill_mode() const { return ZFM_UNKNOWN; }
    HatchFillSettings* mutable_hatch_settings() { return &hatch_; }
    const HatchFillSettings& hatch_settings() const { return hatch_; }
    bool has_hatch_settings() const { return false; }
    Net* mutable_net() { return &net_; }
    const Net& net() const { return net_; }
    bool has_net() const { return false; }
    TeardropSettings* mutable_teardrop() { return &td_; }
    const TeardropSettings& teardrop() const { return td_; }
    bool has_teardrop() const { return false; }
private:
    ZoneConnectionSettings conn_; Distance clear_, minT_; HatchFillSettings hatch_;
    Net net_; TeardropSettings td_;
};

class RuleAreaSettings : public google::protobuf::Message {
public:
    RuleAreaSettings() = default;
    void set_keepout_copper(bool) {}
    bool keepout_copper() const { return false; }
    void set_keepout_vias(bool) {}
    bool keepout_vias() const { return false; }
    void set_keepout_tracks(bool) {}
    bool keepout_tracks() const { return false; }
    void set_keepout_pads(bool) {}
    bool keepout_pads() const { return false; }
    void set_keepout_footprints(bool) {}
    bool keepout_footprints() const { return false; }
    void set_placement_enabled(bool) {}
    bool placement_enabled() const { return false; }
    void set_placement_source_type(PlacementRuleSourceType) {}
    PlacementRuleSourceType placement_source_type() const { return PRST_UNKNOWN; }
    void set_placement_source(const std::string&) {}
    const std::string& placement_source() const { static std::string s; return s; }
};

class ZoneBorderSettings {
public:
    ZoneBorderSettings() = default;
    void set_style(ZoneBorderStyle) {}
    ZoneBorderStyle style() const { return ZBS_UNKNOWN; }
    Distance* mutable_pitch() { return &pitch_; }
    const Distance& pitch() const { return pitch_; }
private:
    Distance pitch_;
};

class ZoneFilledPolygons {
public:
    ZoneFilledPolygons() = default;
    void set_layer(BoardLayer) {}
    BoardLayer layer() const { return BL_UNDEFINED; }
    PolySet* mutable_shapes() { return &shapes_; }
    const PolySet& shapes() const { return shapes_; }
private:
    PolySet shapes_;
};

class ZoneLayerProperties {
public:
    ZoneLayerProperties() = default;
    void set_layer(BoardLayer) {}
    BoardLayer layer() const { return BL_UNDEFINED; }
    Vector2* mutable_hatching_offset() { return &offset_; }
    const Vector2& hatching_offset() const { return offset_; }
    bool has_hatching_offset() const { return false; }
private:
    Vector2 offset_;
};

class Zone : public google::protobuf::Message {
public:
    Zone() = default;
    kiapi::common::types::KIID* mutable_id() { return &id_; }
    const kiapi::common::types::KIID& id() const { return id_; }
    void set_type(ZoneType) {}
    ZoneType type() const { return ZT_UNKNOWN; }
    void add_layers(BoardLayer l) { zlayers_.Add((int)l); }
    int layers_size() const { return zlayers_.size(); }
    BoardLayer layers(int i) const { return (BoardLayer)zlayers_[i]; }
    const google::protobuf::RepeatedField<int>& layers() const { return zlayers_; }
    google::protobuf::RepeatedField<int>* mutable_layers() { return &zlayers_; }
    void clear_layers() { zlayers_.Clear(); }
    PolySet* mutable_outline() { return &outline_; }
    const PolySet& outline() const { return outline_; }
    bool has_outline() const { return false; }
    void set_name(const std::string& n) { name_ = n; }
    const std::string& name() const { return name_; }
    // copper settings
    CopperZoneSettings* mutable_copper_settings() { return &copper_; }
    const CopperZoneSettings& copper_settings() const { return copper_; }
    bool has_copper_settings() const { return false; }
    // rule area settings
    RuleAreaSettings* mutable_rule_area_settings() { return &rule_; }
    const RuleAreaSettings& rule_area_settings() const { return rule_; }
    bool has_rule_area_settings() const { return false; }
    void set_priority(uint32_t) {}
    uint32_t priority() const { return 0; }
    void set_filled(bool) {}
    bool filled() const { return false; }
    ZoneFilledPolygons* add_filled_polygons() { fpoly_.emplace_back(); return &fpoly_.back(); }
    int filled_polygons_size() const { return (int)fpoly_.size(); }
    const ZoneFilledPolygons& filled_polygons(int i) const { return fpoly_[i]; }
    const std::vector<ZoneFilledPolygons>& filled_polygons() const { return fpoly_; }
    ZoneBorderSettings* mutable_border() { return &border_; }
    const ZoneBorderSettings& border() const { return border_; }
    bool has_border() const { return false; }
    void set_locked(LockedState) {}
    LockedState locked() const { return LS_UNLOCKED; }
    ZoneLayerProperties* add_layer_properties() { lprops_.emplace_back(); return &lprops_.back(); }
    int layer_properties_size() const { return (int)lprops_.size(); }
    const ZoneLayerProperties& layer_properties(int i) const { return lprops_[i]; }
    const std::vector<ZoneLayerProperties>& layer_properties() const { return lprops_; }
private:
    kiapi::common::types::KIID id_; std::string name_; PolySet outline_; CopperZoneSettings copper_;
    RuleAreaSettings rule_; ZoneBorderSettings border_;
    std::vector<ZoneFilledPolygons> fpoly_; std::vector<ZoneLayerProperties> lprops_;
    google::protobuf::RepeatedField<int> zlayers_;
};

// ── Dimension types ────────────────────────────────────────────────────
class AlignedDimensionAttributes : public google::protobuf::Message {
public:
    AlignedDimensionAttributes() = default;
    Vector2* mutable_start() { return &s_; }
    Vector2* mutable_end() { return &e_; }
    const Vector2& start() const { return s_; }
    const Vector2& end() const { return e_; }
    Distance* mutable_height() { return &h_; }
    const Distance& height() const { return h_; }
    Distance* mutable_extension_height() { return &eh_; }
    const Distance& extension_height() const { return eh_; }
private:
    Vector2 s_, e_; Distance h_, eh_;
};

class OrthogonalDimensionAttributes : public google::protobuf::Message {
public:
    OrthogonalDimensionAttributes() = default;
    Vector2* mutable_start() { return &s_; }
    Vector2* mutable_end() { return &e_; }
    const Vector2& start() const { return s_; }
    const Vector2& end() const { return e_; }
    Distance* mutable_height() { return &h_; }
    const Distance& height() const { return h_; }
    Distance* mutable_extension_height() { return &eh_; }
    const Distance& extension_height() const { return eh_; }
    void set_alignment(AxisAlignment) {}
    AxisAlignment alignment() const { return AA_HORIZONTAL; }
private:
    Vector2 s_, e_; Distance h_, eh_;
};

class RadialDimensionAttributes : public google::protobuf::Message {
public:
    RadialDimensionAttributes() = default;
    Vector2* mutable_center() { return &c_; }
    Vector2* mutable_radius_point() { return &r_; }
    const Vector2& center() const { return c_; }
    const Vector2& radius_point() const { return r_; }
    Distance* mutable_leader_length() { return &ll_; }
    const Distance& leader_length() const { return ll_; }
private:
    Vector2 c_, r_; Distance ll_;
};

class LeaderDimensionAttributes : public google::protobuf::Message {
public:
    LeaderDimensionAttributes() = default;
    Vector2* mutable_start() { return &s_; }
    Vector2* mutable_end() { return &e_; }
    const Vector2& start() const { return s_; }
    const Vector2& end() const { return e_; }
    void set_border_style(DimensionTextBorderStyle) {}
    DimensionTextBorderStyle border_style() const { return DTBS_UNKNOWN; }
private:
    Vector2 s_, e_;
};

class CenterDimensionAttributes : public google::protobuf::Message {
public:
    CenterDimensionAttributes() = default;
    Vector2* mutable_center() { return &c_; }
    Vector2* mutable_end() { return &e_; }
    const Vector2& center() const { return c_; }
    const Vector2& end() const { return e_; }
private:
    Vector2 c_, e_;
};

class Dimension : public google::protobuf::Message {
public:
    Dimension() = default;
    kiapi::common::types::KIID* mutable_id() { return &id_; }
    const kiapi::common::types::KIID& id() const { return id_; }
    void set_locked(LockedState) {}
    LockedState locked() const { return LS_UNLOCKED; }
    void set_layer(BoardLayer) {}
    BoardLayer layer() const { return BL_UNDEFINED; }
    Text* mutable_text() { return &text_; }
    const Text& text() const { return text_; }
    bool has_text() const { return false; }
    // dimension style (oneof in proto)
    AlignedDimensionAttributes* mutable_aligned() { return &aligned_; }
    const AlignedDimensionAttributes& aligned() const { return aligned_; }
    bool has_aligned() const { return false; }
    OrthogonalDimensionAttributes* mutable_orthogonal() { return &orth_; }
    const OrthogonalDimensionAttributes& orthogonal() const { return orth_; }
    bool has_orthogonal() const { return false; }
    RadialDimensionAttributes* mutable_radial() { return &radial_; }
    const RadialDimensionAttributes& radial() const { return radial_; }
    bool has_radial() const { return false; }
    LeaderDimensionAttributes* mutable_leader() { return &leader_; }
    const LeaderDimensionAttributes& leader() const { return leader_; }
    bool has_leader() const { return false; }
    CenterDimensionAttributes* mutable_center() { return &center_; }
    const CenterDimensionAttributes& center() const { return center_; }
    bool has_center() const { return false; }
    // dimension properties
    void set_override_text_enabled(bool) {}
    bool override_text_enabled() const { return false; }
    void set_override_text(const std::string&) {}
    const std::string& override_text() const { static std::string s; return s; }
    void set_prefix(const std::string&) {}
    const std::string& prefix() const { static std::string s; return s; }
    void set_suffix(const std::string&) {}
    const std::string& suffix() const { static std::string s; return s; }
    void set_unit(DimensionUnit) {}
    DimensionUnit unit() const { return DU_UNKNOWN; }
    void set_unit_format(DimensionUnitFormat) {}
    DimensionUnitFormat unit_format() const { return DUF_UNKNOWN; }
    void set_arrow_direction(DimensionArrowDirection) {}
    DimensionArrowDirection arrow_direction() const { return DAD_UNKNOWN; }
    void set_precision(DimensionPrecision) {}
    DimensionPrecision precision() const { return DP_UNKNOWN; }
    void set_suppress_trailing_zeroes(bool) {}
    bool suppress_trailing_zeroes() const { return false; }
    Distance* mutable_line_thickness() { return &lt_; }
    const Distance& line_thickness() const { return lt_; }
    Distance* mutable_arrow_length() { return &al_; }
    const Distance& arrow_length() const { return al_; }
    Distance* mutable_extension_offset() { return &eo_; }
    const Distance& extension_offset() const { return eo_; }
    void set_text_position(DimensionTextPosition) {}
    DimensionTextPosition text_position() const { return DTP_UNKNOWN; }
    void set_keep_text_aligned(bool) {}
    bool keep_text_aligned() const { return false; }
private:
    kiapi::common::types::KIID id_; Text text_;
    AlignedDimensionAttributes aligned_; OrthogonalDimensionAttributes orth_;
    RadialDimensionAttributes radial_; LeaderDimensionAttributes leader_;
    CenterDimensionAttributes center_;
    Distance lt_, al_, eo_;
};

// ── Group ──────────────────────────────────────────────────────────────
class Group : public google::protobuf::Message {
public:
    Group() = default;
    kiapi::common::types::KIID* mutable_id() { return &id_; }
    const kiapi::common::types::KIID& id() const { return id_; }
    void set_name(const std::string& n) { name_ = n; }
    const std::string& name() const { return name_; }
    kiapi::common::types::KIID* add_items() { items_.emplace_back(); return &items_.back(); }
    int items_size() const { return (int)items_.size(); }
    const kiapi::common::types::KIID& items(int i) const { return items_[i]; }
    const std::vector<kiapi::common::types::KIID>& items() const { return items_; }
private:
    kiapi::common::types::KIID id_; std::string name_; std::vector<kiapi::common::types::KIID> items_;
};

// ── FootprintAttributes ────────────────────────────────────────────────
class FootprintAttributes : public google::protobuf::Message {
public:
    FootprintAttributes() = default;
    void set_description(const std::string&) {}
    const std::string& description() const { static std::string s; return s; }
    void set_keywords(const std::string&) {}
    const std::string& keywords() const { static std::string s; return s; }
    void set_not_in_schematic(bool) {}
    bool not_in_schematic() const { return false; }
    void set_exclude_from_position_files(bool) {}
    bool exclude_from_position_files() const { return false; }
    void set_exclude_from_bill_of_materials(bool) {}
    bool exclude_from_bill_of_materials() const { return false; }
    void set_exempt_from_courtyard_requirement(bool) {}
    bool exempt_from_courtyard_requirement() const { return false; }
    void set_do_not_populate(bool) {}
    bool do_not_populate() const { return false; }
    void set_mounting_style(FootprintMountingStyle) {}
    FootprintMountingStyle mounting_style() const { return FMS_UNKNOWN; }
    void set_allow_soldermask_bridges(bool) {}
    bool allow_soldermask_bridges() const { return false; }
};

// ── FootprintDesignRuleOverrides ───────────────────────────────────────
class FootprintDesignRuleOverrides : public google::protobuf::Message {
public:
    FootprintDesignRuleOverrides() = default;
    SolderMaskOverrides* mutable_solder_mask() { return &sm_; }
    const SolderMaskOverrides& solder_mask() const { return sm_; }
    bool has_solder_mask() const { return false; }
    SolderPasteOverrides* mutable_solder_paste() { return &sp_; }
    const SolderPasteOverrides& solder_paste() const { return sp_; }
    bool has_solder_paste() const { return false; }
    Distance* mutable_copper_clearance() { return &cc_; }
    const Distance& copper_clearance() const { return cc_; }
    bool has_copper_clearance() const { return false; }
    void set_zone_connection(ZoneConnectionStyle) {}
    ZoneConnectionStyle zone_connection() const { return ZCS_INHERITED; }
private:
    SolderMaskOverrides sm_; SolderPasteOverrides sp_; Distance cc_;
};

// ── NetTieDefinition ───────────────────────────────────────────────────
class NetTieDefinition {
public:
    NetTieDefinition() = default;
    void add_pad_number(const std::string& s) { pads_.push_back(s); }
    int pad_number_size() const { return (int)pads_.size(); }
    const std::string& pad_number(int i) const { return pads_[i]; }
    // Range-based for: for (const std::string& pad : msg.pad_number())
    const std::vector<std::string>& pad_number() const { return pads_; }
private:
    std::vector<std::string> pads_;
};

// ── Footprint3DModel ──────────────────────────────────────────────────
class Footprint3DModel : public google::protobuf::Message {
public:
    Footprint3DModel() = default;
    void set_filename(const std::string&) {}
    const std::string& filename() const { static std::string s; return s; }
    Vector3D* mutable_scale() { return &scale_; }
    const Vector3D& scale() const { return scale_; }
    Vector3D* mutable_rotation() { return &rot_; }
    const Vector3D& rotation() const { return rot_; }
    Vector3D* mutable_offset() { return &off_; }
    const Vector3D& offset() const { return off_; }
    void set_visible(bool) {}
    bool visible() const { return true; }
    void set_opacity(double) {}
    double opacity() const { return 1.0; }
private:
    Vector3D scale_, rot_, off_;
};

// ── JumperGroup / JumperSettings ──────────────────────────────────────
class JumperGroup {
public:
    JumperGroup() = default;
    void add_pad_names(const std::string&) {}
    int pad_names_size() const { return 0; }
    const std::string& pad_names(int) const { static std::string s; return s; }
};

class JumperSettings {
public:
    JumperSettings() = default;
    void set_duplicate_names_are_jumpered(bool) {}
    bool duplicate_names_are_jumpered() const { return false; }
    JumperGroup* add_groups() { groups_.emplace_back(); return &groups_.back(); }
    int groups_size() const { return (int)groups_.size(); }
    const JumperGroup& groups(int i) const { return groups_[i]; }
private:
    std::vector<JumperGroup> groups_;
};

// ── Footprint (definition) ────────────────────────────────────────────
class Footprint : public google::protobuf::Message {
public:
    Footprint() = default;
    LibraryIdentifier* mutable_id() { return &id_; }
    const LibraryIdentifier& id() const { return id_; }
    bool has_id() const { return false; }
    Vector2* mutable_anchor() { return &anchor_; }
    const Vector2& anchor() const { return anchor_; }
    FootprintAttributes* mutable_attributes() { return &attrs_; }
    const FootprintAttributes& attributes() const { return attrs_; }
    bool has_attributes() const { return false; }
    FootprintDesignRuleOverrides* mutable_overrides() { return &over_; }
    const FootprintDesignRuleOverrides& overrides() const { return over_; }
    bool has_overrides() const { return false; }
    NetTieDefinition* add_net_ties() { ties_.emplace_back(); return &ties_.back(); }
    int net_ties_size() const { return (int)ties_.size(); }
    const NetTieDefinition& net_ties(int i) const { return ties_[i]; }
    // Range-based for support
    const std::vector<NetTieDefinition>& net_ties() const { return ties_; }
    void add_private_layers(BoardLayer l) { players_.push_back((int)l); }
    int private_layers_size() const { return (int)players_.size(); }
    BoardLayer private_layers(int i) const { return (BoardLayer)players_[i]; }
    // Range-based for support (returns ints for cast to BoardLayer)
    const std::vector<int>& private_layers() const { return players_; }
    Field* mutable_reference_field() { return &ref_; }
    const Field& reference_field() const { return ref_; }
    bool has_reference_field() const { return false; }
    Field* mutable_value_field() { return &val_; }
    const Field& value_field() const { return val_; }
    bool has_value_field() const { return false; }
    Field* mutable_datasheet_field() { return &ds_; }
    const Field& datasheet_field() const { return ds_; }
    bool has_datasheet_field() const { return false; }
    Field* mutable_description_field() { return &desc_; }
    const Field& description_field() const { return desc_; }
    bool has_description_field() const { return false; }
    google::protobuf::Any* add_items() { items_.emplace_back(); return &items_.back(); }
    int items_size() const { return (int)items_.size(); }
    const google::protobuf::Any& items(int i) const { return items_[i]; }
    // Range-based for support
    const std::vector<google::protobuf::Any>& items() const { return items_; }
    JumperSettings* mutable_jumpers() { return &jumpers_; }
    const JumperSettings& jumpers() const { return jumpers_; }
    bool has_jumpers() const { return false; }
private:
    LibraryIdentifier id_; Vector2 anchor_;
    FootprintAttributes attrs_; FootprintDesignRuleOverrides over_;
    std::vector<NetTieDefinition> ties_;
    std::vector<int> players_;
    Field ref_, val_, ds_, desc_;
    std::vector<google::protobuf::Any> items_;
    JumperSettings jumpers_;
};

// ── FootprintInstance ──────────────────────────────────────────────────
class FootprintInstance : public google::protobuf::Message {
public:
    FootprintInstance() = default;
    kiapi::common::types::KIID* mutable_id() { return &id_; }
    const kiapi::common::types::KIID& id() const { return id_; }
    Vector2* mutable_position() { return &pos_; }
    const Vector2& position() const { return pos_; }
    Angle* mutable_orientation() { return &orient_; }
    const Angle& orientation() const { return orient_; }
    void set_layer(BoardLayer) {}
    BoardLayer layer() const { return BL_UNDEFINED; }
    void set_locked(LockedState) {}
    LockedState locked() const { return LS_UNLOCKED; }
    types::Footprint* mutable_definition() { return &def_; }
    const types::Footprint& definition() const { return def_; }
    bool has_definition() const { return false; }
    Field* mutable_reference_field() { return &ref_; }
    const Field& reference_field() const { return ref_; }
    bool has_reference_field() const { return false; }
    Field* mutable_value_field() { return &val_; }
    const Field& value_field() const { return val_; }
    bool has_value_field() const { return false; }
    Field* mutable_datasheet_field() { return &ds_; }
    const Field& datasheet_field() const { return ds_; }
    bool has_datasheet_field() const { return false; }
    Field* mutable_description_field() { return &desc_; }
    const Field& description_field() const { return desc_; }
    bool has_description_field() const { return false; }
    FootprintAttributes* mutable_attributes() { return &attrs_; }
    const FootprintAttributes& attributes() const { return attrs_; }
    bool has_attributes() const { return false; }
    FootprintDesignRuleOverrides* mutable_overrides() { return &over_; }
    const FootprintDesignRuleOverrides& overrides() const { return over_; }
    bool has_overrides() const { return false; }
    SheetPath* mutable_symbol_path() { return &sp_; }
    const SheetPath& symbol_path() const { return sp_; }
    bool has_symbol_path() const { return false; }
    void set_symbol_sheet_name(const std::string&) {}
    const std::string& symbol_sheet_name() const { static std::string s; return s; }
    void set_symbol_sheet_filename(const std::string&) {}
    const std::string& symbol_sheet_filename() const { static std::string s; return s; }
    void set_symbol_footprint_filters(const std::string&) {}
    const std::string& symbol_footprint_filters() const { static std::string s; return s; }
private:
    kiapi::common::types::KIID id_; Vector2 pos_; Angle orient_;
    types::Footprint def_;
    Field ref_, val_, ds_, desc_;
    FootprintAttributes attrs_; FootprintDesignRuleOverrides over_;
    SheetPath sp_;
};

// ── Board Stackup ──────────────────────────────────────────────────────
class BoardStackupLayer {
public:
    BoardStackupLayer() = default;
    void set_layer(BoardLayer) {}
    BoardLayer layer() const { return BL_UNDEFINED; }
    void set_type(int) {}
    void set_thickness(int64_t) {}
    void set_material(const std::string&) {}
    void set_color(const std::string&) {}
    void set_epsilon_r(double) {}
    void set_loss_tangent(double) {}
    void set_enabled(bool) {}
};

class BoardStackup : public google::protobuf::Message {
public:
    BoardStackup() = default;
    BoardStackupLayer* add_layers() { layers_.emplace_back(); return &layers_.back(); }
    int layers_size() const { return (int)layers_.size(); }
    const BoardStackupLayer& layers(int i) const { return layers_[i]; }
    void set_finish(const std::string&) {}
    const std::string& finish() const { static std::string s; return s; }
    void set_board_thickness(int64_t) {}
    int64_t board_thickness() const { return 0; }
    void set_dielectric_constraint(bool) {}
    bool dielectric_constraint() const { return false; }
    void set_castellated_pads(bool) {}
    bool castellated_pads() const { return false; }
    void set_edge_plating(bool) {}
    bool edge_plating() const { return false; }
private:
    std::vector<BoardStackupLayer> layers_;
};

}  // namespace types
}  // namespace board
}  // namespace kiapi
