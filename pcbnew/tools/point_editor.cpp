/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2013 CERN
 * @author Maciej Suminski <maciej.suminski@cern.ch>
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

#include <boost/make_shared.hpp>

#include <tool/tool_manager.h>
#include <view/view_controls.h>
#include <gal/graphics_abstraction_layer.h>
#include <confirm.h>

#include "common_actions.h"
#include "selection_tool.h"
#include "point_editor.h"

#include <wxPcbStruct.h>
#include <class_drawsegment.h>
#include <class_zone.h>

/**
 * Class POINT_EDITOR
 *
 * Tool that displays edit points allowing to modify items by dragging the points.
 */

class EDIT_POINTS_FACTORY
{
public:
    static boost::shared_ptr<EDIT_POINTS> Make( EDA_ITEM* aItem )
    {
        boost::shared_ptr<EDIT_POINTS> points = boost::make_shared<EDIT_POINTS>( aItem );

        // Generate list of edit points basing on the item type
        switch( aItem->Type() )
        {
            case PCB_LINE_T:
            {
                const DRAWSEGMENT* segment = static_cast<const DRAWSEGMENT*>( aItem );

                switch( segment->GetShape() )
                {
                case S_SEGMENT:
                    points->AddPoint( segment->GetStart() );
                    points->AddPoint( segment->GetEnd() );

                    break;

                case S_ARC:
                    points->AddPoint( segment->GetCenter() );         // points[0]
                    points->AddPoint( segment->GetArcStart() );       // points[1]
                    points->AddPoint( segment->GetArcEnd() );         // points[2]

                    // Set constraints
                    // Arc end has to stay at the same radius as the start
                    (*points)[2].SetConstraint( new EPC_CIRCLE( (*points)[2], (*points)[0], (*points)[1] ) );
                    break;

                case S_CIRCLE:
                    points->AddPoint( segment->GetCenter() );
                    points->AddPoint( segment->GetEnd() );
                    break;

                default:        // suppress warnings
                    break;
                }

                break;
            }

            case PCB_ZONE_AREA_T:
            {
                const CPolyLine* outline = static_cast<const ZONE_CONTAINER*>( aItem )->Outline();
                int cornersCount = outline->GetCornersCount();

                for( int i = 0; i < cornersCount; ++i )
                    points->AddPoint( outline->GetPos( i ) );

                // Lines have to be added after creating edit points, so they use EDIT_POINT references
                for( int i = 0; i < cornersCount - 1; ++i )
                    points->AddLine( (*points)[i], (*points)[i + 1] );

                // The one missing line
                points->AddLine( (*points)[cornersCount - 1], (*points)[0] );

                break;
            }

            default:
                points.reset();
                break;
        }

        return points;
    }

private:
    EDIT_POINTS_FACTORY() {};
};


POINT_EDITOR::POINT_EDITOR() :
    TOOL_INTERACTIVE( "pcbnew.PointEditor" ), m_selectionTool( NULL )
{
}


void POINT_EDITOR::Reset( RESET_REASON aReason )
{
    m_editPoints.reset();
}


bool POINT_EDITOR::Init()
{
    // Find the selection tool, so they can cooperate
    m_selectionTool = static_cast<SELECTION_TOOL*>( m_toolMgr->FindTool( "pcbnew.InteractiveSelection" ) );

    if( !m_selectionTool )
    {
        DisplayError( NULL, wxT( "pcbnew.InteractiveSelection tool is not available" ) );
        return false;
    }

    setTransitions();

    return true;
}


int POINT_EDITOR::OnSelectionChange( TOOL_EVENT& aEvent )
{
    const SELECTION_TOOL::SELECTION& selection = m_selectionTool->GetSelection();

    if( selection.Size() == 1 )
    {
        Activate();

        KIGFX::VIEW_CONTROLS* controls = getViewControls();
        KIGFX::VIEW* view = getView();
        PCB_EDIT_FRAME* editFrame = getEditFrame<PCB_EDIT_FRAME>();
        EDA_ITEM* item = selection.items.GetPickedItem( 0 );
        EDIT_POINT constrainer( VECTOR2I( 0, 0 ) );

        m_editPoints = EDIT_POINTS_FACTORY::Make( item );
        if( !m_editPoints )
        {
            setTransitions();
            return 0;
        }

        view->Add( m_editPoints.get() );
        m_dragPoint = NULL;
        bool modified = false;

        // Main loop: keep receiving events
        while( OPT_TOOL_EVENT evt = Wait() )
        {
            if( !m_editPoints ||
                evt->Matches( m_selectionTool->ClearedEvent ) ||
                evt->Matches( m_selectionTool->DeselectedEvent ) ||
                evt->Matches( m_selectionTool->SelectedEvent ) )
            {
                break;
            }

            if( evt->IsMotion() )
            {
                EDIT_POINT* point = m_editPoints->FindPoint( evt->Position() );

                if( m_dragPoint != point )
                {
                    if( point )
                    {
                        controls->ShowCursor( true );
                        controls->SetAutoPan( true );
                        controls->SetSnapping( true );
                    }
                    else
                    {
                        controls->ShowCursor( false );
                        controls->SetAutoPan( false );
                        controls->SetSnapping( false );
                    }
                }

                m_dragPoint = point;
            }

            else if( evt->IsDrag( BUT_LEFT ) && m_dragPoint )
            {
                if( !modified )
                {
                    // Save items, so changes can be undone
                    editFrame->OnModify();
                    editFrame->SaveCopyInUndoList( selection.items, UR_CHANGED );
                    modified = true;
                }

                if( evt->Modifier( MD_CTRL ) )      // 45 degrees mode
                {
                    if( !m_dragPoint->IsConstrained() )
                    {
                        // Find a proper constraining point for 45 degrees mode
                        constrainer = get45DegConstrainer();
                        m_dragPoint->SetConstraint( new EPC_45DEGREE( *m_dragPoint, constrainer ) );
                    }
                }
                else
                {
                    m_dragPoint->ClearConstraint();
                }

                m_dragPoint->SetPosition( controls->GetCursorPosition() );
                m_dragPoint->ApplyConstraint();
                updateItem();
                updatePoints();

                m_editPoints->ViewUpdate( KIGFX::VIEW_ITEM::GEOMETRY );
            }

            else if( evt->IsAction( &COMMON_ACTIONS::pointEditorUpdate ) )
            {
                updatePoints();
            }

            else if( evt->IsMouseUp( BUT_LEFT ) )
            {
                modified = false;
            }

            else if( evt->IsCancel() )
            {
                if( modified )      // Restore the last change
                {
                    wxCommandEvent dummy;
                    editFrame->GetBoardFromUndoList( dummy );

                    updatePoints();
                    modified = false;
                }

                break;
            }

            else
            {
                m_toolMgr->PassEvent();
            }
        }

        if( m_editPoints )
        {
            finishItem();
            item->ViewUpdate( KIGFX::VIEW_ITEM::GEOMETRY );
            view->Remove( m_editPoints.get() );
            m_editPoints.reset();
        }

        controls->ShowCursor( false );
        controls->SetAutoPan( false );
        controls->SetSnapping( false );
    }

    setTransitions();

    return 0;
}


void POINT_EDITOR::updateItem() const
{
    EDA_ITEM* item = m_editPoints->GetParent();

    switch( item->Type() )
    {
    case PCB_LINE_T:
    {
        DRAWSEGMENT* segment = static_cast<DRAWSEGMENT*>( item );
        switch( segment->GetShape() )
        {
        case S_SEGMENT:
            if( &(*m_editPoints)[0] == m_dragPoint )
                segment->SetStart( wxPoint( (*m_editPoints)[0].GetPosition().x,
                                            (*m_editPoints)[0].GetPosition().y ) );

            else if( &(*m_editPoints)[1] == m_dragPoint )
                segment->SetEnd( wxPoint( (*m_editPoints)[1].GetPosition().x,
                                          (*m_editPoints)[1].GetPosition().y ) );

            break;

        case S_ARC:
        {
            const VECTOR2I& center = (*m_editPoints)[0].GetPosition();
            const VECTOR2I& start = (*m_editPoints)[1].GetPosition();
            const VECTOR2I& end = (*m_editPoints)[2].GetPosition();

            if( center != segment->GetCenter() )
            {
                wxPoint moveVector = wxPoint( center.x, center.y ) - segment->GetCenter();
                segment->Move( moveVector );

                (*m_editPoints)[1].SetPosition( segment->GetArcStart() );
                (*m_editPoints)[2].SetPosition( segment->GetArcEnd() );
            }

            else
            {
                segment->SetArcStart( wxPoint( start.x, start.y ) );

                VECTOR2D startLine = start - center;
                VECTOR2I endLine = end - center;
                double newAngle = RAD2DECIDEG( endLine.Angle() - startLine.Angle() );

                // Adjust the new angle to (counter)clockwise setting
                bool clockwise = ( segment->GetAngle() > 0 );

                if( clockwise && newAngle < 0.0 )
                    newAngle += 3600.0;
                else if( !clockwise && newAngle > 0.0 )
                    newAngle -= 3600.0;

                segment->SetAngle( newAngle );
            }

            break;
        }

        case S_CIRCLE:
        {
            const VECTOR2I& center = (*m_editPoints)[0].GetPosition();
            const VECTOR2I& end = (*m_editPoints)[1].GetPosition();

            if( m_dragPoint == &(*m_editPoints)[0] )
            {
                wxPoint moveVector = wxPoint( center.x, center.y ) - segment->GetCenter();
                segment->Move( moveVector );
            }
            else
            {
                segment->SetEnd( wxPoint( end.x, end.y ) );
            }

            break;
        }

        default:        // suppress warnings
            break;
        }

        break;
    }

    case PCB_ZONE_AREA_T:
    {
        ZONE_CONTAINER* zone = static_cast<ZONE_CONTAINER*>( item );
        CPolyLine* outline = zone->Outline();

        for( int i = 0; i < outline->GetCornersCount(); ++i )
        {
            outline->SetX( i, (*m_editPoints)[i].GetPosition().x );
            outline->SetY( i, (*m_editPoints)[i].GetPosition().y );
        }

        break;
    }

    default:
        break;
    }
}


void POINT_EDITOR::finishItem() const
{
    EDA_ITEM* item = m_editPoints->GetParent();

    if( item->Type() == PCB_ZONE_AREA_T )
    {
        ZONE_CONTAINER* zone = static_cast<ZONE_CONTAINER*>( item );

        if( zone->IsFilled() )
            getEditFrame<PCB_EDIT_FRAME>()->Fill_Zone( zone );
    }
}


void POINT_EDITOR::updatePoints() const
{
    EDA_ITEM* item = m_editPoints->GetParent();

    switch( item->Type() )
    {
    case PCB_LINE_T:
    {
        const DRAWSEGMENT* segment = static_cast<const DRAWSEGMENT*>( item );
        {
            switch( segment->GetShape() )
            {
            case S_SEGMENT:
                (*m_editPoints)[0].SetPosition( segment->GetStart() );
                (*m_editPoints)[1].SetPosition( segment->GetEnd() );
                break;

            case S_ARC:
                (*m_editPoints)[0].SetPosition( segment->GetCenter() );
                (*m_editPoints)[1].SetPosition( segment->GetArcStart() );
                (*m_editPoints)[2].SetPosition( segment->GetArcEnd() );
                break;

            case S_CIRCLE:
                (*m_editPoints)[0].SetPosition( segment->GetCenter() );
                (*m_editPoints)[1].SetPosition( segment->GetEnd() );
                break;

            default:        // suppress warnings
                break;
            }

            break;
        }
    }

    case PCB_ZONE_AREA_T:
    {
        const ZONE_CONTAINER* zone = static_cast<const ZONE_CONTAINER*>( item );
        const CPolyLine* outline = zone->Outline();

        for( int i = 0; i < outline->GetCornersCount(); ++i )
            (*m_editPoints)[i].SetPosition( outline->GetPos( i ) );

        break;
    }

    default:
        break;
    }
}


EDIT_POINT POINT_EDITOR::get45DegConstrainer() const
{
    EDA_ITEM* item = m_editPoints->GetParent();

    if( item->Type() == PCB_LINE_T )
    {
        const DRAWSEGMENT* segment = static_cast<const DRAWSEGMENT*>( item );
        {
            switch( segment->GetShape() )
            {
            case S_SEGMENT:
                return *( m_editPoints->Next( *m_dragPoint ) );     // select the other end of line

            case S_ARC:
            case S_CIRCLE:
                return (*m_editPoints)[0];      // center

            default:        // suppress warnings
                break;
            }
        }
    }

    // In any other case we may align item to the current cursor position.
    return EDIT_POINT( getViewControls()->GetCursorPosition() );
}
