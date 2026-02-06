/**
 * Test program for the isolated DRC library.
 *
 * Usage: ./test_drc <path-to-kicad_pcb>
 *
 * Reads a .kicad_pcb file from disk, runs DRC, and prints the JSON results.
 */

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include "kicad_drc_api.h"

#include <wx/app.h>
#include <wx/init.h>

// wxIMPLEMENT_APP_NO_MAIN provides wxGetApp() and wxCreateApp() so
// that wxInitialize() can create a wxAppConsole singleton.
class DRCApp : public wxAppConsole
{
public:
    bool OnInit() override { return true; }
    int  OnRun() override { return 0; }
};

wxIMPLEMENT_APP_NO_MAIN( DRCApp );

int main( int argc, char* argv[] )
{
    // Initialize wxWidgets - KiCad libraries depend on it
    wxInitialize( argc, argv );

    if( argc < 2 )
    {
        fprintf( stderr, "Usage: %s <path-to-kicad_pcb>\n", argv[0] );
        wxUninitialize();
        return 1;
    }

    const char* pcb_path = argv[1];

    // Read the PCB file into memory
    std::ifstream ifs( pcb_path );
    if( !ifs )
    {
        fprintf( stderr, "Error: cannot open '%s'\n", pcb_path );
        return 1;
    }

    std::ostringstream oss;
    oss << ifs.rdbuf();
    std::string pcb_content = oss.str();
    ifs.close();

    printf( "Loading PCB (%zu bytes)...\n", pcb_content.size() );

    int rc = kicad_load_pcb( pcb_content.c_str(), pcb_content.size() );
    if( rc != 0 )
    {
        fprintf( stderr, "Error: kicad_load_pcb failed with code %d\n", rc );
        return 1;
    }

    printf( "Running DRC...\n" );

    int violations = kicad_run_drc();
    if( violations < 0 )
    {
        fprintf( stderr, "Error: kicad_run_drc failed with code %d\n", violations );
        kicad_cleanup();
        return 1;
    }

    printf( "Found %d violations.\n\n", violations );

    const char* json = kicad_get_drc_results();
    if( json )
    {
        printf( "=== DRC JSON Report ===\n%s\n", json );
    }
    else
    {
        fprintf( stderr, "Warning: no JSON results available\n" );
    }

    kicad_cleanup();
    printf( "Done.\n" );

    return 0;
}
