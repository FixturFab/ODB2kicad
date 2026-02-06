#pragma once
// Stub enums.pb.h for WASM build - minimal enums needed by kicommon_wasm

namespace kiapi {
namespace common {
namespace types {

enum CommandStatus : int { CS_OK = 0 };
enum FrameType : int { FT_UNKNOWN = 0 };
enum DocumentType : int { DT_UNKNOWN = 0 };
enum ItemRequestStatus : int { IRS_OK = 0 };
enum LockedState : int { LS_UNLOCKED = 0 };
enum GraphicFillType : int { GFT_NONE = 0, GFT_FILLED = 1 };
enum AxisAlignment : int { AA_HORIZONTAL = 0, AA_VERTICAL = 1 };
enum MapMergeMode : int { MMM_REPLACE = 0 };
enum ElectricalPinType : int { EPT_INPUT = 0 };
enum StrokeLineStyle : int { SLS_DEFAULT = 0, SLS_SOLID = 1, SLS_DASH = 2, SLS_DOT = 3 };

}  // namespace types
}  // namespace common
}  // namespace kiapi
