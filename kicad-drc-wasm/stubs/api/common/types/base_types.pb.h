#pragma once
// Stub base_types.pb.h for WASM build
#include <string>
#include <vector>
#include <google/protobuf/message.h>
#include <google/protobuf/any.pb.h>
#include <api/common/types/enums.pb.h>

#ifndef KIAPI_IMPORTEXPORT
#define KIAPI_IMPORTEXPORT
#endif

namespace kiapi {
namespace common {
namespace types {

class Distance {
public:
    Distance() = default;
    int64_t value_nm() const { return value_nm_; }
    void set_value_nm(int64_t v) { value_nm_ = v; }
    // Delay values (used by pad_to_die_delay)
    double value_as() const { return 0; }
    void set_value_as(double) {}
private:
    int64_t value_nm_ = 0;
};

class Angle {
public:
    Angle() = default;
    double value_degrees() const { return 0; }
    void set_value_degrees(double) {}
};

class Ratio {
public:
    Ratio() = default;
    double value() const { return 0; }
    void set_value(double) {}
    operator double() const { return 0; }
};

class Vector2 {
public:
    Vector2() = default;
    int64_t x_nm() const { return x_; }
    int64_t y_nm() const { return y_; }
    void set_x_nm(int64_t v) { x_ = v; }
    void set_y_nm(int64_t v) { y_ = v; }
private:
    int64_t x_ = 0, y_ = 0;
};

class Vector3 {
public:
    Vector3() = default;
    int64_t x_nm() const { return 0; }
    int64_t y_nm() const { return 0; }
    int64_t z_nm() const { return 0; }
    void set_x_nm(int64_t) {}
    void set_y_nm(int64_t) {}
    void set_z_nm(int64_t) {}
};

class Vector3D {
public:
    Vector3D() = default;
    double x() const { return 0; }
    double y() const { return 0; }
    double z() const { return 0; }
    void set_x(double) {}
    void set_y(double) {}
    void set_z(double) {}
};

class Box2 {
public:
    Box2() = default;
    Vector2* mutable_position() { return &pos_; }
    Vector2* mutable_size() { return &size_; }
    const Vector2& position() const { return pos_; }
    const Vector2& size() const { return size_; }
private:
    Vector2 pos_, size_;
};

class Color {
public:
    Color() = default;
    double r() const { return 0; }
    double g() const { return 0; }
    double b() const { return 0; }
    double a() const { return 1; }
    void set_r(double) {}
    void set_g(double) {}
    void set_b(double) {}
    void set_a(double) {}
};

class KIID {
public:
    KIID() = default;
    const std::string& value() const { static std::string s; return s; }
    void set_value(const std::string&) {}
    int id() const { return 0; }
    void set_id(int) {}
};

class LibraryIdentifier {
public:
    LibraryIdentifier() = default;
    const std::string& library_name() const { static std::string s; return s; }
    const std::string& entry_name() const { static std::string s; return s; }
    void set_library_name(const std::string&) {}
    void set_entry_name(const std::string&) {}
    void CopyFrom(const LibraryIdentifier&) {}
};

class PolyLineNode {
public:
    PolyLineNode() = default;
    Vector2* mutable_point() { return &pt_; }
    const Vector2& point() const { return pt_; }
    bool has_arc_mid() const { return false; }
    Vector2* mutable_arc_mid() { return &pt_; }
    const Vector2& arc_mid() const { return pt_; }
private:
    Vector2 pt_;
};

class PolyLine {
public:
    PolyLine() = default;
    PolyLineNode* add_nodes() { nodes_.emplace_back(); return &nodes_.back(); }
    int nodes_size() const { return (int)nodes_.size(); }
    const PolyLineNode& nodes(int i) const { return nodes_[i]; }
    bool closed() const { return false; }
    void set_closed(bool) {}
private:
    std::vector<PolyLineNode> nodes_;
};

class PolygonWithHoles {
public:
    PolygonWithHoles() = default;
    PolyLine* mutable_outline() { return &outline_; }
    const PolyLine& outline() const { return outline_; }
    PolyLine* add_holes() { holes_.emplace_back(); return &holes_.back(); }
    int holes_size() const { return (int)holes_.size(); }
    const PolyLine& holes(int i) const { return holes_[i]; }
private:
    PolyLine outline_;
    std::vector<PolyLine> holes_;
};

class PolySet {
public:
    PolySet() = default;
    PolygonWithHoles* add_polygons() { polys_.emplace_back(); return &polys_.back(); }
    int polygons_size() const { return (int)polys_.size(); }
    const PolygonWithHoles& polygons(int i) const { return polys_[i]; }
private:
    std::vector<PolygonWithHoles> polys_;
};

class SheetPath {
public:
    SheetPath() = default;
    void add_path_segments(const std::string&) {}
    int path_segments_size() const { return 0; }
    const std::string& path_segments(int) const { static std::string s; return s; }
};

class StrokeAttributes {
public:
    StrokeAttributes() = default;
    Distance* mutable_width() { return &width_; }
    const Distance& width() const { return width_; }
    void set_style(StrokeLineStyle) {}
    StrokeLineStyle style() const { return SLS_DEFAULT; }
    Color* mutable_color() { return &color_; }
    const Color& color() const { return color_; }
private:
    Distance width_;
    Color color_;
};

class TextAttributes : public google::protobuf::Message {
public:
    TextAttributes() = default;
    // Font
    void set_font_name(const std::string&) {}
    const std::string& font_name() const { static std::string s; return s; }
    // Size
    Vector2* mutable_size() { return &size_; }
    const Vector2& size() const { return size_; }
    // Stroke width
    Distance* mutable_stroke_width() { return &stroke_; }
    const Distance& stroke_width() const { return stroke_; }
    // Angle
    Angle* mutable_angle() { return &angle_; }
    const Angle& angle() const { return angle_; }
    // Line spacing
    void set_line_spacing(double) {}
    double line_spacing() const { return 1.0; }
    // Bold/italic/etc
    void set_bold(bool) {}
    bool bold() const { return false; }
    void set_italic(bool) {}
    bool italic() const { return false; }
    void set_underlined(bool) {}
    bool underlined() const { return false; }
    void set_mirrored(bool) {}
    bool mirrored() const { return false; }
    void set_multiline(bool) {}
    bool multiline() const { return false; }
    void set_keep_upright(bool) {}
    bool keep_upright() const { return false; }
    void set_visible(bool) {}
    bool visible() const { return true; }
    // Alignment
    void set_horizontal_alignment(HorizontalAlignment) {}
    HorizontalAlignment horizontal_alignment() const { return HA_LEFT; }
    void set_vertical_alignment(VerticalAlignment) {}
    VerticalAlignment vertical_alignment() const { return VA_TOP; }
    // Color
    Color* mutable_color() { return &color_; }
    const Color& color() const { return color_; }
    bool has_color() const { return false; }
private:
    Vector2 size_;
    Distance stroke_;
    Angle angle_;
    Color color_;
};

class Text : public google::protobuf::Message {
public:
    Text() = default;
    // Position
    Vector2* mutable_position() { return &pos_; }
    const Vector2& position() const { return pos_; }
    // Text content
    void set_text(const std::string&) {}
    const std::string& text() const { static std::string s; return s; }
    // Attributes
    TextAttributes* mutable_attributes() { return &attrs_; }
    const TextAttributes& attributes() const { return attrs_; }
    bool has_attributes() const { return false; }
    // Layer (used by BoardText serialization)
    void set_layer(int) {}
    int layer() const { return 0; }
    // Hyperlink
    void set_hyperlink(const std::string&) {}
    const std::string& hyperlink() const { static std::string s; return s; }
private:
    Vector2 pos_;
    TextAttributes attrs_;
};

class TextBox : public google::protobuf::Message {
public:
    TextBox() = default;
    // Corners
    Vector2* mutable_top_left() { return &tl_; }
    Vector2* mutable_bottom_right() { return &br_; }
    const Vector2& top_left() const { return tl_; }
    const Vector2& bottom_right() const { return br_; }
    // Text
    void set_text(const std::string&) {}
    const std::string& text() const { static std::string s; return s; }
    // Attributes
    TextAttributes* mutable_attributes() { return &attrs_; }
    const TextAttributes& attributes() const { return attrs_; }
    bool has_attributes() const { return false; }
    // Stroke
    StrokeAttributes* mutable_border_stroke() { return &stroke_; }
    const StrokeAttributes& border_stroke() const { return stroke_; }
    bool has_border_stroke() const { return false; }
    // Margins
    Distance* mutable_margin_left() { return &margin_; }
    Distance* mutable_margin_top() { return &margin_; }
    Distance* mutable_margin_right() { return &margin_; }
    Distance* mutable_margin_bottom() { return &margin_; }
    const Distance& margin_left() const { return margin_; }
    const Distance& margin_top() const { return margin_; }
    const Distance& margin_right() const { return margin_; }
    const Distance& margin_bottom() const { return margin_; }
private:
    Vector2 tl_, br_;
    TextAttributes attrs_;
    StrokeAttributes stroke_;
    Distance margin_;
};

class GraphicAttributes {
public:
    GraphicAttributes() = default;
    StrokeAttributes* mutable_stroke() { return &stroke_; }
    const StrokeAttributes& stroke() const { return stroke_; }
    void set_fill(GraphicFillType) {}
    GraphicFillType fill() const { return GFT_NONE; }
private:
    StrokeAttributes stroke_;
};

class GraphicShape : public google::protobuf::Message {
public:
    GraphicShape() = default;
    GraphicAttributes* mutable_attributes() { return &attrs_; }
    const GraphicAttributes& attributes() const { return attrs_; }
    bool has_attributes() const { return false; }
    // Segment
    Vector2* mutable_start() { return &start_; }
    Vector2* mutable_end() { return &end_; }
    const Vector2& start() const { return start_; }
    const Vector2& end() const { return end_; }
    // Other shape types
    Vector2* mutable_center() { return &start_; }
    const Vector2& center() const { return start_; }
    Vector2* mutable_top_left() { return &start_; }
    Vector2* mutable_bottom_right() { return &end_; }
    const Vector2& top_left() const { return start_; }
    const Vector2& bottom_right() const { return end_; }
    // Radius
    Distance* mutable_radius() { return &width_; }
    const Distance& radius() const { return width_; }
    // Polygon
    PolyLine* mutable_polygon() { return &poly_; }
    const PolyLine& polygon() const { return poly_; }
    // Bezier
    Vector2* mutable_control1() { return &start_; }
    Vector2* mutable_control2() { return &end_; }
    // Type
    void set_type(int) {}
    int type() const { return 0; }
private:
    GraphicAttributes attrs_;
    Vector2 start_, end_;
    Distance width_;
    PolyLine poly_;
};

class ItemHeader {
public:
    ItemHeader() = default;
};

class CommandStatusResponse {
public:
    CommandStatusResponse() = default;
};

class DocumentSpecifier {
public:
    DocumentSpecifier() = default;
};

class ProjectSpecifier {
public:
    ProjectSpecifier() = default;
};

class KiCadVersion {
public:
    KiCadVersion() = default;
};

class Time {
public:
    Time() = default;
};

class TitleBlockInfo {
public:
    TitleBlockInfo() = default;
};

class CompoundShape {
public:
    CompoundShape() = default;
};

class GraphicFillAttributes {
public:
    GraphicFillAttributes() = default;
};

class GraphicSegmentAttributes {
public:
    GraphicSegmentAttributes() = default;
};

class GraphicCircleAttributes {
public:
    GraphicCircleAttributes() = default;
};

class GraphicArcAttributes {
public:
    GraphicArcAttributes() = default;
};

class GraphicRectangleAttributes {
public:
    GraphicRectangleAttributes() = default;
};

class GraphicBezierAttributes {
public:
    GraphicBezierAttributes() = default;
};

class ArcStartMidEnd {
public:
    ArcStartMidEnd() = default;
};

class SymbolPinId {
public:
    SymbolPinId() = default;
    const std::string& name() const { static std::string s; return s; }
    void set_name(const std::string&) {}
    int type() const { return 0; }
    void set_type(int) {}
    bool no_connect() const { return false; }
    void set_no_connect(bool) {}
};

}  // namespace types
}  // namespace common
}  // namespace kiapi
