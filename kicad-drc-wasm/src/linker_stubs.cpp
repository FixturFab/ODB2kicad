/**
 * Linker stubs for WASM build.
 *
 * Provides empty/no-op implementations for symbols referenced by the
 * KiCad object code but not actually needed for DRC-only operation.
 * This avoids having to compile dozens of additional source files that
 * would pull in heavy dependencies (protobuf, OpenGL, pcbnew I/O plugins,
 * zint barcode, etc.).
 */

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstdio>

// ── Forward declarations (avoid pulling in heavy headers) ───────────────
// We define just enough to match the mangled symbol signatures.

#include <wx/string.h>

// Protobuf stub
namespace google { namespace protobuf {
class Any {};
template<typename T> class RepeatedField {
public:
    int size() const { return 0; }
    void Add(T) {}
    void Clear() {}
};
}}

// KiCad types needed for signatures
#include <layer_ids.h>
#include <lset.h>
#include <math/vector2d.h>
#include <math/vector3.h>
#include <geometry/eda_angle.h>

// Include full types needed for Unpack return values
#include <geometry/shape_poly_set.h>
#include <kiid.h>     // for KIID_PATH
#include <lib_id.h>   // for LIB_ID
#include <gal/color4d.h>

// ── wx global converter definitions ─────────────────────────────────────
#include <wx/strconv.h>
wxMBConvUTF8 wxConvUTF8;
wxCSConv wxConvLocal(wxString(wxT("")));
wxMBConv wxConvLibc;

// ── kiapi namespace stubs ───────────────────────────────────────────────
namespace kiapi {
namespace common {
namespace types {
    class Color {};
    class PolySet {};
    class SheetPath {};
    class Vector2 {};
    class Vector3D {};
    enum ElectricalPinType : int;
    enum HorizontalAlignment : int;
    enum VerticalAlignment : int;
}

void PackColor(types::Color&, const KIGFX::COLOR4D&) {}
KIGFX::COLOR4D UnpackColor(const types::Color&) { return KIGFX::COLOR4D(); }
void PackPolySet(types::PolySet&, const SHAPE_POLY_SET&) {}
SHAPE_POLY_SET UnpackPolySet(const types::PolySet&) { return SHAPE_POLY_SET(); }
void PackSheetPath(types::SheetPath&, const KIID_PATH&) {}
void PackVector2(types::Vector2&, const VECTOR2I&) {}
void PackVector3D(types::Vector3D&, const VECTOR3D&) {}

VECTOR2I UnpackVector2(const types::Vector2&) { return VECTOR2I(0,0); }
VECTOR3D UnpackVector3D(const types::Vector3D&) { return VECTOR3D(0,0,0); }
KIID_PATH UnpackSheetPath(const types::SheetPath&) { return KIID_PATH(); }
wxString TypeNameFromAny(const google::protobuf::Any&) { return wxT(""); }

namespace types { class LibraryIdentifier {}; }
void LibIdToProto(const LIB_ID&) {}
LIB_ID LibIdFromProto(const types::LibraryIdentifier&) { return LIB_ID(); }

} // namespace common

namespace board {
namespace types {
    enum BoardLayer : int;
    enum BoardStackupLayerType : int;
    enum DrillShape : int;
    enum PadStackShape : int;
    enum PadStackType : int;
    enum PadType : int;
    enum PlacementRuleSourceType : int;
    enum TeardropType : int;
    enum UnconnectedLayerRemoval : int;
    enum ViaType : int;
    enum ZoneBorderStyle : int;
    enum ZoneConnectionStyle : int;
    enum ZoneFillMode : int;
    enum IslandRemovalMode : int;
    enum DimensionArrowDirection : int;
    enum DimensionPrecision : int;
    enum DimensionTextBorderStyle : int;
    enum DimensionTextPosition : int;
    enum DimensionUnit : int;
    enum DimensionUnitFormat : int;
}

void PackLayerSet(google::protobuf::RepeatedField<int>&, const LSET&) {}
LSET UnpackLayerSet(const google::protobuf::RepeatedField<int>&) { return LSET(); }

// board.pb.h stub declares BoardStackupLayerType directly in kiapi::board::
enum BoardStackupLayerType : int;

} // namespace board
} // namespace kiapi

// ── ToProtoEnum template specializations ────────────────────────────────
// These are template specializations that must have the same mangling as
// the KiCad originals.

// Forward declare KiCad enums
enum PAD_ATTRIB : int;
enum PAD_DRILL_SHAPE : int;
enum PAD_SHAPE : int;
enum VIATYPE : int;
enum ZONE_CONNECTION : int;
enum UNCONNECTED_LAYER_MODE : int;
enum ISLAND_REMOVAL_MODE : int;
enum ZONE_FILL_MODE : int;
enum ZONE_BORDER_DISPLAY_STYLE : int;
enum PLACEMENT_SOURCE_T : int;
enum TEARDROP_TYPE : int;
enum DIM_TEXT_BORDER : int;
enum DIM_UNITS_FORMAT : int;
enum DIM_ARROW_DIRECTION : int;
enum DIM_PRECISION : int;
enum DIM_TEXT_POSITION : int;
enum DIM_UNITS_MODE : int;
enum BOARD_STACKUP_ITEM_TYPE : int;
enum ELECTRICAL_PINTYPE : int;
namespace PADSTACK { enum class MODE : int; }

// Include the real text_attributes header to get GR_TEXT_H_ALIGN_T / GR_TEXT_V_ALIGN_T
// (they use unscoped enums without fixed underlying type, can't forward-declare with `: int`)
#include <font/text_attributes.h>

template<typename KiCadEnum, typename ProtoEnum>
ProtoEnum ToProtoEnum(KiCadEnum);

// Board enum specializations
template<> kiapi::board::types::BoardLayer ToProtoEnum<PCB_LAYER_ID, kiapi::board::types::BoardLayer>(PCB_LAYER_ID) { return (kiapi::board::types::BoardLayer)0; }
template<> kiapi::board::types::PadType ToProtoEnum<PAD_ATTRIB, kiapi::board::types::PadType>(PAD_ATTRIB) { return (kiapi::board::types::PadType)0; }
template<> kiapi::board::types::DrillShape ToProtoEnum<PAD_DRILL_SHAPE, kiapi::board::types::DrillShape>(PAD_DRILL_SHAPE) { return (kiapi::board::types::DrillShape)0; }
template<> kiapi::board::types::PadStackShape ToProtoEnum<PAD_SHAPE, kiapi::board::types::PadStackShape>(PAD_SHAPE) { return (kiapi::board::types::PadStackShape)0; }
template<> kiapi::board::types::PadStackType ToProtoEnum<PADSTACK::MODE, kiapi::board::types::PadStackType>(PADSTACK::MODE) { return (kiapi::board::types::PadStackType)0; }
template<> kiapi::board::types::ViaType ToProtoEnum<VIATYPE, kiapi::board::types::ViaType>(VIATYPE) { return (kiapi::board::types::ViaType)0; }
template<> kiapi::board::types::ZoneConnectionStyle ToProtoEnum<ZONE_CONNECTION, kiapi::board::types::ZoneConnectionStyle>(ZONE_CONNECTION) { return (kiapi::board::types::ZoneConnectionStyle)0; }
template<> kiapi::board::types::UnconnectedLayerRemoval ToProtoEnum<UNCONNECTED_LAYER_MODE, kiapi::board::types::UnconnectedLayerRemoval>(UNCONNECTED_LAYER_MODE) { return (kiapi::board::types::UnconnectedLayerRemoval)0; }
template<> kiapi::board::types::IslandRemovalMode ToProtoEnum<ISLAND_REMOVAL_MODE, kiapi::board::types::IslandRemovalMode>(ISLAND_REMOVAL_MODE) { return (kiapi::board::types::IslandRemovalMode)0; }
template<> kiapi::board::types::ZoneFillMode ToProtoEnum<ZONE_FILL_MODE, kiapi::board::types::ZoneFillMode>(ZONE_FILL_MODE) { return (kiapi::board::types::ZoneFillMode)0; }
template<> kiapi::board::types::ZoneBorderStyle ToProtoEnum<ZONE_BORDER_DISPLAY_STYLE, kiapi::board::types::ZoneBorderStyle>(ZONE_BORDER_DISPLAY_STYLE) { return (kiapi::board::types::ZoneBorderStyle)0; }
template<> kiapi::board::types::PlacementRuleSourceType ToProtoEnum<PLACEMENT_SOURCE_T, kiapi::board::types::PlacementRuleSourceType>(PLACEMENT_SOURCE_T) { return (kiapi::board::types::PlacementRuleSourceType)0; }
template<> kiapi::board::types::TeardropType ToProtoEnum<TEARDROP_TYPE, kiapi::board::types::TeardropType>(TEARDROP_TYPE) { return (kiapi::board::types::TeardropType)0; }
template<> kiapi::board::types::DimensionTextBorderStyle ToProtoEnum<DIM_TEXT_BORDER, kiapi::board::types::DimensionTextBorderStyle>(DIM_TEXT_BORDER) { return (kiapi::board::types::DimensionTextBorderStyle)0; }
template<> kiapi::board::types::DimensionUnitFormat ToProtoEnum<DIM_UNITS_FORMAT, kiapi::board::types::DimensionUnitFormat>(DIM_UNITS_FORMAT) { return (kiapi::board::types::DimensionUnitFormat)0; }
template<> kiapi::board::types::DimensionArrowDirection ToProtoEnum<DIM_ARROW_DIRECTION, kiapi::board::types::DimensionArrowDirection>(DIM_ARROW_DIRECTION) { return (kiapi::board::types::DimensionArrowDirection)0; }
template<> kiapi::board::types::DimensionPrecision ToProtoEnum<DIM_PRECISION, kiapi::board::types::DimensionPrecision>(DIM_PRECISION) { return (kiapi::board::types::DimensionPrecision)0; }
template<> kiapi::board::types::DimensionTextPosition ToProtoEnum<DIM_TEXT_POSITION, kiapi::board::types::DimensionTextPosition>(DIM_TEXT_POSITION) { return (kiapi::board::types::DimensionTextPosition)0; }
template<> kiapi::board::types::DimensionUnit ToProtoEnum<DIM_UNITS_MODE, kiapi::board::types::DimensionUnit>(DIM_UNITS_MODE) { return (kiapi::board::types::DimensionUnit)0; }
template<> kiapi::board::types::BoardStackupLayerType ToProtoEnum<BOARD_STACKUP_ITEM_TYPE, kiapi::board::types::BoardStackupLayerType>(BOARD_STACKUP_ITEM_TYPE) { return (kiapi::board::types::BoardStackupLayerType)0; }
// board.pb.h stub declares BoardStackupLayerType in kiapi::board:: (no types:: sub-ns).
// Provide a specialization matching the mangled symbol the linker expects.
template<> kiapi::board::BoardStackupLayerType ToProtoEnum<BOARD_STACKUP_ITEM_TYPE, kiapi::board::BoardStackupLayerType>(BOARD_STACKUP_ITEM_TYPE) { return (kiapi::board::BoardStackupLayerType)0; }

// Common enum specializations
template<> kiapi::common::types::ElectricalPinType ToProtoEnum<ELECTRICAL_PINTYPE, kiapi::common::types::ElectricalPinType>(ELECTRICAL_PINTYPE) { return (kiapi::common::types::ElectricalPinType)0; }
template<> kiapi::common::types::HorizontalAlignment ToProtoEnum<GR_TEXT_H_ALIGN_T, kiapi::common::types::HorizontalAlignment>(GR_TEXT_H_ALIGN_T) { return (kiapi::common::types::HorizontalAlignment)0; }
template<> kiapi::common::types::VerticalAlignment ToProtoEnum<GR_TEXT_V_ALIGN_T, kiapi::common::types::VerticalAlignment>(GR_TEXT_V_ALIGN_T) { return (kiapi::common::types::VerticalAlignment)0; }

// ── FromProtoEnum template specializations ──────────────────────────────
// Reverse of ToProtoEnum - convert proto enums back to KiCad enums.
// Used by Deserialize() methods which we don't actually call, but which
// are linked in from board object files.

template<typename KiCadEnum, typename ProtoEnum>
KiCadEnum FromProtoEnum(ProtoEnum);

template<> PCB_LAYER_ID FromProtoEnum<PCB_LAYER_ID, kiapi::board::types::BoardLayer>(kiapi::board::types::BoardLayer) { return (PCB_LAYER_ID)0; }
template<> PAD_ATTRIB FromProtoEnum<PAD_ATTRIB, kiapi::board::types::PadType>(kiapi::board::types::PadType) { return (PAD_ATTRIB)0; }
template<> PAD_SHAPE FromProtoEnum<PAD_SHAPE, kiapi::board::types::PadStackShape>(kiapi::board::types::PadStackShape) { return (PAD_SHAPE)0; }
template<> PADSTACK::MODE FromProtoEnum<PADSTACK::MODE, kiapi::board::types::PadStackType>(kiapi::board::types::PadStackType) { return (PADSTACK::MODE)0; }
template<> ZONE_CONNECTION FromProtoEnum<ZONE_CONNECTION, kiapi::board::types::ZoneConnectionStyle>(kiapi::board::types::ZoneConnectionStyle) { return (ZONE_CONNECTION)0; }
template<> ELECTRICAL_PINTYPE FromProtoEnum<ELECTRICAL_PINTYPE, kiapi::common::types::ElectricalPinType>(kiapi::common::types::ElectricalPinType) { return (ELECTRICAL_PINTYPE)0; }
template<> PAD_DRILL_SHAPE FromProtoEnum<PAD_DRILL_SHAPE, kiapi::board::types::DrillShape>(kiapi::board::types::DrillShape) { return (PAD_DRILL_SHAPE)0; }
template<> UNCONNECTED_LAYER_MODE FromProtoEnum<UNCONNECTED_LAYER_MODE, kiapi::board::types::UnconnectedLayerRemoval>(kiapi::board::types::UnconnectedLayerRemoval) { return (UNCONNECTED_LAYER_MODE)0; }
template<> VIATYPE FromProtoEnum<VIATYPE, kiapi::board::types::ViaType>(kiapi::board::types::ViaType) { return (VIATYPE)0; }
template<> GR_TEXT_H_ALIGN_T FromProtoEnum<GR_TEXT_H_ALIGN_T, kiapi::common::types::HorizontalAlignment>(kiapi::common::types::HorizontalAlignment) { return (GR_TEXT_H_ALIGN_T)0; }
template<> GR_TEXT_V_ALIGN_T FromProtoEnum<GR_TEXT_V_ALIGN_T, kiapi::common::types::VerticalAlignment>(kiapi::common::types::VerticalAlignment) { return (GR_TEXT_V_ALIGN_T)0; }
template<> DIM_UNITS_MODE FromProtoEnum<DIM_UNITS_MODE, kiapi::board::types::DimensionUnit>(kiapi::board::types::DimensionUnit) { return (DIM_UNITS_MODE)0; }
template<> DIM_UNITS_FORMAT FromProtoEnum<DIM_UNITS_FORMAT, kiapi::board::types::DimensionUnitFormat>(kiapi::board::types::DimensionUnitFormat) { return (DIM_UNITS_FORMAT)0; }
template<> DIM_ARROW_DIRECTION FromProtoEnum<DIM_ARROW_DIRECTION, kiapi::board::types::DimensionArrowDirection>(kiapi::board::types::DimensionArrowDirection) { return (DIM_ARROW_DIRECTION)0; }
template<> DIM_PRECISION FromProtoEnum<DIM_PRECISION, kiapi::board::types::DimensionPrecision>(kiapi::board::types::DimensionPrecision) { return (DIM_PRECISION)0; }
template<> DIM_TEXT_POSITION FromProtoEnum<DIM_TEXT_POSITION, kiapi::board::types::DimensionTextPosition>(kiapi::board::types::DimensionTextPosition) { return (DIM_TEXT_POSITION)0; }
template<> DIM_TEXT_BORDER FromProtoEnum<DIM_TEXT_BORDER, kiapi::board::types::DimensionTextBorderStyle>(kiapi::board::types::DimensionTextBorderStyle) { return (DIM_TEXT_BORDER)0; }
template<> PLACEMENT_SOURCE_T FromProtoEnum<PLACEMENT_SOURCE_T, kiapi::board::types::PlacementRuleSourceType>(kiapi::board::types::PlacementRuleSourceType) { return (PLACEMENT_SOURCE_T)0; }
template<> ISLAND_REMOVAL_MODE FromProtoEnum<ISLAND_REMOVAL_MODE, kiapi::board::types::IslandRemovalMode>(kiapi::board::types::IslandRemovalMode) { return (ISLAND_REMOVAL_MODE)0; }
template<> ZONE_FILL_MODE FromProtoEnum<ZONE_FILL_MODE, kiapi::board::types::ZoneFillMode>(kiapi::board::types::ZoneFillMode) { return (ZONE_FILL_MODE)0; }
template<> TEARDROP_TYPE FromProtoEnum<TEARDROP_TYPE, kiapi::board::types::TeardropType>(kiapi::board::types::TeardropType) { return (TEARDROP_TYPE)0; }
template<> ZONE_BORDER_DISPLAY_STYLE FromProtoEnum<ZONE_BORDER_DISPLAY_STYLE, kiapi::board::types::ZoneBorderStyle>(kiapi::board::types::ZoneBorderStyle) { return (ZONE_BORDER_DISPLAY_STYLE)0; }
template<> BOARD_STACKUP_ITEM_TYPE FromProtoEnum<BOARD_STACKUP_ITEM_TYPE, kiapi::board::types::BoardStackupLayerType>(kiapi::board::types::BoardStackupLayerType) { return (BOARD_STACKUP_ITEM_TYPE)0; }

// ── CreateItemForType stub ──────────────────────────────────────────────
// Used by footprint.cpp Deserialize() - we don't call Deserialize.
#include <core/typeinfo.h>
class BOARD_ITEM;
class BOARD_ITEM_CONTAINER;
BOARD_ITEM* CreateItemForType(KICAD_T, BOARD_ITEM_CONTAINER*) { return nullptr; }

// PCB I/O plugin stubs removed - WASM build uses pcb_io_mgr_wasm.cpp
// which only registers kicad_sexpr and legacy plugins.

// ── Zint barcode stubs ──────────────────────────────────────────────────
extern "C" {
struct zint_symbol;
struct zint_vector;
void* ZBarcode_Create() { return nullptr; }
void ZBarcode_Delete(void*) {}
int ZBarcode_Encode(void*, const unsigned char*, int) { return -1; }
int ZBarcode_Buffer_Vector(void*, int) { return -1; }
}

// ── GL_CONTEXT_MANAGER stub ─────────────────────────────────────────────
// Must define out-of-line so the linker sees the mangled class method symbol.
class GL_CONTEXT_MANAGER {
public:
    static GL_CONTEXT_MANAGER& Get();
    void DeleteAll();
};

GL_CONTEXT_MANAGER& GL_CONTEXT_MANAGER::Get()
{
    static GL_CONTEXT_MANAGER inst;
    return inst;
}

void GL_CONTEXT_MANAGER::DeleteAll() {}

// ── PGM_BASE::GetCommonSettings stub ────────────────────────────────────
// This is missing from pgm_base_wasm.cpp. Let's provide it here.
#include <pgm_base.h>
#include <settings/common_settings.h>

COMMON_SETTINGS* PGM_BASE::GetCommonSettings() const
{
    // Return nullptr - DRC doesn't use common settings in the critical path
    return nullptr;
}

// ── NETLIST / COMPONENT stubs ───────────────────────────────────────────
// Used by drc_test_provider_schematic_parity.cpp but not critical for DRC
class COMPONENT {
public:
    struct NET {
        wxString m_pinName;
        wxString m_netName;
    };
    const NET* GetNet(const wxString&) const { return nullptr; }
};

class NETLIST {
public:
    COMPONENT* GetComponentByReference(const wxString&) { return nullptr; }
};

// ── FOOTPRINT_LIBRARY_ADAPTER stubs ─────────────────────────────────────
// These are UI-related and not needed for DRC
class LIBRARY_MANAGER;
class LIBRARY_MANAGER_ADAPTER;

// ── ui_common.cpp stubs (removed from kicommon_wasm, too many GUI deps) ──
#include <widgets/ui_common.h>

SEVERITY SeverityFromString( const wxString& aSeverity )
{
    if( aSeverity == wxT( "warning" ) )
        return RPT_SEVERITY_WARNING;
    else if( aSeverity == wxT( "ignore" ) )
        return RPT_SEVERITY_IGNORE;
    else
        return RPT_SEVERITY_ERROR;
}

wxString SeverityToString( const SEVERITY& aSeverity )
{
    if( aSeverity == RPT_SEVERITY_IGNORE )
        return wxT( "ignore" );
    else if( aSeverity == RPT_SEVERITY_WARNING )
        return wxT( "warning" );
    else
        return wxT( "error" );
}

int KIUI::GetStdMargin() { return 5; }

wxSize KIUI::GetTextSize( const wxString& aSingleLine, wxWindow* aWindow )
{
    // Approximate: 8 pixels per character width, 16 pixels height
    return wxSize( (int)aSingleLine.length() * 8, 16 );
}

wxFont KIUI::GetMonospacedUIFont() { return wxFont(); }
wxFont KIUI::GetControlFont( wxWindow* ) { return wxFont(); }
wxFont KIUI::GetInfoFont( wxWindow* ) { return wxFont(); }
wxFont KIUI::GetSmallInfoFont( wxWindow* ) { return wxFont(); }
wxFont KIUI::GetDockedPaneFont( wxWindow* ) { return wxFont(); }
wxFont KIUI::GetStatusFont( wxWindow* ) { return wxFont(); }

bool KIUI::EnsureTextCtrlWidth( wxTextCtrl* aCtrl, const wxString* aString ) { return false; }
void KIUI::SelectReferenceNumber( wxTextEntry* aTextEntry ) {}
wxString KIUI::EllipsizeStatusText( wxWindow* aWindow, const wxString& aString ) { return aString; }
wxString KIUI::EllipsizeMenuText( const wxString& aString )
{
    if( aString.length() > 36 )
        return aString.substr( 0, 33 ) + wxT( "..." );
    return aString;
}
bool KIUI::IsInputControlFocused( wxWindow* ) { return false; }
bool KIUI::IsInputControlEditable( wxWindow* ) { return false; }
bool KIUI::IsModalDialogFocused() { return false; }
void KIUI::Disable( wxWindow* ) {}
void KIUI::AddBitmapToMenuItem( wxMenuItem*, const wxBitmapBundle& ) {}
wxMenuItem* KIUI::AddMenuItem( wxMenu* aMenu, int aId, const wxString& aText,
                                const wxBitmapBundle& aImage, wxItemKind aType )
{ return nullptr; }
wxMenuItem* KIUI::AddMenuItem( wxMenu* aMenu, int aId, const wxString& aText,
                                const wxString& aHelpText, const wxBitmapBundle& aImage,
                                wxItemKind aType )
{ return nullptr; }
wxMenuItem* KIUI::AddMenuItem( wxMenu* aMenu, wxMenu* aSubMenu, int aId,
                                const wxString& aText, const wxBitmapBundle& aImage )
{ return nullptr; }
wxMenuItem* KIUI::AddMenuItem( wxMenu* aMenu, wxMenu* aSubMenu, int aId,
                                const wxString& aText, const wxString& aHelpText,
                                const wxBitmapBundle& aImage )
{ return nullptr; }

const wxString KIUI::s_FocusStealableInputName = wxT( "kiui_focus_stealable" );

// ── GR basic drawing stubs (gr_basic.cpp) ───────────────────────────────
// These are wxDC-based drawing functions used by gr_text.cpp and bitmap_base.cpp.
// Not needed for DRC (no actual rendering).
#include <gal/color4d.h>

enum wxPenStyle : int;

void GRLine( wxDC* aDC, const VECTOR2I& aStart, const VECTOR2I& aEnd, int aWidth,
             const KIGFX::COLOR4D& aColor, wxPenStyle aStyle = (wxPenStyle)0 ) {}

void GRLine( wxDC* DC, int x1, int y1, int x2, int y2, int width,
             const KIGFX::COLOR4D& Color, wxPenStyle aStyle = (wxPenStyle)0 ) {}

void GRCSegm( wxDC* aDC, const VECTOR2I& aStart, const VECTOR2I& aEnd, int aWidth,
              const KIGFX::COLOR4D& aColor ) {}

void GRClosedPoly( wxDC* aDC, int aPointCount, const VECTOR2I* aPoints, bool doFill,
                   const KIGFX::COLOR4D& aColor ) {}

void GRPoly( wxDC* DC, int n, const VECTOR2I* Points, bool Fill, int width,
             const KIGFX::COLOR4D& Color, const KIGFX::COLOR4D& BgColor ) {}

void GRRect( wxDC* DC, const VECTOR2I& aStart, const VECTOR2I& aEnd, int aWidth,
             const KIGFX::COLOR4D& aColor ) {}

static bool s_ForceBlackPen = false;
bool GetGRForceBlackPenState() { return s_ForceBlackPen; }
void SetGRForceBlackPenState( bool state ) { s_ForceBlackPen = state; }

// ── TEXT_EVAL_VCS stubs ─────────────────────────────────────────────────
// VCS (git) text evaluation functions - not needed for DRC.
// These use libgit2 which we don't have in WASM.
namespace TEXT_EVAL_VCS {
    std::string GetCommitHash( const std::string& aPath, int aLength ) { return ""; }
    std::string GetNearestTag( const std::string& aMatch, bool aAnyTags ) { return ""; }
    int GetDistanceFromTag( const std::string& aMatch, bool aAnyTags ) { return 0; }
    bool IsDirty( bool aIncludeUntracked ) { return false; }
    std::string GetAuthor( const std::string& aPath ) { return ""; }
    std::string GetAuthorEmail( const std::string& aPath ) { return ""; }
    std::string GetCommitter( const std::string& aPath ) { return ""; }
    std::string GetCommitterEmail( const std::string& aPath ) { return ""; }
    std::string GetBranch() { return ""; }
    int64_t GetCommitTimestamp( const std::string& aPath ) { return 0; }
    std::string GetCommitDate( const std::string& aPath ) { return ""; }
}

// ── libc++ atomic wait stubs ────────────────────────────────────────────
// Emscripten libc++-mt has __cxx_contention_t = int64_t but wasm32 compilation
// generates references with __cxx_contention_t = int32_t. Provide matching stubs.
#include <__atomic/contention_t.h>

namespace std {
inline namespace __2 {
    _LIBCPP_EXPORTED_FROM_ABI void
    __libcpp_atomic_wait(void const volatile*, __cxx_contention_t) noexcept {}
    _LIBCPP_EXPORTED_FROM_ABI __cxx_contention_t
    __libcpp_atomic_monitor(void const volatile*) noexcept { return 0; }
}}

// ── ENV_VAR stubs ────────────────────────────────────────────────────────
// env_vars.cpp excluded because its static globals call wxString::Format
// with wxString varargs, which crashes our vsnprintf-based stub.
#include <env_vars.h>
#include <build_version.h>

static std::vector<wxString> s_emptyPredefinedVars;

bool ENV_VAR::IsEnvVarImmutable( const wxString& ) { return false; }
const std::vector<wxString>& ENV_VAR::GetPredefinedEnvVars() { return s_emptyPredefinedVars; }
void ENV_VAR::GetEnvVarAutocompleteTokens( wxArrayString* ) {}

wxString ENV_VAR::GetVersionedEnvVarName( const wxString& aBaseName )
{
    // Safe version that avoids wxString::Format with wxString varargs
    int version = 0;
    std::tie(version, std::ignore, std::ignore) = GetMajorMinorPatchTuple();
    char buf[256];
    snprintf(buf, sizeof(buf), "KICAD%d_", version);
    return wxString(buf) + aBaseName;
}

std::optional<wxString> ENV_VAR::GetVersionedEnvVarValue(
    const std::map<wxString, ENV_VAR_ITEM>&, const wxString& )
{ return std::nullopt; }

wxString ENV_VAR::LookUpEnvVarHelp( const wxString& ) { return wxT(""); }

template<> std::optional<double> ENV_VAR::GetEnvVar( const wxString& aEnvVarName )
{
    const char* val = getenv(aEnvVarName.c_str());
    if( val )
        return std::stod(val);
    return std::nullopt;
}

template<> std::optional<wxString> ENV_VAR::GetEnvVar( const wxString& aEnvVarName )
{
    const char* val = getenv(aEnvVarName.c_str());
    if( val )
        return wxString(val);
    return std::nullopt;
}

// ── DS_DRAW_ITEM_LIST::BuildFullText stub ────────────────────────────────
// Must go after all forward enum decls to avoid redeclaration conflicts.
#include <drawing_sheet/ds_draw_item.h>

wxString DS_DRAW_ITEM_LIST::BuildFullText( const wxString& aTextbase )
{
    return aTextbase;
}
