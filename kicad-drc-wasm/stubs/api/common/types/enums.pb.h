#pragma once
// Stub enums.pb.h for WASM build - enums needed by pcbnew Serialize/Deserialize

namespace kiapi {
namespace common {
namespace types {

enum CommandStatus : int { CS_OK = 0 };
enum FrameType : int { FT_UNKNOWN = 0 };
enum DocumentType : int { DT_UNKNOWN = 0 };
enum ItemRequestStatus : int { IRS_OK = 0 };
enum LockedState : int { LS_UNLOCKED = 0, LS_LOCKED = 1 };
enum GraphicFillType : int { GFT_NONE = 0, GFT_FILLED = 1, GFT_UNFILLED = 2 };
enum AxisAlignment : int { AA_HORIZONTAL = 0, AA_VERTICAL = 1, AA_X_AXIS = 2, AA_Y_AXIS = 3 };
enum MapMergeMode : int { MMM_REPLACE = 0 };
enum ElectricalPinType : int { EPT_INPUT = 0, EPT_UNKNOWN = -1 };
enum StrokeLineStyle : int { SLS_DEFAULT = 0, SLS_SOLID = 1, SLS_DASH = 2, SLS_DOT = 3, SLS_DASHDOT = 4, SLS_DASHDOTDOT = 5 };
enum HorizontalAlignment : int { HA_LEFT = 0, HA_CENTER = 1, HA_RIGHT = 2 };
enum VerticalAlignment : int { VA_TOP = 0, VA_CENTER = 1, VA_BOTTOM = 2 };
enum GraphicShapeType : int { GST_UNKNOWN = 0, GST_SEGMENT = 1, GST_RECT = 2, GST_ARC = 3, GST_CIRCLE = 4, GST_POLY = 5, GST_BEZIER = 6 };

}  // namespace types
}  // namespace common
}  // namespace kiapi
