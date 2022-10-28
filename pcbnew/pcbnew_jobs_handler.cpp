/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2022 Mark Roszko <mark.roszko@gmail.com>
 * Copyright (C) 1992-2022 KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "pcbnew_jobs_handler.h"
#include <kicad2step.h>
#include <jobs/job_export_pcb_dxf.h>
#include <jobs/job_export_pcb_svg.h>
#include <jobs/job_export_pcb_step.h>
#include <cli/exit_codes.h>
#include <plotters/plotters_pslike.h>
#include <plotters/plotter_dxf.h>
#include <pgm_base.h>
#include <pcbplot.h>
#include <board_design_settings.h>
#include <pcbnew_settings.h>
#include <wx/crt.h>
#include <pcb_plot_svg.h>

#include "pcbnew_scripting_helpers.h"

PCBNEW_JOBS_HANDLER::PCBNEW_JOBS_HANDLER()
{
    Register( "step",
              std::bind( &PCBNEW_JOBS_HANDLER::JobExportStep, this, std::placeholders::_1 ) );
    Register( "svg", std::bind( &PCBNEW_JOBS_HANDLER::JobExportSvg, this, std::placeholders::_1 ) );
    Register( "dxf", std::bind( &PCBNEW_JOBS_HANDLER::JobExportDxf, this, std::placeholders::_1 ) );
}


int PCBNEW_JOBS_HANDLER::JobExportStep( JOB* aJob )
{
    JOB_EXPORT_PCB_STEP* aStepJob = dynamic_cast<JOB_EXPORT_PCB_STEP*>( aJob );

    if( aStepJob == nullptr )
        return CLI::EXIT_CODES::ERR_UNKNOWN;

    KICAD2MCAD_PRMS params;
    params.m_useDrillOrigin = aStepJob->m_useDrillOrigin;
    params.m_useGridOrigin = aStepJob->m_useGridOrigin;
    params.m_overwrite = aStepJob->m_overwrite;
    params.m_includeVirtual = aStepJob->m_includeVirtual;
    params.m_filename = aStepJob->m_filename;
    params.m_outputFile = aStepJob->m_outputFile;
    params.m_xOrigin = aStepJob->m_xOrigin;
    params.m_yOrigin = aStepJob->m_yOrigin;
    params.m_minDistance = aStepJob->m_minDistance;
    params.m_substModels = aStepJob->m_substModels;

    // we might need the lifetime of the converter to continue until frame destruction
    // due to the gui parameter
    KICAD2STEP* converter = new KICAD2STEP( params );

    return converter->Run();
}


int PCBNEW_JOBS_HANDLER::JobExportSvg( JOB* aJob )
{
    JOB_EXPORT_PCB_SVG* aSvgJob = dynamic_cast<JOB_EXPORT_PCB_SVG*>( aJob );

    if( aSvgJob == nullptr )
        return CLI::EXIT_CODES::ERR_UNKNOWN;

    PCB_PLOT_SVG_OPTIONS svgPlotOptions;
    svgPlotOptions.m_blackAndWhite = aSvgJob->m_blackAndWhite;
    svgPlotOptions.m_colorTheme = aSvgJob->m_colorTheme;
    svgPlotOptions.m_outputFile = aSvgJob->m_outputFile;
    svgPlotOptions.m_mirror = aSvgJob->m_mirror;
    svgPlotOptions.m_pageSizeMode = aSvgJob->m_pageSizeMode;
    svgPlotOptions.m_printMaskLayer = aSvgJob->m_printMaskLayer;

    if( aJob->IsCli() )
        wxPrintf( _( "Loading board\n" ) );

    BOARD* brd = LoadBoard( aSvgJob->m_filename );

    if( aJob->IsCli() )
    {
        if( PCB_PLOT_SVG::Plot( brd, svgPlotOptions ) )
            wxPrintf( _( "Successfully created svg file" ) );
        else
            wxPrintf( _( "Error creating svg file" ) );
    }

    return CLI::EXIT_CODES::OK;
}


int PCBNEW_JOBS_HANDLER::JobExportDxf( JOB* aJob )
{
    JOB_EXPORT_PCB_DXF* aDxfJob = dynamic_cast<JOB_EXPORT_PCB_DXF*>( aJob );

    if( aDxfJob == nullptr )
        return CLI::EXIT_CODES::ERR_UNKNOWN;

    if( aJob->IsCli() )
        wxPrintf( _( "Loading board\n" ) );

    BOARD* brd = LoadBoard( aDxfJob->m_filename );

    if( aDxfJob->m_outputFile.IsEmpty() )
    {
        wxFileName fn = brd->GetFileName();
        fn.SetName( fn.GetName() );
        fn.SetExt( wxS( "dxf" ) );

        aDxfJob->m_outputFile = fn.GetFullName();
    }

    PCB_PLOT_PARAMS plotOpts;
    plotOpts.SetFormat( PLOT_FORMAT::DXF );


    plotOpts.SetDXFPlotPolygonMode( aDxfJob->m_plotGraphicItemsUsingContours );

    if( aDxfJob->m_dxfUnits == JOB_EXPORT_PCB_DXF::DXF_UNITS::MILLIMETERS )
    {
        plotOpts.SetDXFPlotUnits( DXF_UNITS::MILLIMETERS );
    }
    else
    {
        plotOpts.SetDXFPlotUnits( DXF_UNITS::INCHES );
    }
    plotOpts.SetPlotValue( aDxfJob->m_plotFootprintValues );
    plotOpts.SetPlotReference( aDxfJob->m_plotRefDes );

    plotOpts.SetLayerSelection( aDxfJob->m_printMaskLayer );


    //@todo allow controlling the sheet name and path that will be displayed in the title block
    // Leave blank for now
    DXF_PLOTTER* plotter = (DXF_PLOTTER*) StartPlotBoard(
            brd, &plotOpts, UNDEFINED_LAYER, aDxfJob->m_outputFile, wxEmptyString, wxEmptyString );

    if( plotter )
    {
        PlotBoardLayers( brd, plotter, aDxfJob->m_printMaskLayer.SeqStackupBottom2Top(), plotOpts );
        plotter->EndPlot();
    }

    delete plotter;

    return CLI::EXIT_CODES::OK;
}