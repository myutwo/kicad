/*
 * This program source code file is part of KICAD, a free EDA CAD application.
 *
 * Copyright (C) 2011 SoftPLC Corporation, Dick Hollenbeck <dick@softplc.com>
 * Copyright (C) 2010 Kicad Developers, see change_log.txt for contributors.
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

#include <wx/wx.h>          // _()

#include <sch_part.h>
#include <sch_sweet_parser.h>
#include <sch_lpid.h>
#include <sch_lib_table.h>
#include <macros.h>
//#include <richio.h>

using namespace SCH;


PART::PART( LIB* aOwner, const STRING& aPartNameAndRev ) :
    owner( aOwner ),
    contains( 0 ),
    partNameAndRev( aPartNameAndRev ),
    extends( 0 ),
    base( 0 )
/*
    reference(  this, wxT( "reference " ) ),
    value(      this, wxT( "value" ) ),
    footprint(  this, wxT( "footprint" ) ),
    model(      this, wxT( "model" ) ),
    datasheet(  this, wxT( "datasheet" ) )
*/
{
    // Our goal is to have class LIB only instantiate what is needed, so print here
    // what it is doing. It is the only class where PART can be instantiated.
    D(printf("PART::PART(%s)\n", aPartNameAndRev.c_str() );)

    for( int i=REFERENCE;  i<END;  ++i )
        mandatory[i] = 0;
}


void PART::clear()
{
    if( extends )
    {
        delete extends;
        extends = 0;
    }

    // delete graphics I own, since their container will not destroy them:
    for( GRAPHICS::iterator it = graphics.begin();  it != graphics.end();  ++it )
        delete *it;
    graphics.clear();

    // delete PINs I own, since their container will not destroy them.
    for( PINS::iterator it = pins.begin();  it != pins.end();  ++it )
        delete *it;
    pins.clear();

    // delete properties I own, since their container will not destroy them:
    for( PROPERTIES::iterator it = properties.begin();  it != properties.end();  ++it )
        delete *it;
    properties.clear();

    keywords.clear();

    contains = 0;

    // clear the mandatory fields
    for( int ndx = REFERENCE;  ndx < END;  ++ndx )
    {
        delete mandatory[ndx];
        mandatory[ndx] = 0;
    }
}


PROPERTY* PART::FieldLookup( PROP_ID aPropertyId )
{
    wxASSERT( unsigned(aPropertyId) < unsigned(END) );

    PROPERTY* p = mandatory[aPropertyId];

    if( !p )
    {
        switch( aPropertyId )
        {
        case REFERENCE:
            p = new PROPERTY( this, wxT( "reference" ) );
            p->text = wxT( "U?" );
            break;

        case VALUE:
            p = new PROPERTY( this, wxT( "value" ) );
            break;

        case FOOTPRINT:
            p = new PROPERTY(  this, wxT( "footprint" ) );
            break;

        case DATASHEET:
            p = new PROPERTY( this, wxT( "datasheet" ) );
            break;

        case MODEL:
            p = new PROPERTY( this, wxT( "model" ) );
            break;

        default:
            ;
        }

        mandatory[aPropertyId] = p;
    }

    return p;
}


PART::~PART()
{
    clear();
}


void PART::setExtends( LPID* aLPID )
{
    delete extends;
    extends = aLPID;
}


void PART::inherit( const PART& other )
{
    contains = other.contains;

    // @todo copy the inherited drawables, properties, and pins here
}


PART& PART::operator=( const PART& other )
{
    owner          = other.owner;
    partNameAndRev = other.partNameAndRev;
    body           = other.body;
    base           = other.base;

    setExtends( other.extends ? new LPID( *other.extends ) : 0 );

    // maintain in concert with inherit(), which is a partial assignment operator.
    inherit( other );

    return *this;
}


void PART::Parse( SWEET_PARSER* aParser , LIB_TABLE* aTable ) throw( IO_ERROR, PARSE_ERROR )
{
    aParser->Parse( this, aTable );
}


void PART::PropertyDelete( const wxString& aPropertyName ) throw( IO_ERROR )
{
    PROPERTIES::iterator it = propertyFind( aPropertyName );
    if( it == properties.end() )
    {
        wxString    msg;
        msg.Printf( _( "Unable to find property: %s" ), aPropertyName.GetData() );
        THROW_IO_ERROR( msg );
    }

    delete *it;
    properties.erase( it );
    return;
}


PROPERTIES::iterator PART::propertyFind( const wxString& aPropertyName )
{
    PROPERTIES::iterator it;
    for( it = properties.begin();  it!=properties.end();  ++it )
        if( (*it)->name == aPropertyName )
            break;
    return it;
}


void PART::Format( OUTPUTFORMATTER* out, int indent, int ctl ) const
    throw( IO_ERROR )
{
    out->Print( indent, "(part %s", partNameAndRev.c_str() );

    if( extends )
        out->Print( 0, " inherits %s", extends->Format().c_str() );

    out->Print( 0, "\n" );

    for( int i = REFERENCE;  i < END;  ++i )
    {
        PROPERTY* prop = Field( PROP_ID( i ) );
        if( prop )
            prop->Format( out, indent+1, ctl );
    }

    for( PROPERTIES::const_iterator it = properties.begin();  it != properties.end();  ++it )
    {
        (*it)->Format( out, indent+1, ctl );
    }

    if( anchor.x || anchor.y )
    {
        out->Print( indent+1, "(anchor (at %.6g %.6g))\n",
            InternalToLogical( anchor.x ),
            InternalToLogical( anchor.y ) );
    }

    if( keywords.size() )
    {
        out->Print( indent+1, "(keywords" );
        for( KEYWORDS::iterator it = keywords.begin();  it != keywords.end();  ++it )
            out->Print( 0, " %s", out->Quotew( *it ).c_str() );
        out->Print( 0, ")\n" );
    }

    for( GRAPHICS::const_iterator it = graphics.begin();  it != graphics.end();  ++it )
    {
        (*it)->Format( out, indent+1, ctl );
    }

    for( PINS::const_iterator it = pins.begin();  it != pins.end();  ++it )
    {
        (*it)->Format( out, indent+1, ctl );
    }

    out->Print( indent, ")\n" );
}


//-----< PART objects  >------------------------------------------------------


void PROPERTY::Format( OUTPUTFORMATTER* out, int indent, int ctl ) const
    throw( IO_ERROR )
{
    wxASSERT( owner );      // all PROPERTYs should have an owner.

    int i;
    for( i = PART::REFERENCE;  i < PART::END;  ++i )
    {
        if( owner->Field( PART::PROP_ID(i) ) == this )
            break;
    }

    if( i < PART::END )   // is a field not a property
        out->Print( indent, "(%s", TO_UTF8( name ) );
    else
        out->Print( indent, "(property %s", out->Quotew( name ).c_str() );

    if( effects )
    {
        out->Print( 0, " %s\n", out->Quotew( text ).c_str() );
        effects->Format( out, indent+1, ctl | CTL_OMIT_NL );
        out->Print( 0, ")\n" );
    }
    else
    {
        out->Print( 0, " %s)\n", out->Quotew( text ).c_str() );
    }
}


TEXT_EFFECTS* PROPERTY::EffectsLookup()
{
    if( !effects )
    {
        effects = new TEXT_EFFECTS();
    }

    return effects;
}


void TEXT_EFFECTS::Format( OUTPUTFORMATTER* out, int indent, int ctl ) const
    throw( IO_ERROR )
{
    if( propName.IsEmpty() )
        out->Print( indent, "(effects " );
    else
        out->Print( indent, "(effects %s ",  out->Quotew( propName ).c_str() );

    out->Print( 0, "(at %.6g %.6g", InternalToLogical( pos.x ), InternalToLogical( pos.y ) );

    if( angle )
        out->Print( 0, " %.6g)", double( angle ) );
    else
        out->Print( 0, ")" );

    font.Format( out, 0, ctl | CTL_OMIT_NL );

    out->Print( 0, "(visible %s))%s",
            isVisible ? "yes" : "no",
            ctl & CTL_OMIT_NL ? "" : "\n" );
}


void FONT::Format( OUTPUTFORMATTER* out, int indent, int ctl ) const
    throw( IO_ERROR )
{
    if( name.IsEmpty() )
        out->Print( indent, "(font " );
    else
        out->Print( indent, "(font %s ", out->Quotew( name ).c_str() );

    out->Print( 0, "(size %.6g %.6g)",
            InternalToLogical( size.GetHeight() ),
            InternalToLogical( size.GetWidth() ) );

    if( italic )
        out->Print( 0, " italic" );

    if( bold )
        out->Print( 0, " bold" );

    out->Print( 0, ")%s", (ctl & CTL_OMIT_NL) ? "" : "\n" );
}


void PIN::Format( OUTPUTFORMATTER* out, int indent, int ctl ) const
    throw( IO_ERROR )
{
    out->Print( indent, "(pin %s %s ", ShowType(), ShowShape() );

    if( angle )
        out->Print( 0, "(at %.6g %.6g %.6g)", InternalToLogical( pos.x ), InternalToLogical( pos.y ), double(angle) );
    else
        out->Print( 0, "(at %.6g %.6g)", InternalToLogical( pos.x ), InternalToLogical( pos.y ) );

    out->Print( 0, "(length %.6g)", InternalToLogical( length ) );

    out->Print( 0, "(visible %s)\n", isVisible ? "yes" : "no" );

    signal.Format(  out, "signal",  indent+1, 0 );
    padname.Format( out, "padname", indent+1, CTL_OMIT_NL );
    out->Print( 0, ")\n" );
}


void PINTEXT::Format( OUTPUTFORMATTER* out, const char* aElement, int indent, int ctl ) const
    throw( IO_ERROR )
{
    out->Print( indent, "(%s %s ", aElement, out->Quotew( text ).c_str() );
    font.Format( out, 0, CTL_OMIT_NL );
    out->Print( 0, "(visible %s))%s",
        isVisible ? "yes" : "no",
        ctl & CTL_OMIT_NL ? "" : "\n" );
}


void POLY_LINE::Format( OUTPUTFORMATTER* out, int indent, int ctl ) const
    throw( IO_ERROR )
{
    out->Print( indent, "(%s ",  pts.size() == 2 ? "line" : "polyline" );
    formatContents( out, indent, ctl );
}


void POLY_LINE::formatContents(  OUTPUTFORMATTER* out, int indent, int ctl ) const
    throw( IO_ERROR )
{
    out->Print( 0, "(line_width %.6g)", lineWidth ); // @todo use logical units?

    if( fillType != PR::T_none )
        out->Print( 0, "(fill %s)", ShowFill( fillType ) );

    out->Print( 0, "\n" );

    if( pts.size() )
    {
        const int   maxLength = 75;
        int         len = 10;

        len += out->Print( indent+1, "(pts " );

        for( POINTS::const_iterator it = pts.begin();  it != pts.end();  ++it )
        {
            if( len > maxLength )
            {
                len = 10;
                out->Print( 0, "\n" );
                out->Print( indent+2, "(xy %.6g %.6g)",
                    InternalToLogical( it->x ), InternalToLogical( it->y ) );
            }
            else
                out->Print( 0, "(xy %.6g %.6g)",
                    InternalToLogical( it->x ), InternalToLogical( it->y ) );
        }

        out->Print( 0, ")" );
    }

    out->Print( 0, ")\n" );
}


void BEZIER::Format( OUTPUTFORMATTER* out, int indent, int ctl ) const
    throw( IO_ERROR )
{
    out->Print( indent, "(bezier " );
    formatContents( out, indent, ctl );     // inherited from POLY_LINE
}


void RECTANGLE::Format( OUTPUTFORMATTER* out, int indent, int ctl ) const
    throw( IO_ERROR )
{
    // (rectangle (start X Y) (end X Y) (line_width WIDTH) (fill FILL_TYPE))

    out->Print( indent, "(rectangle (start %.6g %.6g)(end %.6g %.6g)(line_width %.6g)",
        InternalToLogical( start.x ), InternalToLogical( start.y ),
        InternalToLogical( end.x ), InternalToLogical( end.y ),
        lineWidth );

    if( fillType != PR::T_none )
        out->Print( 0, "(fill %s)", ShowFill( fillType ) );

    out->Print( 0, ")\n" );
}


void CIRCLE::Format( OUTPUTFORMATTER* out, int indent, int ctl ) const
    throw( IO_ERROR )
{
    /*
        (circle (center X Y)(radius LENGTH)(line_width WIDTH)(fill FILL_TYPE))
    */

    out->Print( indent, "(circle (center %.6g %.6g)(radius %.6g)(line_width %.6g)",
        InternalToLogical( center.x ), InternalToLogical( center.y ),
        InternalToLogical( radius),
        lineWidth );

    if( fillType != PR::T_none )
        out->Print( 0, "(fill %s)", ShowFill( fillType ) );

    out->Print( 0, ")\n" );
}


void ARC::Format( OUTPUTFORMATTER* out, int indent, int ctl ) const
    throw( IO_ERROR )
{
    /*
        (arc (pos X Y)(radius RADIUS)(start X Y)(end X Y)(line_width WIDTH)(fill FILL_TYPE))
    */

    out->Print( indent, "(arc (pos %.6g %.6g)(radius %.6g)(start %.6g %.6g)(end %.6g %.6g)(line_width %.6g)",
        InternalToLogical( pos.x ), InternalToLogical( pos.y ),
        InternalToLogical( radius),
        InternalToLogical( start.x ), InternalToLogical( start.y ),
        InternalToLogical( end.x ),   InternalToLogical( end.y ),
        lineWidth );

    if( fillType != PR::T_none )
        out->Print( 0, "(fill %s)", ShowFill( fillType ) );

    out->Print( 0, ")\n" );
}


void GR_TEXT::Format( OUTPUTFORMATTER* out, int indent, int ctl ) const
    throw( IO_ERROR )
{
    /*
        (text "This is the text that gets drawn."
            (at X Y [ANGLE])

            # Valid horizontal justification values are center, right, and left.  Valid
            # vertical justification values are center, top, bottom.
            (justify HORIZONTAL_JUSTIFY VERTICAL_JUSTIFY)
            (font [FONT] (size HEIGHT WIDTH) [italic] [bold])
            (visible YES)
            (fill FILL_TYPE)
        )
    */

    out->Print( indent, "(text %s\n", out->Quotew( text ).c_str() );

    if( angle )
        out->Print( indent+1, "(at %.6g %.6g %.6g)",  InternalToLogical( pos.x ), InternalToLogical( pos.y ), double( angle ) );
    else
        out->Print( indent+1, "(at %.6g %.6g)",  InternalToLogical( pos.x ), InternalToLogical( pos.y ) );

    out->Print( 0, "(justify %s %s)(visible %s)",
            ShowJustify( hjustify ), ShowJustify( vjustify ),
            isVisible ? "yes" : "no" );

    if( fillType != PR::T_none )
        out->Print( 0, "(fill %s)", ShowFill( fillType ) );

    out->Print( 0, "\n" );
    font.Format( out, indent+1, CTL_OMIT_NL );
    out->Print( 0, ")\n" );
}

