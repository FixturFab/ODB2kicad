#pragma once
// Stub base_types.pb.h for WASM build
#include <string>
#include <vector>
#include <google/protobuf/message.h>
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
};

class LibraryIdentifier {
public:
    LibraryIdentifier() = default;
    const std::string& library_name() const { static std::string s; return s; }
    const std::string& entry_name() const { static std::string s; return s; }
    void set_library_name(const std::string&) {}
    void set_entry_name(const std::string&) {}
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
};

class TextAttributes {
public:
    TextAttributes() = default;
};

class Text {
public:
    Text() = default;
};

class GraphicAttributes {
public:
    GraphicAttributes() = default;
};

class GraphicShape {
public:
    GraphicShape() = default;
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

class TextBox {
public:
    TextBox() = default;
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

}  // namespace types
}  // namespace common
}  // namespace kiapi
