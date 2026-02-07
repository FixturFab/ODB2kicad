/**
 * Simplified pcb_io_mgr.cpp for WASM build.
 * Only registers kicad_sexpr and legacy plugins (the only ones needed for DRC).
 * This avoids pulling in all the other I/O plugin headers and their complex deps.
 */

#include <wx/filename.h>
#include <wx/uri.h>

#include <config.h>
#include <kiway_player.h>
#include <wildcards_and_files_ext.h>
#include <libraries/library_table.h>
#include <pcb_io/pcb_io_mgr.h>

// Only include the plugins we actually need
#include <pcb_io/kicad_sexpr/pcb_io_kicad_sexpr.h>
#include <pcb_io/kicad_legacy/pcb_io_kicad_legacy.h>
#include <reporter.h>
#include <libraries/library_table_parser.h>


#define FMT_UNIMPLEMENTED   _( "Plugin '%s' does not implement the '%s' function." )
#define FMT_NOTFOUND        _( "Plugin type '%s' is not found." )


PCB_IO* PCB_IO_MGR::FindPlugin( PCB_FILE_T aFileType )
{
    return PLUGIN_REGISTRY::Instance()->Create( aFileType );
}


const wxString PCB_IO_MGR::ShowType( PCB_FILE_T aType )
{
    if( aType == PCB_IO_MGR::NESTED_TABLE )
        return LIBRARY_TABLE_ROW::TABLE_TYPE_NAME;

    const auto& plugins = PLUGIN_REGISTRY::Instance()->AllPlugins();

    for( const auto& plugin : plugins )
    {
        if ( plugin.m_type == aType )
            return plugin.m_name;
    }

    return wxString::Format( _( "UNKNOWN (%d)" ), aType );
}


PCB_IO_MGR::PCB_FILE_T PCB_IO_MGR::EnumFromStr( const wxString& aType )
{
    if( aType == LIBRARY_TABLE_ROW::TABLE_TYPE_NAME )
        return PCB_IO_MGR::NESTED_TABLE;

    const auto& plugins = PLUGIN_REGISTRY::Instance()->AllPlugins();

    for( const auto& plugin : plugins )
    {
        if( plugin.m_name.CmpNoCase( aType ) == 0 )
            return plugin.m_type;
    }

    return PCB_IO_MGR::PCB_FILE_UNKNOWN;
}


PCB_IO_MGR::PCB_FILE_T PCB_IO_MGR::FindPluginTypeFromBoardPath( const wxString& aFileName, int aCtl )
{
    const auto& plugins = PCB_IO_MGR::PLUGIN_REGISTRY::Instance()->AllPlugins();

    for( const auto& plugin : plugins )
    {
        bool isKiCad = plugin.m_type == PCB_IO_MGR::KICAD_SEXP || plugin.m_type == PCB_IO_MGR::LEGACY;

        if( ( aCtl & KICTL_KICAD_ONLY ) && !isKiCad )
            continue;

        if( ( aCtl & KICTL_NONKICAD_ONLY ) && isKiCad )
            continue;

        IO_RELEASER<PCB_IO> pi( plugin.m_createFunc() );

        if( pi->CanReadBoard( aFileName ) )
            return plugin.m_type;
    }

    return PCB_IO_MGR::FILE_TYPE_NONE;
}


PCB_IO_MGR::PCB_FILE_T PCB_IO_MGR::GuessPluginTypeFromLibPath( const wxString& aLibPath, int aCtl )
{
    LIBRARY_TABLE_PARSER parser;

    if( parser.Parse( aLibPath.ToStdString() ).has_value() )
        return NESTED_TABLE;

    const auto& plugins = PCB_IO_MGR::PLUGIN_REGISTRY::Instance()->AllPlugins();

    for( const auto& plugin : plugins )
    {
        bool isKiCad = plugin.m_type == PCB_IO_MGR::KICAD_SEXP || plugin.m_type == PCB_IO_MGR::LEGACY;

        if( ( aCtl & KICTL_KICAD_ONLY ) && !isKiCad )
            continue;

        if( ( aCtl & KICTL_NONKICAD_ONLY ) && isKiCad )
            continue;

        IO_RELEASER<PCB_IO> pi( plugin.m_createFunc() );

        if( pi->CanReadLibrary( aLibPath ) )
            return plugin.m_type;
    }

    return PCB_IO_MGR::FILE_TYPE_NONE;
}


BOARD* PCB_IO_MGR::Load( PCB_FILE_T aFileType, const wxString& aFileName, BOARD* aAppendToMe,
                     const std::map<std::string, UTF8>* aProperties, PROJECT* aProject,
                     PROGRESS_REPORTER* aProgressReporter )
{
    IO_RELEASER<PCB_IO> pi( FindPlugin( aFileType ) );

    if( pi )
    {
        pi->SetProgressReporter( aProgressReporter );
        return pi->LoadBoard( aFileName, aAppendToMe, aProperties, aProject );
    }

    THROW_IO_ERROR( wxString::Format( FMT_NOTFOUND, ShowType( aFileType ).GetData() ) );
}


void PCB_IO_MGR::Save( PCB_FILE_T aFileType, const wxString& aFileName, BOARD* aBoard,
                   const std::map<std::string, UTF8>* aProperties )
{
    IO_RELEASER<PCB_IO> pi( FindPlugin( aFileType ) );

    if( pi )
    {
        pi->SaveBoard( aFileName, aBoard, aProperties );
        return;
    }

    THROW_IO_ERROR( wxString::Format( FMT_NOTFOUND, ShowType( aFileType ).GetData() ) );
}


bool PCB_IO_MGR::ConvertLibrary( const std::map<std::string, UTF8>& aOldFileProps,
                                 const wxString& aOldFilePath, const wxString& aNewFilePath,
                                 REPORTER* aReporter )
{
    // Not needed for WASM DRC
    return false;
}


// Only register KiCad native plugins
static PCB_IO_MGR::REGISTER_PLUGIN registerKicadPlugin(
        PCB_IO_MGR::KICAD_SEXP,
        wxT( "KiCad" ),
        []() -> PCB_IO* { return new PCB_IO_KICAD_SEXPR; } );

static PCB_IO_MGR::REGISTER_PLUGIN registerLegacyPlugin(
        PCB_IO_MGR::LEGACY,
        wxT( "Legacy" ),
        []() -> PCB_IO* { return new PCB_IO_KICAD_LEGACY; } );
