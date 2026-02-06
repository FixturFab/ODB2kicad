/**
 * Implementations for wx stub globals that need to be defined somewhere.
 */

#include <wx/strconv.h>
#include <wx/gdicmn.h>

// Global converter instances
wxMBConvUTF8 wxConvUTF8;
wxCSConv wxConvLocal( wxString("UTF-8") );
wxMBConv wxConvLibc;

// Default position and size
const wxPoint wxDefaultPosition(-1, -1);
const wxSize wxDefaultSize(-1, -1);
