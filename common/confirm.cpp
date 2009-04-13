/*************************/
/* Menu " CONFIRMATION " */
/* fonction Get_Message	 */
/* test demande ESC 	 */
/*************************/

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

// for all others, include the necessary headers (this file is usually all you
// need because it includes almost all "standard" wxWindows headers
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include "fctsys.h"
#include "common.h"

enum id_dialog {
    ID_TIMOUT = 1500
};

/* Classe d'affichage de messages, identique a wxMessageDialog,
 *  mais pouvant etre effacee au bout d'un time out donne
 */
class WinEDA_MessageDialog : public wxMessageDialog
{
private:
    int     m_LifeTime;
    wxTimer m_Timer;

public:
    WinEDA_MessageDialog( wxWindow * parent, const wxString &msg,
                          const wxString &title, int style, int lifetime );
    ~WinEDA_MessageDialog() { };

    void OnTimeOut( wxTimerEvent& event );

    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE( WinEDA_MessageDialog, wxMessageDialog )
EVT_TIMER( ID_TIMOUT, WinEDA_MessageDialog::OnTimeOut )
END_EVENT_TABLE()

/**********************************************************************************/
WinEDA_MessageDialog::WinEDA_MessageDialog( wxWindow* parent, const wxString& msg,
                                            const wxString& title, int style, int lifetime ) :
    wxMessageDialog( parent, msg, title, style )
/**********************************************************************************/
{
    m_LifeTime = lifetime;
    m_Timer.SetOwner( this, ID_TIMOUT );
    if( m_LifeTime > 0 )
        m_Timer.Start( 100 * m_LifeTime, wxTIMER_ONE_SHOT ); // m_LifeTime = duree en 0.1 secondes
}


/********************************************************/
void WinEDA_MessageDialog::OnTimeOut( wxTimerEvent& event )
/********************************************************/
{
    m_Timer.Stop();
    EndModal( wxID_YES );   // Does not work, I do not know why (this function is correctly called after time out)
}


/*****************************************************************************/
void DisplayError( wxWindow* parent, const wxString& text, int displaytime )
/*****************************************************************************/

/*  Affiche un Message d'Erreur ou d'avertissement.
 *  si warn > 0 le dialogue disparait apres displaytime * 0.1 secondes
 */
{
    wxMessageDialog* dialog;

    if( displaytime > 0 )
        dialog = new    WinEDA_MessageDialog( parent, text, _( "Warning" ),
                                              wxOK | wxICON_INFORMATION,
                                              displaytime );
    else
        dialog = new    WinEDA_MessageDialog( parent, text, _( "Error" ),
                                              wxOK | wxICON_ERROR, 0 );

    dialog->ShowModal();
    dialog->Destroy();
}


/**************************************************************************/
void DisplayInfo( wxWindow* parent, const wxString& text, int displaytime )
/**************************************************************************/

/* Affiche un Message d'information.
 */

{
    wxMessageDialog* dialog;

    dialog = new WinEDA_MessageDialog( parent, text, _( "Info:" ),
                                       wxOK | wxICON_INFORMATION, displaytime );

    dialog->ShowModal();
    dialog->Destroy();
}


/**************************************************/
bool IsOK( wxWindow* parent, const wxString& text )
/**************************************************/
{
    int ii;

    ii = wxMessageBox( text, _( "Confirmation" ), wxYES_NO | wxCENTRE | wxICON_HAND, parent );
    if( ii == wxYES )
        return TRUE;
    return FALSE;
}


/***********************************************************************/
int Get_Message( const wxString& title,                // The question
                 const wxString& frame_caption,        // The frame caption
                 wxString& buffer,                     // String input/return buffer
                 wxWindow* frame )
/***********************************************************************/

/* Get a text from user
 *  titre = titre a afficher
 *  buffer : text enter by user
 *  leading and trailing spaces are removed
 *      if buffer != "" buffer is displayed
 *      return:
 *          0 if OK
 *          != 0 if ESCAPE
 */
{
    wxString message;

    message = wxGetTextFromUser( title, frame_caption,
                                 buffer, frame );
    if( !message.IsEmpty() )
    {
        message.Trim( FALSE );      // Remove blanks at beginning
        message.Trim( TRUE );       // Remove blanks at end
        buffer = message;
        return 0;
    }

    return 1;
}
