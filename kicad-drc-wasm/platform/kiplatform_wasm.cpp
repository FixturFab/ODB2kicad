/**
 * Platform stubs for WebAssembly build.
 * Provides minimal implementations of all kiplatform functions.
 */

#include <wx/string.h>
#include <wx/filename.h>
#include <wx/gdicmn.h>
#include <wx/window.h>

#include <kiplatform/app.h>
#include <kiplatform/environment.h>
#include <kiplatform/io.h>
#include <kiplatform/policy.h>
#include <kiplatform/drivers.h>
#include <kiplatform/ui.h>
#include <kiplatform/secrets.h>

#include <cstdio>
#include <string>
#include <vector>

// ── APP ─────────────────────────────────────────────────────────────────────

namespace KIPLATFORM { namespace APP {

bool Init()
{
    return true;
}

bool AttachConsole( bool aTryAlloc )
{
    return true;
}

bool IsOperatingSystemUnsupported()
{
    return false;
}

bool RegisterApplicationRestart( const wxString& aCommandLine )
{
    return false;
}

bool UnregisterApplicationRestart()
{
    return false;
}

bool SupportsShutdownBlockReason()
{
    return false;
}

void SetShutdownBlockReason( wxWindow* aWindow, const wxString& aReason )
{
}

void RemoveShutdownBlockReason( wxWindow* aWindow )
{
}

void ForceTimerMessagesToBeCreatedIfNecessary()
{
}

void AddDynamicLibrarySearchPath( const wxString& aPath )
{
}

}} // namespace KIPLATFORM::APP

// ── ENVIRONMENT ─────────────────────────────────────────────────────────────

namespace KIPLATFORM { namespace ENV {

void Init()
{
}

bool MoveToTrash( const wxString& aPath, wxString& aError )
{
    aError = wxString("Not supported in WASM");
    return false;
}

bool IsNetworkPath( const wxString& aPath )
{
    return false;
}

wxString GetDocumentsPath()
{
    return wxString("/");
}

wxString GetUserConfigPath()
{
    return wxString("/config");
}

wxString GetUserDataPath()
{
    return wxString("/data");
}

wxString GetUserLocalDataPath()
{
    return wxString("/data");
}

wxString GetUserCachePath()
{
    return wxString("/cache");
}

bool GetSystemProxyConfig( const wxString& aURL, PROXY_CONFIG& aCfg )
{
    return false;
}

bool VerifyFileSignature( const wxString& aPath )
{
    return true;
}

wxString GetAppUserModelId()
{
    return wxString("KiCad.DRC.WASM");
}

void SetAppDetailsForWindow( wxWindow* aWindow, const wxString& aRelaunchCommand,
                             const wxString& aRelaunchDisplayName )
{
}

wxString GetCommandLineStr()
{
    return wxString();
}

void AddToRecentDocs( const wxString& aPath )
{
}

}} // namespace KIPLATFORM::ENV

// ── IO ──────────────────────────────────────────────────────────────────────

namespace KIPLATFORM { namespace IO {

FILE* SeqFOpen( const wxString& aPath, const wxString& mode )
{
    return fopen( aPath.c_str(), mode.c_str() );
}

bool DuplicatePermissions( const wxString& aSrc, const wxString& aDest )
{
    return true;
}

bool MakeWriteable( const wxString& aFilePath )
{
    return true;
}

bool IsFileHidden( const wxString& aFileName )
{
    return false;
}

void LongPathAdjustment( wxFileName& aFilename )
{
}

long long TimestampDir( const wxString& aDirPath, const wxString& aFilespec )
{
    return 0;
}

}} // namespace KIPLATFORM::IO

// ── POLICY ──────────────────────────────────────────────────────────────────

namespace KIPLATFORM { namespace POLICY {

PBOOL GetPolicyBool( const wxString& aKey )
{
    return PBOOL::NOT_CONFIGURED;
}

std::uint32_t GetPolicyEnumUInt( const wxString& aKey )
{
    return 0;
}

}} // namespace KIPLATFORM::POLICY

// ── DRIVERS ─────────────────────────────────────────────────────────────────

namespace KIPLATFORM { namespace DRIVERS {

bool Valid3DConnexionDriverVersion()
{
    return false;
}

}} // namespace KIPLATFORM::DRIVERS

// ── UI ──────────────────────────────────────────────────────────────────────

namespace KIPLATFORM { namespace UI {

bool IsDarkTheme()
{
    return false;
}

wxColour GetDialogBGColour()
{
    return wxColour(240, 240, 240);
}

void ForceFocus( wxWindow* aWindow )
{
}

bool IsWindowActive( wxWindow* aWindow )
{
    return false;
}

void ReparentModal( wxWindow* aWindow )
{
}

void FixupCancelButtonCmdKeyCollision( wxWindow* aWindow )
{
}

bool IsStockCursorOk( wxStockCursor aCursor )
{
    return true;
}

void LargeChoiceBoxHack( wxChoice* aChoice )
{
}

void EllipsizeChoiceBox( wxChoice* aChoice )
{
}

double GetPixelScaleFactor( const wxWindow* aWindow )
{
    return 1.0;
}

double GetContentScaleFactor( const wxWindow* aWindow )
{
    return 1.0;
}

void GetInfoBarColours( wxColour& aFGColour, wxColour& aBGColour )
{
    aFGColour = wxColour(0, 0, 0);
    aBGColour = wxColour(240, 240, 240);
}

wxSize GetUnobscuredSize( const wxWindow* aWindow )
{
    return wxSize(800, 600);
}

void SetOverlayScrolling( const wxWindow* aWindow, bool overlay )
{
}

bool AllowIconsInMenus()
{
    return false;
}

wxPoint GetMousePosition()
{
    return wxPoint(0, 0);
}

bool WarpPointer( wxWindow* aWindow, int aX, int aY )
{
    return false;
}

void ImmControl( wxWindow* aWindow, bool aEnable )
{
}

void ImeNotifyCancelComposition( wxWindow* aWindow )
{
}

bool InfiniteDragPrepareWindow( wxWindow* aWindow )
{
    return false;
}

void InfiniteDragReleaseWindow()
{
}

void EnsureVisible( wxWindow* aWindow )
{
}

void SetFloatLevel( wxWindow* aWindow )
{
}

void AllowNetworkFileSystems( wxDialog* aDialog )
{
}

}} // namespace KIPLATFORM::UI

// ── SECRETS ─────────────────────────────────────────────────────────────────

namespace KIPLATFORM { namespace SECRETS {

bool StoreSecret( const wxString& aService, const wxString& aKey, const wxString& aSecret )
{
    return false;
}

bool GetSecret( const wxString& aService, const wxString& aKey, wxString& aSecret )
{
    return false;
}

}} // namespace KIPLATFORM::SECRETS
