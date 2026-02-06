/**
 * Minimal PGM_BASE implementation for WASM build.
 * Only implements methods that are NOT inline in pgm_base.h.
 */

#include <wx/intl.h>
#include <wx/snglinst.h>
#include <wx/app.h>

// Provide complete type definitions for unique_ptr members in PGM_BASE
// These would normally come from full KiCad headers but are stubs in WASM
class LIBRARY_MANAGER { public: virtual ~LIBRARY_MANAGER() = default; };
class SCRIPTING { public: virtual ~SCRIPTING() = default; };
class API_PLUGIN_MANAGER { public: virtual ~API_PLUGIN_MANAGER() = default; };
class KICAD_API_SERVER { public: virtual ~KICAD_API_SERVER() = default; };

#include <pgm_base.h>
#include <settings/settings_manager.h>
#include <background_jobs_monitor.h>
#include <notifications_manager.h>

// Global PGM_BASE pointer
static PGM_BASE* s_pgm = nullptr;

PGM_BASE& Pgm()
{
    wxASSERT( s_pgm );
    return *s_pgm;
}

PGM_BASE* PgmOrNull()
{
    return s_pgm;
}

void SetPgm( PGM_BASE* pgm )
{
    s_pgm = pgm;
}

PGM_BASE::PGM_BASE()
    : m_Printing( false ),
      m_Quitting( false ),
      m_PropertyGridInitialized( false )
{
}

PGM_BASE::~PGM_BASE()
{
    Destroy();
}

const wxString& PGM_BASE::GetExecutablePath() const
{
    static wxString path( "/" );
    return path;
}

bool PGM_BASE::SetLocalEnvVariable( const wxString& aName, const wxString& aValue )
{
    return true;
}

void PGM_BASE::SetLocalEnvVariables()
{
}

ENV_VAR_MAP& PGM_BASE::GetLocalEnvVariables() const
{
    static ENV_VAR_MAP vars;
    return vars;
}

void PGM_BASE::SetTextEditor( const wxString& aFileName )
{
}

const wxString& PGM_BASE::GetTextEditor( bool aCanShowFileChooser )
{
    static wxString editor;
    return editor;
}

const wxString PGM_BASE::AskUserForPreferredEditor( const wxString& aDefaultEditor )
{
    return aDefaultEditor;
}

bool PGM_BASE::SetLanguage( wxString& aErrMsg, bool first_time )
{
    return true;
}

bool PGM_BASE::SetDefaultLanguage( wxString& aErrMsg )
{
    return true;
}

void PGM_BASE::SetLanguageIdentifier( int menu_id )
{
}

wxString PGM_BASE::GetLanguageTag()
{
    return wxString( "en" );
}

void PGM_BASE::SetLanguagePath()
{
}

void PGM_BASE::ReadPdfBrowserInfos()
{
}

void PGM_BASE::WritePdfBrowserInfos()
{
}

bool PGM_BASE::InitPgm( bool aHeadless, bool aSkipPyInit, bool aIsUnitTest )
{
    // Initialize thread pool
    if( !m_singleton.m_ThreadPool )
    {
        m_singleton.m_ThreadPool = new BS::priority_thread_pool();
    }

    return true;
}

void PGM_BASE::Destroy()
{
}

void PGM_BASE::SaveCommonSettings()
{
}

void PGM_BASE::HandleException( std::exception_ptr aPtr, bool aUnhandled )
{
}

void PGM_BASE::HandleAssert( const wxString& aFile, int aLine, const wxString& aFunc,
                              const wxString& aCond, const wxString& aMsg )
{
}

wxApp& PGM_BASE::App()
{
    static wxApp* app = nullptr;
    if( !app )
        app = new wxApp();
    return *app;
}

bool PGM_BASE::IsGUI()
{
    return false;
}

void PGM_BASE::ShowSplash()
{
}

void PGM_BASE::HideSplash()
{
}

void PGM_BASE::BuildArgvUtf8()
{
}
