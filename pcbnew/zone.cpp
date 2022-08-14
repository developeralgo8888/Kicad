/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2017 Jean-Pierre Charras, jp.charras at wanadoo.fr
 * Copyright (C) 2012 SoftPLC Corporation, Dick Hollenbeck <dick@softplc.com>
 * Copyright (C) 1992-2022 KiCad Developers, see AUTHORS.txt for contributors.
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

#include <bitmaps.h>
#include <geometry/geometry_utils.h>
#include <geometry/shape_null.h>
#include <core/mirror.h>
#include <advanced_config.h>
#include <pcb_edit_frame.h>
#include <pcb_screen.h>
#include <board.h>
#include <board_design_settings.h>
#include <pad.h>
#include <zone.h>
#include <string_utils.h>
#include <math_for_graphics.h>
#include <settings/color_settings.h>
#include <settings/settings_manager.h>
#include <trigo.h>
#include <i18n_utility.h>


ZONE::ZONE( BOARD_ITEM_CONTAINER* aParent, bool aInFP ) :
        BOARD_CONNECTED_ITEM( aParent, aInFP ? PCB_FP_ZONE_T : PCB_ZONE_T ),
        m_area( 0.0 )
{
    m_CornerSelection = nullptr;                // no corner is selected
    m_isFilled = false;                         // fill status : true when the zone is filled
    m_teardropType = TEARDROP_TYPE::TD_NONE;
    m_islandRemovalMode = ISLAND_REMOVAL_MODE::ALWAYS;
    m_borderStyle = ZONE_BORDER_DISPLAY_STYLE::DIAGONAL_EDGE;
    m_borderHatchPitch = GetDefaultHatchPitch();
    m_priority = 0;
    SetIsRuleArea( aInFP );           // Zones living in footprints have the rule area option
    SetLocalFlags( 0 );               // flags temporary used in zone calculations
    m_Poly = new SHAPE_POLY_SET();    // Outlines
    m_fillVersion = 5;                // set the "old" way to build filled polygon areas (< 6.0.x)

    aParent->GetZoneSettings().ExportSetting( *this );

    m_ZoneMinThickness = Mils2iu( ZONE_THICKNESS_MIL );
    m_thermalReliefSpokeWidth = Mils2iu( ZONE_THERMAL_RELIEF_COPPER_WIDTH_MIL );
    m_thermalReliefGap = Mils2iu( ZONE_THERMAL_RELIEF_GAP_MIL );

    m_needRefill = false;   // True only after edits.
}


ZONE::ZONE( const ZONE& aZone ) :
        BOARD_CONNECTED_ITEM( aZone ),
        m_Poly( nullptr ),
        m_CornerSelection( nullptr )
{
    InitDataFromSrcInCopyCtor( aZone );
}


ZONE& ZONE::operator=( const ZONE& aOther )
{
    BOARD_CONNECTED_ITEM::operator=( aOther );

    InitDataFromSrcInCopyCtor( aOther );

    return *this;
}


ZONE::~ZONE()
{
    delete m_Poly;
    delete m_CornerSelection;
}


void ZONE::InitDataFromSrcInCopyCtor( const ZONE& aZone )
{
    // members are expected non initialize in this.
    // InitDataFromSrcInCopyCtor() is expected to be called
    // only from a copy constructor.

    // Copy only useful EDA_ITEM flags:
    m_flags                   = aZone.m_flags;
    m_forceVisible            = aZone.m_forceVisible;

    // Replace the outlines for aZone outlines.
    delete m_Poly;
    m_Poly = new SHAPE_POLY_SET( *aZone.m_Poly );

    m_cornerSmoothingType     = aZone.m_cornerSmoothingType;
    m_cornerRadius            = aZone.m_cornerRadius;
    m_zoneName                = aZone.m_zoneName;
    m_priority                = aZone.m_priority;
    m_isRuleArea              = aZone.m_isRuleArea;
    SetLayerSet( aZone.GetLayerSet() );

    m_doNotAllowCopperPour    = aZone.m_doNotAllowCopperPour;
    m_doNotAllowVias          = aZone.m_doNotAllowVias;
    m_doNotAllowTracks        = aZone.m_doNotAllowTracks;
    m_doNotAllowPads          = aZone.m_doNotAllowPads;
    m_doNotAllowFootprints    = aZone.m_doNotAllowFootprints;

    m_PadConnection           = aZone.m_PadConnection;
    m_ZoneClearance           = aZone.m_ZoneClearance;     // clearance value
    m_ZoneMinThickness        = aZone.m_ZoneMinThickness;
    m_fillVersion             = aZone.m_fillVersion;
    m_islandRemovalMode       = aZone.m_islandRemovalMode;
    m_minIslandArea           = aZone.m_minIslandArea;

    m_isFilled                = aZone.m_isFilled;
    m_needRefill              = aZone.m_needRefill;
    m_teardropType            = aZone.m_teardropType;

    m_thermalReliefGap        = aZone.m_thermalReliefGap;
    m_thermalReliefSpokeWidth = aZone.m_thermalReliefSpokeWidth;

    m_fillMode                = aZone.m_fillMode;         // solid vs. hatched
    m_hatchThickness          = aZone.m_hatchThickness;
    m_hatchGap                = aZone.m_hatchGap;
    m_hatchOrientation        = aZone.m_hatchOrientation;
    m_hatchSmoothingLevel     = aZone.m_hatchSmoothingLevel;
    m_hatchSmoothingValue     = aZone.m_hatchSmoothingValue;
    m_hatchBorderAlgorithm    = aZone.m_hatchBorderAlgorithm;
    m_hatchHoleMinArea        = aZone.m_hatchHoleMinArea;

    // For corner moving, corner index to drag, or nullptr if no selection
    delete m_CornerSelection;
    m_CornerSelection         = nullptr;

    for( PCB_LAYER_ID layer : aZone.GetLayerSet().Seq() )
    {
        std::shared_ptr<SHAPE_POLY_SET> fill = aZone.m_FilledPolysList.at( layer );

        if( fill )
            m_FilledPolysList[layer] = std::make_shared<SHAPE_POLY_SET>( *fill );
        else
            m_FilledPolysList[layer] = std::make_shared<SHAPE_POLY_SET>();

        m_filledPolysHash[layer]  = aZone.m_filledPolysHash.at( layer );
        m_insulatedIslands[layer] = aZone.m_insulatedIslands.at( layer );
    }

    m_borderStyle             = aZone.m_borderStyle;
    m_borderHatchPitch        = aZone.m_borderHatchPitch;
    m_borderHatchLines        = aZone.m_borderHatchLines;

    SetLocalFlags( aZone.GetLocalFlags() );

    m_netinfo                 = aZone.m_netinfo;
    m_area                    = aZone.m_area;
}


EDA_ITEM* ZONE::Clone() const
{
    return new ZONE( *this );
}


bool ZONE::HigherPriority( const ZONE* aOther ) const
{
    if( m_priority != aOther->m_priority )
        return m_priority > aOther->m_priority;

    return m_Uuid > aOther->m_Uuid;
}


bool ZONE::SameNet( const ZONE* aOther ) const
{
    return GetNetCode() == aOther->GetNetCode();
}


bool ZONE::UnFill()
{
    bool change = false;

    for( std::pair<const PCB_LAYER_ID, std::shared_ptr<SHAPE_POLY_SET>>& pair : m_FilledPolysList )
    {
        change |= !pair.second->IsEmpty();
        m_insulatedIslands[pair.first].clear();
        pair.second->RemoveAllContours();
    }

    m_isFilled = false;
    m_fillFlags.clear();

    return change;
}


VECTOR2I ZONE::GetPosition() const
{
    return GetCornerPosition( 0 );
}


PCB_LAYER_ID ZONE::GetLayer() const
{
    return BOARD_ITEM::GetLayer();
}


PCB_LAYER_ID ZONE::GetFirstLayer() const
{
    if( m_layerSet.size() )
        return m_layerSet.UIOrder()[0];
    else
        return UNDEFINED_LAYER;
}


bool ZONE::IsOnCopperLayer() const
{
    return ( m_layerSet & LSET::AllCuMask() ).count() > 0;
}


bool ZONE::CommonLayerExists( const LSET aLayerSet ) const
{
    LSET common = GetLayerSet() & aLayerSet;

    return common.count() > 0;
}


void ZONE::SetLayer( PCB_LAYER_ID aLayer )
{
    SetLayerSet( LSET( aLayer ) );
}


void ZONE::SetLayerSet( LSET aLayerSet )
{
    if( aLayerSet.count() == 0 )
        return;

    if( m_layerSet != aLayerSet )
    {
        SetNeedRefill( true );

        UnFill();

        m_FilledPolysList.clear();
        m_filledPolysHash.clear();
        m_insulatedIslands.clear();

        for( PCB_LAYER_ID layer : aLayerSet.Seq() )
        {
            m_FilledPolysList[layer]  = std::make_shared<SHAPE_POLY_SET>();
            m_filledPolysHash[layer]  = {};
            m_insulatedIslands[layer] = {};
        }
    }

    m_layerSet = aLayerSet;
}


LSET ZONE::GetLayerSet() const
{
    return m_layerSet;
}


void ZONE::ViewGetLayers( int aLayers[], int& aCount ) const
{
    LSEQ layers = m_layerSet.Seq();

    for( unsigned int idx = 0; idx < layers.size(); idx++ )
        aLayers[idx] = LAYER_ZONE_START + layers[idx];

    aCount = layers.size();
}


double ZONE::ViewGetLOD( int aLayer, KIGFX::VIEW* aView ) const
{
    constexpr double HIDE = std::numeric_limits<double>::max();

    return aView->IsLayerVisible( LAYER_ZONES ) ? 0.0 : HIDE;
}


bool ZONE::IsOnLayer( PCB_LAYER_ID aLayer ) const
{
    return m_layerSet.test( aLayer );
}


const EDA_RECT ZONE::GetBoundingBox() const
{
    BOX2I bb = m_Poly->BBox();

    EDA_RECT ret( bb.GetOrigin(), VECTOR2I( bb.GetWidth(), bb.GetHeight() ) );

    return ret;
}


int ZONE::GetThermalReliefGap( PAD* aPad, wxString* aSource ) const
{
    if( aPad->GetLocalThermalGapOverride() == 0 )
    {
        if( aSource )
            *aSource = _( "zone" );

        return m_thermalReliefGap;
    }

    return aPad->GetLocalThermalGapOverride( aSource );

}


void ZONE::SetCornerRadius( unsigned int aRadius )
{
    if( m_cornerRadius != aRadius )
        SetNeedRefill( true );

    m_cornerRadius = aRadius;
}


static SHAPE_POLY_SET g_nullPoly;


MD5_HASH ZONE::GetHashValue( PCB_LAYER_ID aLayer )
{
    if( !m_filledPolysHash.count( aLayer ) )
        return g_nullPoly.GetHash();
    else
        return m_filledPolysHash.at( aLayer );
}


void ZONE::BuildHashValue( PCB_LAYER_ID aLayer )
{
    if( !m_FilledPolysList.count( aLayer ) )
        m_filledPolysHash[aLayer] = g_nullPoly.GetHash();
    else
        m_filledPolysHash[aLayer] = m_FilledPolysList.at( aLayer )->GetHash();
}


bool ZONE::HitTest( const VECTOR2I& aPosition, int aAccuracy ) const
{
    // When looking for an "exact" hit aAccuracy will be 0 which works poorly for very thin
    // lines.  Give it a floor.
    int accuracy = std::max( aAccuracy, Millimeter2iu( 0.1 ) );

    return HitTestForCorner( aPosition, accuracy * 2 ) || HitTestForEdge( aPosition, accuracy );
}


bool ZONE::HitTestForCorner( const VECTOR2I& refPos, int aAccuracy,
                             SHAPE_POLY_SET::VERTEX_INDEX* aCornerHit ) const
{
    return m_Poly->CollideVertex( VECTOR2I( refPos ), aCornerHit, aAccuracy );
}


bool ZONE::HitTestForEdge( const VECTOR2I& refPos, int aAccuracy,
                           SHAPE_POLY_SET::VERTEX_INDEX* aCornerHit ) const
{
    return m_Poly->CollideEdge( VECTOR2I( refPos ), aCornerHit, aAccuracy );
}


bool ZONE::HitTest( const EDA_RECT& aRect, bool aContained, int aAccuracy ) const
{
    // Calculate bounding box for zone
    EDA_RECT bbox = GetBoundingBox();
    bbox.Normalize();

    EDA_RECT arect = aRect;
    arect.Normalize();
    arect.Inflate( aAccuracy );

    if( aContained )
    {
         return arect.Contains( bbox );
    }
    else
    {
        // Fast test: if aBox is outside the polygon bounding box, rectangles cannot intersect
        if( !arect.Intersects( bbox ) )
            return false;

        int count = m_Poly->TotalVertices();

        for( int ii = 0; ii < count; ii++ )
        {
            auto vertex = m_Poly->CVertex( ii );
            auto vertexNext = m_Poly->CVertex( ( ii + 1 ) % count );

            // Test if the point is within the rect
            if( arect.Contains( vertex ) )
                return true;

            // Test if this edge intersects the rect
            if( arect.Intersects( vertex, vertexNext ) )
                return true;
        }

        return false;
    }
}


int ZONE::GetLocalClearance( wxString* aSource ) const
{
    if( m_isRuleArea )
        return 0;

    if( aSource )
        *aSource = _( "zone" );

    return m_ZoneClearance;
}


bool ZONE::HitTestFilledArea( PCB_LAYER_ID aLayer, const VECTOR2I& aRefPos, int aAccuracy ) const
{
    // Rule areas have no filled area, but it's generally nice to treat their interior as if it were
    // filled so that people don't have to select them by their outline (which is min-width)
    if( GetIsRuleArea() )
        return m_Poly->Contains( aRefPos, -1, aAccuracy );

    if( !m_FilledPolysList.count( aLayer ) )
        return false;

    return m_FilledPolysList.at( aLayer )->Contains( aRefPos, -1, aAccuracy );
}


bool ZONE::HitTestCutout( const VECTOR2I& aRefPos, int* aOutlineIdx, int* aHoleIdx ) const
{
    // Iterate over each outline polygon in the zone and then iterate over
    // each hole it has to see if the point is in it.
    for( int i = 0; i < m_Poly->OutlineCount(); i++ )
    {
        for( int j = 0; j < m_Poly->HoleCount( i ); j++ )
        {
            if( m_Poly->Hole( i, j ).PointInside( aRefPos ) )
            {
                if( aOutlineIdx )
                    *aOutlineIdx = i;

                if( aHoleIdx )
                    *aHoleIdx = j;

                return true;
            }
        }
    }

    return false;
}


void ZONE::GetMsgPanelInfo( EDA_DRAW_FRAME* aFrame, std::vector<MSG_PANEL_ITEM>& aList )
{
    EDA_UNITS units = aFrame->GetUserUnits();
    wxString  msg;

    if( GetIsRuleArea() )
        msg = _( "Rule Area" );
    else if( IsTeardropArea() )
        msg = _( "Teardrop Area" );
    else if( IsOnCopperLayer() )
        msg = _( "Copper Zone" );
    else
        msg = _( "Non-copper Zone" );

    // Display Cutout instead of Outline for holes inside a zone (i.e. when num contour !=0).
    // Check whether the selected corner is in a hole; i.e., in any contour but the first one.
    if( m_CornerSelection != nullptr && m_CornerSelection->m_contour > 0 )
        msg << wxT( " " ) << _( "Cutout" );

    aList.emplace_back( _( "Type" ), msg );

    if( GetIsRuleArea() )
    {
        msg.Empty();

        if( GetDoNotAllowVias() )
            AccumulateDescription( msg, _( "No vias" ) );

        if( GetDoNotAllowTracks() )
            AccumulateDescription( msg, _( "No tracks" ) );

        if( GetDoNotAllowPads() )
            AccumulateDescription( msg, _( "No pads" ) );

        if( GetDoNotAllowCopperPour() )
            AccumulateDescription( msg, _( "No copper zones" ) );

        if( GetDoNotAllowFootprints() )
            AccumulateDescription( msg, _( "No footprints" ) );

        if( !msg.IsEmpty() )
            aList.emplace_back( _( "Restrictions" ), msg );
    }
    else if( IsOnCopperLayer() )
    {
        if( aFrame->GetName() == PCB_EDIT_FRAME_NAME )
        {
            aList.emplace_back( _( "Net" ), UnescapeString( GetNetname() ) );

            aList.emplace_back( _( "Resolved Netclass" ),
                                UnescapeString( GetEffectiveNetClass()->GetName() ) );
        }

        // Display priority level
        aList.emplace_back( _( "Priority" ),
                            wxString::Format( wxT( "%d" ), GetAssignedPriority() ) );
    }

    if( aFrame->GetName() == PCB_EDIT_FRAME_NAME )
    {
        if( IsLocked() )
            aList.emplace_back( _( "Status" ), _( "Locked" ) );
    }

    wxString layerDesc;
    int count = 0;

    for( PCB_LAYER_ID layer : m_layerSet.Seq() )
    {
        if( count == 0 )
            layerDesc = GetBoard()->GetLayerName( layer );

        count++;
    }

    if( count > 1 )
        layerDesc.Printf( _( "%s and %d more" ), layerDesc, count - 1 );

    aList.emplace_back( _( "Layer" ), layerDesc );

    if( !m_zoneName.empty() )
        aList.emplace_back( _( "Name" ), m_zoneName );

    switch( m_fillMode )
    {
    case ZONE_FILL_MODE::POLYGONS:      msg = _( "Solid" ); break;
    case ZONE_FILL_MODE::HATCH_PATTERN: msg = _( "Hatched" ); break;
    default:                            msg = _( "Unknown" ); break;
    }

    aList.emplace_back( _( "Fill Mode" ), msg );

    msg = MessageTextFromValue( units, m_area, true, EDA_DATA_TYPE::AREA );
    aList.emplace_back( _( "Filled Area" ), msg );

    wxString source;
    int      clearance = GetOwnClearance( UNDEFINED_LAYER, &source );

    if( !source.IsEmpty() )
    {
        aList.emplace_back( wxString::Format( _( "Min Clearance: %s" ),
                                              MessageTextFromValue( units, clearance ) ),
                            wxString::Format( _( "(from %s)" ),
                                              source ) );
    }

    if( !m_FilledPolysList.empty() )
    {
        count = 0;

        for( std::pair<const PCB_LAYER_ID, std::shared_ptr<SHAPE_POLY_SET>>& ii: m_FilledPolysList )
            count += ii.second->TotalVertices();

        aList.emplace_back( _( "Corner Count" ), wxString::Format( wxT( "%d" ), count ) );
    }
}


void ZONE::Move( const VECTOR2I& offset )
{
    /* move outlines */
    m_Poly->Move( offset );

    HatchBorder();

    for( std::pair<const PCB_LAYER_ID, std::shared_ptr<SHAPE_POLY_SET>>& pair : m_FilledPolysList )
        pair.second->Move( offset );
}


void ZONE::MoveEdge( const VECTOR2I& offset, int aEdge )
{
    int next_corner;

    if( m_Poly->GetNeighbourIndexes( aEdge, nullptr, &next_corner ) )
    {
        m_Poly->SetVertex( aEdge, m_Poly->CVertex( aEdge ) + VECTOR2I( offset ) );
        m_Poly->SetVertex( next_corner, m_Poly->CVertex( next_corner ) + VECTOR2I( offset ) );
        HatchBorder();

        SetNeedRefill( true );
    }
}


void ZONE::Rotate( const VECTOR2I& aCentre, const EDA_ANGLE& aAngle )
{
    m_Poly->Rotate( aAngle, VECTOR2I( aCentre ) );
    HatchBorder();

    /* rotate filled areas: */
    for( std::pair<const PCB_LAYER_ID, std::shared_ptr<SHAPE_POLY_SET>>& pair : m_FilledPolysList )
        pair.second->Rotate( aAngle, aCentre );
}


void ZONE::Flip( const VECTOR2I& aCentre, bool aFlipLeftRight )
{
    Mirror( aCentre, aFlipLeftRight );

    SetLayerSet( FlipLayerMask( GetLayerSet(), GetBoard()->GetCopperLayerCount() ) );
}


void ZONE::Mirror( const VECTOR2I& aMirrorRef, bool aMirrorLeftRight )
{
    // ZONEs mirror about the x-axis (why?!?)
    m_Poly->Mirror( aMirrorLeftRight, !aMirrorLeftRight, aMirrorRef );

    HatchBorder();

    for( std::pair<const PCB_LAYER_ID, std::shared_ptr<SHAPE_POLY_SET>>& pair : m_FilledPolysList )
        pair.second->Mirror( aMirrorLeftRight, !aMirrorLeftRight, aMirrorRef );
}


void ZONE::RemoveCutout( int aOutlineIdx, int aHoleIdx )
{
    // Ensure the requested cutout is valid
    if( m_Poly->OutlineCount() < aOutlineIdx || m_Poly->HoleCount( aOutlineIdx ) < aHoleIdx )
        return;

    SHAPE_POLY_SET cutPoly( m_Poly->Hole( aOutlineIdx, aHoleIdx ) );

    // Add the cutout back to the zone
    m_Poly->BooleanAdd( cutPoly, SHAPE_POLY_SET::PM_FAST );

    SetNeedRefill( true );
}


void ZONE::AddPolygon( const SHAPE_LINE_CHAIN& aPolygon )
{
    wxASSERT( aPolygon.IsClosed() );

    // Add the outline as a new polygon in the polygon set
    if( m_Poly->OutlineCount() == 0 )
        m_Poly->AddOutline( aPolygon );
    else
        m_Poly->AddHole( aPolygon );

    SetNeedRefill( true );
}


void ZONE::AddPolygon( std::vector<VECTOR2I>& aPolygon )
{
    if( aPolygon.empty() )
        return;

    SHAPE_LINE_CHAIN outline;

    // Create an outline and populate it with the points of aPolygon
    for( const VECTOR2I& pt : aPolygon )
        outline.Append( pt );

    outline.SetClosed( true );

    AddPolygon( outline );
}


bool ZONE::AppendCorner( VECTOR2I aPosition, int aHoleIdx, bool aAllowDuplication )
{
    // Ensure the main outline exists:
    if( m_Poly->OutlineCount() == 0 )
        m_Poly->NewOutline();

    // If aHoleIdx >= 0, the corner musty be added to the hole, index aHoleIdx.
    // (remember: the index of the first hole is 0)
    // Return error if it does not exist.
    if( aHoleIdx >= m_Poly->HoleCount( 0 ) )
        return false;

    m_Poly->Append( aPosition.x, aPosition.y, -1, aHoleIdx, aAllowDuplication );

    SetNeedRefill( true );

    return true;
}


wxString ZONE::GetSelectMenuText( EDA_UNITS aUnits ) const
{
    wxString layerDesc;
    int      count = 0;

    for( PCB_LAYER_ID layer : m_layerSet.Seq() )
    {
        if( count == 0 )
            layerDesc = GetBoard()->GetLayerName( layer );

        count++;
    }

    if( count > 1 )
        layerDesc.Printf( _( "%s and %d more" ), layerDesc, count - 1 );

    // Check whether the selected contour is a hole (contour index > 0)
    if( m_CornerSelection != nullptr &&  m_CornerSelection->m_contour > 0 )
    {
        if( GetIsRuleArea() )
            return wxString::Format( _( "Rule Area Cutout on %s" ), layerDesc  );
        else
            return wxString::Format( _( "Zone Cutout on %s" ), layerDesc  );
    }
    else
    {
        if( GetIsRuleArea() )
            return wxString::Format( _( "Rule Area on %s" ), layerDesc );
        else
            return wxString::Format( _( "Zone %s on %s" ), GetNetnameMsg(), layerDesc );
    }
}


int ZONE::GetBorderHatchPitch() const
{
    return m_borderHatchPitch;
}


void ZONE::SetBorderDisplayStyle( ZONE_BORDER_DISPLAY_STYLE aBorderHatchStyle,
                                  int aBorderHatchPitch,
                                  bool aRebuildBorderHatch )
{
    aBorderHatchPitch = std::max( aBorderHatchPitch,
                                  Millimeter2iu( ZONE_BORDER_HATCH_MINDIST_MM ) );
    aBorderHatchPitch = std::min( aBorderHatchPitch,
                                  Millimeter2iu( ZONE_BORDER_HATCH_MAXDIST_MM ) );
    SetBorderHatchPitch( aBorderHatchPitch );
    m_borderStyle = aBorderHatchStyle;

    if( aRebuildBorderHatch )
        HatchBorder();
}


void ZONE::SetBorderHatchPitch( int aPitch )
{
    m_borderHatchPitch = aPitch;
}


void ZONE::UnHatchBorder()
{
    m_borderHatchLines.clear();
}


// Creates hatch lines inside the outline of the complex polygon
// sort function used in ::HatchBorder to sort points by descending VECTOR2I.x values
bool sortEndsByDescendingX( const VECTOR2I& ref, const VECTOR2I& tst )
{
    return tst.x < ref.x;
}


void ZONE::HatchBorder()
{
    UnHatchBorder();

    if( m_borderStyle == ZONE_BORDER_DISPLAY_STYLE::NO_HATCH
            || m_borderHatchPitch == 0
            || m_Poly->IsEmpty() )
    {
        return;
    }

    // define range for hatch lines
    int min_x = m_Poly->CVertex( 0 ).x;
    int max_x = m_Poly->CVertex( 0 ).x;
    int min_y = m_Poly->CVertex( 0 ).y;
    int max_y = m_Poly->CVertex( 0 ).y;

    for( auto iterator = m_Poly->IterateWithHoles(); iterator; iterator++ )
    {
        if( iterator->x < min_x )
            min_x = iterator->x;

        if( iterator->x > max_x )
            max_x = iterator->x;

        if( iterator->y < min_y )
            min_y = iterator->y;

        if( iterator->y > max_y )
            max_y = iterator->y;
    }

    // Calculate spacing between 2 hatch lines
    int spacing;

    if( m_borderStyle == ZONE_BORDER_DISPLAY_STYLE::DIAGONAL_EDGE )
        spacing = m_borderHatchPitch;
    else
        spacing = m_borderHatchPitch * 2;

    // set the "length" of hatch lines (the length on horizontal axis)
    int  hatch_line_len = m_borderHatchPitch;

    // To have a better look, give a slope depending on the layer
    int     layer = GetFirstLayer();
    int     slope_flag = (layer & 1) ? 1 : -1;  // 1 or -1
    double  slope = 0.707106 * slope_flag;      // 45 degrees slope
    int     max_a, min_a;

    if( slope_flag == 1 )
    {
        max_a   = KiROUND( max_y - slope * min_x );
        min_a   = KiROUND( min_y - slope * max_x );
    }
    else
    {
        max_a   = KiROUND( max_y - slope * max_x );
        min_a   = KiROUND( min_y - slope * min_x );
    }

    min_a = (min_a / spacing) * spacing;

    // calculate an offset depending on layer number,
    // for a better look of hatches on a multilayer board
    int offset = (layer * 7) / 8;
    min_a += offset;

    // loop through hatch lines
    std::vector<VECTOR2I> pointbuffer;
    pointbuffer.reserve( 256 );

    for( int a = min_a; a < max_a; a += spacing )
    {
        pointbuffer.clear();

        // Iterate through all vertices
        for( auto iterator = m_Poly->IterateSegmentsWithHoles(); iterator; iterator++ )
        {
            double x, y;

            SEG segment = *iterator;

            if( FindLineSegmentIntersection( a, slope, segment.A.x, segment.A.y, segment.B.x,
                    segment.B.y, x, y ) )
                pointbuffer.emplace_back( KiROUND( x ), KiROUND( y ) );
        }

        // sort points in order of descending x (if more than 2) to
        // ensure the starting point and the ending point of the same segment
        // are stored one just after the other.
        if( pointbuffer.size() > 2 )
            sort( pointbuffer.begin(), pointbuffer.end(), sortEndsByDescendingX );

        // creates lines or short segments inside the complex polygon
        for( size_t ip = 0; ip + 1 < pointbuffer.size(); ip += 2 )
        {
            int dx = pointbuffer[ip + 1].x - pointbuffer[ip].x;

            // Push only one line for diagonal hatch,
            // or for small lines < twice the line length
            // else push 2 small lines
            if( m_borderStyle == ZONE_BORDER_DISPLAY_STYLE::DIAGONAL_FULL
                || std::abs( dx ) < 2 * hatch_line_len )
            {
                m_borderHatchLines.emplace_back( SEG( pointbuffer[ip], pointbuffer[ ip + 1] ) );
            }
            else
            {
                double dy = pointbuffer[ip + 1].y - pointbuffer[ip].y;
                slope = dy / dx;

                if( dx > 0 )
                    dx = hatch_line_len;
                else
                    dx = -hatch_line_len;

                int x1 = KiROUND( pointbuffer[ip].x + dx );
                int x2 = KiROUND( pointbuffer[ip + 1].x - dx );
                int y1 = KiROUND( pointbuffer[ip].y + dx * slope );
                int y2 = KiROUND( pointbuffer[ip + 1].y - dx * slope );

                m_borderHatchLines.emplace_back( SEG( pointbuffer[ip].x, pointbuffer[ip].y,
                                                      x1, y1 ) );

                m_borderHatchLines.emplace_back( SEG( pointbuffer[ip+1].x, pointbuffer[ip+1].y,
                                                      x2, y2 ) );
            }
        }
    }
}


int ZONE::GetDefaultHatchPitch()
{
    return Mils2iu( ZONE_BORDER_HATCH_DIST_MIL );
}


BITMAPS ZONE::GetMenuImage() const
{
    return BITMAPS::add_zone;
}


void ZONE::SwapData( BOARD_ITEM* aImage )
{
    assert( aImage->Type() == PCB_ZONE_T || aImage->Type() == PCB_FP_ZONE_T );

    std::swap( *((ZONE*) this), *((ZONE*) aImage) );
}


void ZONE::CacheTriangulation( PCB_LAYER_ID aLayer )
{
    if( aLayer == UNDEFINED_LAYER )
    {
        for( std::pair<const PCB_LAYER_ID, std::shared_ptr<SHAPE_POLY_SET>>& pair : m_FilledPolysList )
            pair.second->CacheTriangulation();

        m_Poly->CacheTriangulation( false );
    }
    else
    {
        if( m_FilledPolysList.count( aLayer ) )
            m_FilledPolysList[ aLayer ]->CacheTriangulation();
    }
}


bool ZONE::IsIsland( PCB_LAYER_ID aLayer, int aPolyIdx ) const
{
    if( GetNetCode() < 1 )
        return true;

    if( !m_insulatedIslands.count( aLayer ) )
        return false;

    return m_insulatedIslands.at( aLayer ).count( aPolyIdx );
}


void ZONE::GetInteractingZones( PCB_LAYER_ID aLayer, std::vector<ZONE*>* aZones ) const
{
    int epsilon = Millimeter2iu( 0.001 );

    for( ZONE* candidate : GetBoard()->Zones() )
    {
        if( candidate == this )
            continue;

        if( !candidate->GetLayerSet().test( aLayer ) )
            continue;

        if( candidate->GetIsRuleArea() )
            continue;

        if( candidate->GetNetCode() != GetNetCode() )
            continue;

        for( auto iter = m_Poly->CIterate(); iter; iter++ )
        {
            if( candidate->m_Poly->Collide( iter.Get(), epsilon ) )
            {
                aZones->push_back( candidate );
                break;
            }
        }
    }
}


bool ZONE::BuildSmoothedPoly( SHAPE_POLY_SET& aSmoothedPoly, PCB_LAYER_ID aLayer,
                              SHAPE_POLY_SET* aBoardOutline,
                              SHAPE_POLY_SET* aSmoothedPolyWithApron ) const
{
    if( GetNumCorners() <= 2 )  // malformed zone. polygon calculations will not like it ...
        return false;

    // Processing of arc shapes in zones is not yet supported because Clipper can't do boolean
    // operations on them.  The poly outline must be converted to segments first.
    SHAPE_POLY_SET flattened = m_Poly->CloneDropTriangulation();
    flattened.ClearArcs();

    if( GetIsRuleArea() )
    {
        // We like keepouts just the way they are....
        aSmoothedPoly = flattened;
        return true;
    }

    const BOARD* board = GetBoard();
    int          maxError = ARC_HIGH_DEF;
    bool         keepExternalFillets = false;
    bool         smooth_requested = m_cornerSmoothingType == ZONE_SETTINGS::SMOOTHING_CHAMFER
                                    || m_cornerSmoothingType == ZONE_SETTINGS::SMOOTHING_FILLET;

    if( IsTeardropArea() )  // We use teardrop shapes with no smoothing
                            // these shapes are already optimized
        smooth_requested = false;

    if( board )
    {
        BOARD_DESIGN_SETTINGS& bds = board->GetDesignSettings();

        maxError = bds.m_MaxError;
        keepExternalFillets = bds.m_ZoneKeepExternalFillets;
    }

    auto smooth = [&]( SHAPE_POLY_SET& aPoly )
                  {

                      if( !smooth_requested )
                          return;

                      switch( m_cornerSmoothingType )
                      {
                      case ZONE_SETTINGS::SMOOTHING_CHAMFER:
                          aPoly = aPoly.Chamfer( (int) m_cornerRadius );
                          break;

                      case ZONE_SETTINGS::SMOOTHING_FILLET:
                      {
                          aPoly = aPoly.Fillet( (int) m_cornerRadius, maxError );
                          break;
                      }

                      default:
                          break;
                      }
                  };

    std::vector<ZONE*> interactingZones;
    GetInteractingZones( aLayer, &interactingZones );

    SHAPE_POLY_SET* maxExtents = &flattened;
    SHAPE_POLY_SET  withFillets;

    aSmoothedPoly = flattened;

    // Should external fillets (that is, those applied to concave corners) be kept?  While it
    // seems safer to never have copper extend outside the zone outline, 5.1.x and prior did
    // indeed fill them so we leave the mode available.
    if( keepExternalFillets && smooth_requested )
    {
        withFillets = flattened;
        smooth( withFillets );
        withFillets.BooleanAdd( flattened, SHAPE_POLY_SET::PM_FAST );
        maxExtents = &withFillets;
    }

    for( ZONE* zone : interactingZones )
    {
        SHAPE_POLY_SET flattened_outline = zone->Outline()->CloneDropTriangulation();
        flattened_outline.ClearArcs();
        aSmoothedPoly.BooleanAdd( flattened_outline, SHAPE_POLY_SET::PM_FAST );
    }

    if( aBoardOutline )
        aSmoothedPoly.BooleanIntersection( *aBoardOutline, SHAPE_POLY_SET::PM_STRICTLY_SIMPLE );

    smooth( aSmoothedPoly );

    if( aSmoothedPolyWithApron )
    {
        SHAPE_POLY_SET poly = maxExtents->CloneDropTriangulation();
        poly.Inflate( m_ZoneMinThickness, 64 );
        *aSmoothedPolyWithApron = aSmoothedPoly;
        aSmoothedPolyWithApron->BooleanIntersection( poly, SHAPE_POLY_SET::PM_FAST );
    }

    aSmoothedPoly.BooleanIntersection( *maxExtents, SHAPE_POLY_SET::PM_FAST );

    return true;
}


double ZONE::CalculateFilledArea()
{
    m_area = 0.0;

    // Iterate over each outline polygon in the zone and then iterate over
    // each hole it has to compute the total area.
    for( std::pair<const PCB_LAYER_ID, std::shared_ptr<SHAPE_POLY_SET>>& pair : m_FilledPolysList )
    {
        std::shared_ptr<SHAPE_POLY_SET>& poly = pair.second;

        for( int i = 0; i < poly->OutlineCount(); i++ )
        {
            m_area += poly->Outline( i ).Area();

            for( int j = 0; j < poly->HoleCount( i ); j++ )
                m_area -= poly->Hole( i, j ).Area();
        }
    }

    return m_area;
}


double ZONE::CalculateOutlineArea()
{
    m_outlinearea = std::abs( m_Poly->Area() );
    return m_outlinearea;
}


void ZONE::TransformSmoothedOutlineToPolygon( SHAPE_POLY_SET& aCornerBuffer, int aClearance,
                                              int aMaxError, ERROR_LOC aErrorLoc,
                                              SHAPE_POLY_SET* aBoardOutline ) const
{
    // Creates the zone outline polygon (with holes if any)
    SHAPE_POLY_SET polybuffer;

    // TODO: using GetFirstLayer() means it only works for single-layer zones....
    BuildSmoothedPoly( polybuffer, GetFirstLayer(), aBoardOutline );

    // Calculate the polygon with clearance
    // holes are linked to the main outline, so only one polygon is created.
    if( aClearance )
    {
        const BOARD* board = GetBoard();
        int          maxError = ARC_HIGH_DEF;

        if( board )
            maxError = board->GetDesignSettings().m_MaxError;

        int segCount = GetArcToSegmentCount( aClearance, maxError, FULL_CIRCLE );

        if( aErrorLoc == ERROR_OUTSIDE )
            aClearance += aMaxError;

        polybuffer.Inflate( aClearance, segCount );
    }

    polybuffer.Fracture( SHAPE_POLY_SET::PM_FAST );
    aCornerBuffer.Append( polybuffer );
}


FP_ZONE::FP_ZONE( BOARD_ITEM_CONTAINER* aParent ) :
        ZONE( aParent, true )
{
    // in a footprint, net classes are not managed.
    // so set the net to NETINFO_LIST::ORPHANED_ITEM
    SetNetCode( -1, true );
}


FP_ZONE::FP_ZONE( const FP_ZONE& aZone ) :
        ZONE( aZone )
{
    InitDataFromSrcInCopyCtor( aZone );
}


FP_ZONE& FP_ZONE::operator=( const FP_ZONE& aOther )
{
    ZONE::operator=( aOther );
    return *this;
}


EDA_ITEM* FP_ZONE::Clone() const
{
    return new FP_ZONE( *this );
}


double FP_ZONE::ViewGetLOD( int aLayer, KIGFX::VIEW* aView ) const
{
    constexpr double HIDE = (double)std::numeric_limits<double>::max();

    if( !aView )
        return 0;

    if( !aView->IsLayerVisible( LAYER_ZONES ) )
        return HIDE;

    bool flipped = GetParent() && GetParent()->GetLayer() == B_Cu;

    // Handle Render tab switches
    if( !flipped && !aView->IsLayerVisible( LAYER_MOD_FR ) )
        return HIDE;

    if( flipped && !aView->IsLayerVisible( LAYER_MOD_BK ) )
        return HIDE;

    // Other layers are shown without any conditions
    return 0.0;
}


std::shared_ptr<SHAPE> ZONE::GetEffectiveShape( PCB_LAYER_ID aLayer, FLASHING aFlash ) const
{
    if( m_FilledPolysList.find( aLayer ) == m_FilledPolysList.end() )
        return std::make_shared<SHAPE_NULL>();
    else
        return m_FilledPolysList.at( aLayer );
}


void ZONE::TransformShapeWithClearanceToPolygon( SHAPE_POLY_SET& aCornerBuffer,
                                                 PCB_LAYER_ID aLayer, int aClearance, int aError,
                                                 ERROR_LOC aErrorLoc, bool aIgnoreLineWidth ) const
{
    wxASSERT_MSG( !aIgnoreLineWidth, wxT( "IgnoreLineWidth has no meaning for zones." ) );

    if( !m_FilledPolysList.count( aLayer ) )
        return;

    if( !aClearance )
    {
        aCornerBuffer.Append( *m_FilledPolysList.at( aLayer ) );
        return;
    }
    
    SHAPE_POLY_SET temp_buf = m_FilledPolysList.at( aLayer )->CloneDropTriangulation();

    // Rebuild filled areas only if clearance is not 0
    int numSegs = GetArcToSegmentCount( aClearance, aError, FULL_CIRCLE );

    if( aErrorLoc == ERROR_OUTSIDE )
        aClearance += aError;

    temp_buf.InflateWithLinkedHoles( aClearance, numSegs, SHAPE_POLY_SET::PM_FAST );

    aCornerBuffer.Append( temp_buf );
}


void ZONE::TransformSolidAreasShapesToPolygon( PCB_LAYER_ID aLayer, SHAPE_POLY_SET& aCornerBuffer,
                                               int aError ) const
{
    if( m_FilledPolysList.count( aLayer ) && !m_FilledPolysList.at( aLayer )->IsEmpty() )
        aCornerBuffer.Append( *m_FilledPolysList.at( aLayer ) );
}


static struct ZONE_DESC
{
    ZONE_DESC()
    {
        ENUM_MAP<ZONE_CONNECTION>::Instance()
                .Map( ZONE_CONNECTION::INHERITED,   _HKI( "Inherited" ) )
                .Map( ZONE_CONNECTION::NONE,        _HKI( "None" ) )
                .Map( ZONE_CONNECTION::THERMAL,     _HKI( "Thermal reliefs" ) )
                .Map( ZONE_CONNECTION::FULL,        _HKI( "Solid" ) )
                .Map( ZONE_CONNECTION::THT_THERMAL, _HKI( "Thermal reliefs for PTH" ) );

        PROPERTY_MANAGER& propMgr = PROPERTY_MANAGER::Instance();
        REGISTER_TYPE( ZONE );
        propMgr.InheritsAfter( TYPE_HASH( ZONE ), TYPE_HASH( BOARD_CONNECTED_ITEM ) );
        propMgr.AddProperty( new PROPERTY<ZONE, unsigned>( _HKI( "Priority" ),
                    &ZONE::SetAssignedPriority, &ZONE::GetAssignedPriority ) );
        //propMgr.AddProperty( new PROPERTY<ZONE, bool>( "Filled",
                    //&ZONE::SetIsFilled, &ZONE::IsFilled ) );
        propMgr.AddProperty( new PROPERTY<ZONE, wxString>( _HKI( "Name" ),
                    &ZONE::SetZoneName, &ZONE::GetZoneName ) );
        propMgr.AddProperty( new PROPERTY<ZONE, int>( _HKI( "Clearance Override" ),
                    &ZONE::SetLocalClearance, &ZONE::GetLocalClearance,
                    PROPERTY_DISPLAY::DISTANCE ) );
        propMgr.AddProperty( new PROPERTY<ZONE, int>( _HKI( "Min Width" ),
                    &ZONE::SetMinThickness, &ZONE::GetMinThickness,
                    PROPERTY_DISPLAY::DISTANCE ) );
        propMgr.AddProperty( new PROPERTY_ENUM<ZONE, ZONE_CONNECTION>( _HKI( "Pad Connections" ),
                    &ZONE::SetPadConnection, &ZONE::GetPadConnection ) );
        propMgr.AddProperty( new PROPERTY<ZONE, int>( _HKI( "Thermal Relief Gap" ),
                    &ZONE::SetThermalReliefGap, &ZONE::GetThermalReliefGap,
                    PROPERTY_DISPLAY::DISTANCE ) );
        propMgr.AddProperty( new PROPERTY<ZONE, int>( _HKI( "Thermal Relief Spoke Width" ),
                    &ZONE::SetThermalReliefSpokeWidth, &ZONE::GetThermalReliefSpokeWidth,
                    PROPERTY_DISPLAY::DISTANCE ) );
    }
} _ZONE_DESC;

ENUM_TO_WXANY( ZONE_CONNECTION );
