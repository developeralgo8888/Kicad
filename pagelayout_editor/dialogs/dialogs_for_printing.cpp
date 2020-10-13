/**
 * @file dialogs_for_printing.cpp
 */

/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2013 CERN
 * @author Jean-Pierre Charras, jp.charras at wanadoo.fr
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <base_units.h>
#include <dialog_helpers.h>
#include <ws_draw_item.h>
#include <ws_data_item.h>
#include <ws_data_model.h>
#include <ws_painter.h>
#include <dialog_page_settings.h>

#include "invoke_pl_editor_dialog.h"
#include "pl_editor_frame.h"
#include "pl_editor_id.h"

/**
 * Custom print out for printing schematics.
 */
class PLEDITOR_PRINTOUT : public wxPrintout
{
private:
    PL_EDITOR_FRAME* m_parent;

public:
    PLEDITOR_PRINTOUT( PL_EDITOR_FRAME* aParent, const wxString& aTitle ) :
        wxPrintout( aTitle )
    {
        wxASSERT( aParent != NULL );
        m_parent = aParent;
    }

    bool OnPrintPage( int aPageNum ) override;
    bool HasPage( int aPageNum ) override { return ( aPageNum <= 2 ); }
    void GetPageInfo( int* minPage, int* maxPage, int* selPageFrom, int* selPageTo ) override;
    void PrintPage( int aPageNum );
};

/**
 * Custom print preview frame.
 */
class PLEDITOR_PREVIEW_FRAME : public wxPreviewFrame
{
    PL_EDITOR_FRAME* m_parent;

public:
    PLEDITOR_PREVIEW_FRAME( wxPrintPreview* aPreview, PL_EDITOR_FRAME* aParent,
                       const wxString& aTitle, const wxPoint& aPos = wxDefaultPosition,
                       const wxSize& aSize = wxDefaultSize ) :
        wxPreviewFrame( aPreview, aParent, aTitle, aPos, aSize )
    {
        m_parent = aParent;
    }

    bool Show( bool show ) override
    {
        bool        ret;

        // Show or hide the window.  If hiding, save current position and size.
        // If showing, use previous position and size.
        if( show )
        {
            bool centre = false;
            if( s_size.x == 0 || s_size.y == 0 )
            {
                s_size = (m_parent->GetSize() * 3) / 4;
                s_pos = wxDefaultPosition;
                centre = true;
            }

            SetSize( s_pos.x, s_pos.y, s_size.x, s_size.y, 0 );

            if( centre )
                Center();

            ret = wxPreviewFrame::Show( show );
        }
        else
        {
            // Save the dialog's position & size before hiding
            s_size = GetSize();
            s_pos  = GetPosition();

            ret = wxPreviewFrame::Show( show );
        }
        return ret;
    }

private:
    static wxPoint  s_pos;
    static wxSize   s_size;

    DECLARE_CLASS( PLEDITOR_PREVIEW_FRAME )
    DECLARE_EVENT_TABLE()
    DECLARE_NO_COPY_CLASS( PLEDITOR_PREVIEW_FRAME )
};

wxPoint PLEDITOR_PREVIEW_FRAME::s_pos;
wxSize  PLEDITOR_PREVIEW_FRAME::s_size;

IMPLEMENT_CLASS( PLEDITOR_PREVIEW_FRAME, wxPreviewFrame )

BEGIN_EVENT_TABLE( PLEDITOR_PREVIEW_FRAME, wxPreviewFrame )
    EVT_CLOSE( PLEDITOR_PREVIEW_FRAME::OnCloseWindow )
END_EVENT_TABLE()


bool PLEDITOR_PRINTOUT::OnPrintPage( int aPageNum )
{
    PrintPage( aPageNum );
    return true;
}


void PLEDITOR_PRINTOUT::GetPageInfo( int* minPage, int* maxPage,
                                     int* selPageFrom, int* selPageTo )
{
    *minPage = *selPageFrom = 1;
    *maxPage = *selPageTo   = 2;
}

/*
 * This is the real print function: print the active screen
 */
void PLEDITOR_PRINTOUT::PrintPage( int aPageNum )
{
    wxPoint  tmp_startvisu;
    wxSize   pageSizeIU;             // Page size in internal units
    wxPoint  old_org;
    wxRect   fitRect;
    wxDC*    dc = GetDC();
    BASE_SCREEN* screen = m_parent->GetScreen();

    // Save current offsets and clip box.
    tmp_startvisu = screen->m_StartVisu;
    old_org = screen->m_DrawOrg;

    // Change scale factor and offset to print the whole page.

    pageSizeIU =  m_parent->GetPageSettings().GetSizeIU();
    FitThisSizeToPaper( pageSizeIU );
    fitRect = GetLogicalPaperRect();

    int xoffset = ( fitRect.width - pageSizeIU.x ) / 2;
    int yoffset = ( fitRect.height - pageSizeIU.y ) / 2;

    OffsetLogicalOrigin( xoffset, yoffset );

    GRResetPenAndBrush( dc );
    GRForceBlackPen( true );

    COLOR4D bg_color = m_parent->GetDrawBgColor();
    m_parent->SetDrawBgColor( WHITE );

    screen->m_ScreenNumber = aPageNum;

    KIGFX::WS_RENDER_SETTINGS renderSettings;
    renderSettings.SetDefaultPenWidth( 1 );
    renderSettings.SetLayerColor( LAYER_WORKSHEET, COLOR4D( RED ) );
    renderSettings.SetPrintDC( dc );

    // Ensure the scaling factor (used only in printing) of bitmaps is up to date
    WS_DATA_MODEL&     model = WS_DATA_MODEL::GetTheInstance();

    for( WS_DATA_ITEM* dataItem : model.GetItems() )
    {
        if( dataItem->GetType() != WS_DATA_ITEM::WS_BITMAP )
            continue;

        WS_DATA_ITEM_BITMAP* itemBM = static_cast<WS_DATA_ITEM_BITMAP*>( dataItem );
        itemBM->m_ImageBitmap->SetPixelScaleFactor( IU_PER_MILS * 1000 /
                                                    itemBM->m_ImageBitmap->GetPPI() );
    }

    m_parent->PrintWorkSheet( &renderSettings, screen, IU_PER_MILS, wxEmptyString );

    m_parent->SetDrawBgColor( bg_color );

    GRForceBlackPen( false );

    screen->m_StartVisu = tmp_startvisu;
    screen->m_DrawOrg   = old_org;

    // PrintWorkSheet clears the current display list when calling BuildWorkSheetGraphicList()
    // So rebuild and redraw it.
    m_parent->GetCanvas()->DisplayWorksheet();
}


int InvokeDialogPrint( PL_EDITOR_FRAME* aCaller, wxPrintData* aPrintData,
                       wxPageSetupDialogData* aPageSetupData )
{
    int pageCount = 2;

    wxPrintDialogData printDialogData( *aPrintData );
    printDialogData.SetMaxPage( pageCount );

    if( pageCount > 1 )
        printDialogData.EnablePageNumbers( true );

    wxPrinter printer( &printDialogData );
    PLEDITOR_PRINTOUT printout( aCaller, _( "Print Page Layout" ) );

    if( !printer.Print( aCaller, &printout, true ) )
    {
        if( wxPrinter::GetLastError() == wxPRINTER_ERROR )
            wxMessageBox( _( "An error occurred attempting to print the page layout." ),
                          _( "Printing" ), wxOK );
        return 0;
    }

    *aPageSetupData = printer.GetPrintDialogData().GetPrintData();

    return 1;
}

int InvokeDialogPrintPreview( PL_EDITOR_FRAME* aCaller, wxPrintData* aPrintData )
{
    // Pass two printout objects: for preview, and possible printing.
    wxString        title   = _( "Preview" );
    wxPrintPreview* preview = new wxPrintPreview( new PLEDITOR_PRINTOUT( aCaller, title ),
                                                  new PLEDITOR_PRINTOUT( aCaller, title ),
                                                  aPrintData );

    preview->SetZoom( 70 );

    PLEDITOR_PREVIEW_FRAME* frame = new PLEDITOR_PREVIEW_FRAME( preview, aCaller, title );

    frame->Initialize();
    frame->Show( true );

    return 1;
}

