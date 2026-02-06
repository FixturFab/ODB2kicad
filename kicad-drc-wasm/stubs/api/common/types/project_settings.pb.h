#pragma once
// Stub project_settings.pb.h for WASM build
#include <string>
#include <vector>
#include <google/protobuf/message.h>
#include <api/common/types/base_types.pb.h>
#include <api/board/board_types.pb.h>

#ifndef KIAPI_IMPORTEXPORT
#define KIAPI_IMPORTEXPORT
#endif

namespace kiapi {
namespace common {
namespace project {

enum NetClassType : int {
    NCT_EXPLICIT = 0,
    NCT_IMPLICIT = 1
};

class NetClassBoardSettings {
public:
    NetClassBoardSettings() = default;

    bool has_clearance() const { return has_clearance_; }
    types::Distance* mutable_clearance() { has_clearance_ = true; return &clearance_; }
    const types::Distance& clearance() const { return clearance_; }

    bool has_track_width() const { return has_track_width_; }
    types::Distance* mutable_track_width() { has_track_width_ = true; return &track_width_; }
    const types::Distance& track_width() const { return track_width_; }

    bool has_diff_pair_track_width() const { return has_dpw_; }
    types::Distance* mutable_diff_pair_track_width() { has_dpw_ = true; return &dpw_; }
    const types::Distance& diff_pair_track_width() const { return dpw_; }

    bool has_diff_pair_gap() const { return has_dpg_; }
    types::Distance* mutable_diff_pair_gap() { has_dpg_ = true; return &dpg_; }
    const types::Distance& diff_pair_gap() const { return dpg_; }

    bool has_diff_pair_via_gap() const { return has_dpvg_; }
    types::Distance* mutable_diff_pair_via_gap() { has_dpvg_ = true; return &dpvg_; }
    const types::Distance& diff_pair_via_gap() const { return dpvg_; }

    bool has_via_stack() const { return has_via_stack_; }
    kiapi::board::types::PadStack* mutable_via_stack() { has_via_stack_ = true; return &via_stack_; }
    const kiapi::board::types::PadStack& via_stack() const { return via_stack_; }

    bool has_color() const { return false; }
    types::Color* mutable_color() { return &color_; }
    const types::Color& color() const { return color_; }

    void set_tuning_profile(const std::string&) {}
    const std::string& tuning_profile() const { static std::string s; return s; }
    bool has_tuning_profile() const { return false; }

private:
    types::Distance clearance_, track_width_, dpw_, dpg_, dpvg_;
    kiapi::board::types::PadStack via_stack_;
    types::Color color_;
    bool has_clearance_ = false, has_track_width_ = false, has_dpw_ = false;
    bool has_dpg_ = false, has_dpvg_ = false, has_via_stack_ = false;
};

class NetClassSchematicSettings {
public:
    NetClassSchematicSettings() = default;

    bool has_wire_width() const { return false; }
    types::Distance* mutable_wire_width() { return &wire_width_; }
    const types::Distance& wire_width() const { return wire_width_; }

    bool has_bus_width() const { return false; }
    types::Distance* mutable_bus_width() { return &bus_width_; }
    const types::Distance& bus_width() const { return bus_width_; }

    bool has_color() const { return false; }
    types::Color* mutable_color() { return &color_; }
    const types::Color& color() const { return color_; }

    bool has_line_style() const { return false; }

private:
    types::Distance wire_width_, bus_width_;
    types::Color color_;
};

class KIAPI_IMPORTEXPORT NetClass : public ::google::protobuf::Message {
public:
    NetClass() = default;

    void set_name(const std::string& n) { name_ = n; }
    const std::string& name() const { return name_; }

    void set_priority(int p) { priority_ = p; }
    int priority() const { return priority_; }

    void set_type(NetClassType t) { type_ = t; }
    NetClassType type() const { return type_; }

    void add_constituents(const std::string& s) { constituents_.push_back(s); }
    int constituents_size() const { return (int)constituents_.size(); }
    const std::string& constituents(int i) const { return constituents_[i]; }

    NetClassBoardSettings* mutable_board() { return &board_; }
    const NetClassBoardSettings& board() const { return board_; }

    NetClassSchematicSettings* mutable_schematic() { return &schematic_; }
    const NetClassSchematicSettings& schematic() const { return schematic_; }

private:
    std::string name_;
    int priority_ = 0;
    NetClassType type_ = NCT_EXPLICIT;
    std::vector<std::string> constituents_;
    NetClassBoardSettings board_;
    NetClassSchematicSettings schematic_;
};

class TextVariables : public ::google::protobuf::Message {
public:
    TextVariables() = default;
};

}  // namespace project
}  // namespace common
}  // namespace kiapi
