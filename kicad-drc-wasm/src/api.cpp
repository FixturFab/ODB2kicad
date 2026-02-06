/**
 * Isolated DRC API implementation.
 *
 * Links against KiCad's existing libraries to provide DRC as a standalone
 * function callable without wxWidgets application infrastructure.
 */

#include "kicad_drc_api.h"

#include <memory>
#include <string>
#include <sstream>
#include <fstream>

// KiCad headers
#include <board.h>
#include <board_design_settings.h>
#include <pcb_marker.h>
#include <richio.h>
#include <pcb_io/kicad_sexpr/pcb_io_kicad_sexpr.h>
#include <drc/drc_engine.h>
#include <drc/drc_item.h>
#include <drc/drc_report.h>
#include <pgm_base.h>
#include <advanced_config.h>
#include <connectivity/connectivity_data.h>
#include <marker_base.h>
#include <kiface_base.h>
#include <thread_pool.h>
#include <wx/filename.h>

// Stub for Kiface() - required by some KiCad library code but not used in DRC path
static KIFACE_BASE* s_kiface_stub = nullptr;

KIFACE_BASE& Kiface()
{
    wxASSERT( s_kiface_stub );
    return *s_kiface_stub;
}

// Minimal PGM_BASE subclass for standalone DRC operation.
// Provides the thread pool needed by connectivity algorithms and satisfies
// Pgm() references from GAL and other transitive dependencies.
class PGM_DRC_STANDALONE : public PGM_BASE
{
public:
    PGM_DRC_STANDALONE() : PGM_BASE()
    {
        // Initialize the thread pool required by connectivity algorithms.
        // KICAD_SINGLETON::Init() is not exported from libkicommon.so,
        // so we set the pointer directly.
        int num_threads = std::max( 0, ADVANCED_CFG::GetCfg().m_MaximumThreads );
        m_singleton.m_ThreadPool = new BS::priority_thread_pool( num_threads );
    }

    ~PGM_DRC_STANDALONE()
    {
        delete m_singleton.m_ThreadPool;
        m_singleton.m_ThreadPool = nullptr;
    }

    void MacOpenFile( const wxString& ) override {}
};

// Global state
static std::unique_ptr<BOARD>              g_board;
static std::string                         g_json_result;
static std::unique_ptr<PGM_DRC_STANDALONE> g_pgm;

static void ensure_pgm_initialized()
{
    if( !g_pgm )
    {
        g_pgm = std::make_unique<PGM_DRC_STANDALONE>();
        SetPgm( g_pgm.get() );
    }
}

extern "C" {

int kicad_load_pcb( const char* pcb_content, size_t length )
{
    ensure_pgm_initialized();

    g_board.reset();
    g_json_result.clear();

    if( !pcb_content )
        return -1;

    std::string content;
    if( length == 0 )
        content = std::string( pcb_content );
    else
        content = std::string( pcb_content, length );

    try
    {
        STRING_LINE_READER reader( content, wxS( "input.kicad_pcb" ) );
        PCB_IO_KICAD_SEXPR plugin;
        BOARD* board = plugin.DoLoad( reader, nullptr, nullptr, nullptr, 0 );

        if( !board )
            return -2;

        g_board.reset( board );

        // Build connectivity data (needed for unconnected items DRC check)
        g_board->BuildConnectivity();

        return 0;
    }
    catch( const IO_ERROR& e )
    {
        fprintf( stderr, "PCB parse error: %s\n",
                 static_cast<const char*>( e.What().mb_str() ) );
        return -3;
    }
    catch( const std::exception& e )
    {
        fprintf( stderr, "PCB parse error: %s\n", e.what() );
        return -4;
    }
}

int kicad_run_drc( void )
{
    if( !g_board )
        return -1;

    g_json_result.clear();

    BOARD_DESIGN_SETTINGS& bds = g_board->GetDesignSettings();

    if( !bds.m_DRCEngine )
        bds.m_DRCEngine = std::make_shared<DRC_ENGINE>( g_board.get(), &bds );

    std::shared_ptr<DRC_ENGINE> drcEngine = bds.m_DRCEngine;
    drcEngine->SetBoard( g_board.get() );
    drcEngine->SetDesignSettings( &bds );

    // Initialize the DRC engine - loads default rules and registers test providers
    try
    {
        drcEngine->InitEngine( wxFileName() );
    }
    catch( ... )
    {
        // Best effort - continue with default rules even if custom rules fail
    }

    g_board->DeleteMARKERs( true, true );

    // Set violation handler: create markers and add them to the board
    drcEngine->SetViolationHandler(
            [&]( const std::shared_ptr<DRC_ITEM>& aItem, const VECTOR2I& aPos, int aLayer,
                 const std::function<void( PCB_MARKER* )>& aPathGenerator )
            {
                PCB_MARKER* marker = new PCB_MARKER( aItem, aPos, aLayer );
                aPathGenerator( marker );
                g_board->Add( marker );
            } );

    drcEngine->RunTests( EDA_UNITS::MM, true /*reportAllTrackErrors*/, false /*testFootprints*/ );
    drcEngine->ClearViolationHandler();

    // Write JSON report to a temporary file and read it back
    wxString tmpPath = wxFileName::CreateTempFileName( wxS( "drc_report" ) );

    auto markersProvider = std::make_shared<DRC_ITEMS_PROVIDER>(
            g_board.get(), MARKER_BASE::MARKER_DRC, MARKER_BASE::MARKER_DRAWING_SHEET );
    auto ratsnestProvider = std::make_shared<DRC_ITEMS_PROVIDER>(
            g_board.get(), MARKER_BASE::MARKER_RATSNEST );
    auto fpWarningsProvider = std::make_shared<DRC_ITEMS_PROVIDER>(
            g_board.get(), MARKER_BASE::MARKER_PARITY );

    int severities = RPT_SEVERITY_ERROR | RPT_SEVERITY_WARNING;
    markersProvider->SetSeverities( severities );
    ratsnestProvider->SetSeverities( severities );
    fpWarningsProvider->SetSeverities( severities );

    DRC_REPORT reportWriter( g_board.get(), EDA_UNITS::MM,
                             markersProvider, ratsnestProvider, fpWarningsProvider );

    bool ok = reportWriter.WriteJsonReport( tmpPath );

    if( ok )
    {
        std::ifstream ifs( static_cast<const char*>( tmpPath.mb_str() ) );
        if( ifs )
        {
            std::ostringstream oss;
            oss << ifs.rdbuf();
            g_json_result = oss.str();
        }
        wxRemoveFile( tmpPath );
    }

    int count = markersProvider->GetCount() + ratsnestProvider->GetCount()
                + fpWarningsProvider->GetCount();

    return count;
}

const char* kicad_get_drc_results( void )
{
    if( g_json_result.empty() )
        return nullptr;

    return g_json_result.c_str();
}

void kicad_cleanup( void )
{
    g_json_result.clear();
    g_board.reset();
}

} // extern "C"
