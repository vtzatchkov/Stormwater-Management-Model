//-----------------------------------------------------------------------------
//   coupling.c
//
//   Project:  EPA SWMM5
//   Version:  5.1
//   Date:     03/20/14   (Build 5.1.001)
//             09/15/14   (Build 5.1.007)
//             04/02/15   (Build 5.1.008)
//             08/05/15   (Build 5.1.010)
//   Authors:  Laurent Courty
//
//   Copyrighted (c) distributed under the MIT license (see LICENSE.txt)
//
//   Overland model flow coupling functions.
//
//-----------------------------------------------------------------------------
#define _CRT_SECURE_NO_DEPRECATE

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "headers.h"

//-----------------------------------------------------------------------------
//  Local functions
//-----------------------------------------------------------------------------
static void   coupling_findNodeInflow(int j, double tStep,
                                    double overlandDepth, double overlandSurfArea);
static int    coupling_isNodeCoupled(int j);
static void   coupling_adjustInflows(int j, double inflowAdjustingFactor);
static void   opening_findCouplingType(TCoverOpening* opening, double crestElev,
                                       double nodeHead, double overlandHead);
static void   opening_findCouplingInflow(TCoverOpening* opening, double crestElev,
                                         double nodeHead, double overlandHead);

//=============================================================================

void coupling_initOverlandData(int j)
//
//  Input:   j = node index
//  Output:  none
//  Purpose: initializes a OverlandData's value variables at start of simulation.
//
{
    OverlandData[j].surfArea = 0.0;
    OverlandData[j].depth = 0.0;
    OverlandData[j].couplingFlow = 0.0;
}

//=============================================================================

void coupling_execute(double tStep)
//
//  Input:   tStep = time step of the drainage model (s)
//  Output:  none
//  Purpose: find coupling flow for each node.
//
{
    int j;
    double overlandDepth, overlandSurfArea;

    for ( j = 0; j < Nobjects[NODE]; j++ )
    {
        if ( !coupling_isNodeCoupled(j) ) continue;
        // --- find coupling flow
        overlandDepth =  OverlandData[j].depth;
        overlandSurfArea = OverlandData[j].surfArea;
        coupling_findNodeInflow(j, tStep, overlandDepth, overlandSurfArea);
        // --- apply coupling flow to output array
        OverlandData[j].couplingFlow = Node[j].overlandInflow;
    }
}

//=============================================================================

void coupling_findNodeInflow(int j, double tStep,
                           double overlandDepth, double overlandSurfArea)
//
//  Input:   j = node index
//           tStep = time step of the drainage model (s)
//           overlandDepth = water depth in the overland model (ft)
//           overlandSurfArea = surface in the overland model that is coupled to this node (ft2)
//  Output:  none
//  Purpose: compute the coupling inflow for each node's opening
//
{
    double crestElev, overlandHead, nodeHead;
    double totalCouplingInflow;
    double rawMaxInflow, maxInflow, inflowAdjustingFactor;
    int inflow2outflow, outflow2inflow;
    TCoverOpening* opening;

    // --- calculate elevations
    crestElev = Node[j].invertElev + Node[j].fullDepth;
    nodeHead = Node[j].invertElev + Node[j].newDepth;
    overlandHead = crestElev + overlandDepth;

    // --- init
    totalCouplingInflow = 0.0;

    // --- iterate among the openings
    opening = Node[j].coverOpening;
    while ( opening )
    {
        // --- compute types and inflows
        opening_findCouplingType(opening, crestElev, nodeHead, overlandHead);
        opening_findCouplingInflow(opening, crestElev, nodeHead, overlandHead);
        // --- prevent oscillations
        inflow2outflow = (opening->oldInflow > 0.0) && (opening->newInflow < 0.0);
        outflow2inflow = (opening->oldInflow < 0.0) && (opening->newInflow > 0.0);
        if (inflow2outflow || outflow2inflow)
        {
            opening->couplingType = NO_COUPLING_FLOW;
            opening->newInflow = 0.0;
        }
        // --- add inflow to total inflow
        totalCouplingInflow += opening->newInflow;
        opening = opening->next;
    }
    // --- inflow cannot drain the overland model
    if (totalCouplingInflow > 0.0)
    {
        rawMaxInflow = (overlandDepth * overlandSurfArea) / tStep;
        maxInflow = fmin(rawMaxInflow, totalCouplingInflow);
        inflowAdjustingFactor = maxInflow / totalCouplingInflow;
        coupling_adjustInflows(j, inflowAdjustingFactor);
        // --- get adjusted inflows
        opening = Node[j].coverOpening;
        while ( opening )
        {
            totalCouplingInflow += opening->newInflow;
            opening = opening->next;
        }
    }
    Node[j].overlandInflow = totalCouplingInflow;
}

//=============================================================================

void coupling_setOldState(int j)
//
//  Input:   j = node index
//  Output:  none
//  Purpose: replaces a node's old hydraulic coupling state values with new ones.
//
{
    TCoverOpening* opening;

    // --- iterate among the openings.
    opening = Node[j].coverOpening;
    while ( opening )
    {
        opening->oldInflow = opening->newInflow;
        opening = opening->next;
    }
}

//=============================================================================

int coupling_isNodeCoupled(int j)
//
//  Input:   j = node index
//  Output:  returns the coupling status of a node (yes/no)
//  Purpose: if at least one opening is coupled, return yes.
//
{
    int isCoupled = FALSE;
    TCoverOpening* opening;

    // --- iterate among the openings. If one is coupled, return YES
    opening = Node[j].coverOpening;
    while ( opening )
    {
        if ( opening->couplingType != NO_COUPLING )
        {
            isCoupled = TRUE;
            break;
        }
        opening = opening->next;
    }
    return isCoupled;
}

//=============================================================================

void coupling_adjustInflows(int j, double inflowAdjustingFactor)
//
//  Input:   j = node index
//           inflowAdjustingFactor = an inflow adjusting coefficient
//  Output:  none
//  Purpose: adjust the inflow according to an adjusting factor
//
{
    TCoverOpening* opening;

    // --- iterate among the openings
    opening = Node[j].coverOpening;
    while ( opening )
    {
        opening->newInflow = opening->newInflow * inflowAdjustingFactor;
        opening = opening->next;
    }
}

//=============================================================================

void coupling_deleteOpenings(int j)
//
//  Input:   j = node index
//  Output:  none
//  Purpose: deletes all opening data for a node.
//
{
    TCoverOpening* opening1;
    TCoverOpening* opening2;
    opening1 = Node[j].coverOpening;
    while ( opening1 )
    {
        opening2 = opening1->next;
        free(opening1);
        opening1 = opening2;
    }
}

//=============================================================================

void opening_findCouplingType(TCoverOpening* opening, double crestElev,
                              double nodeHead, double overlandHead)
//
//  Input:   opening = a node opening data structure
//           nodeHead = water elevation in the node (ft)
//           crestElev = elevation of the node crest (= ground) (ft)
//           overlandHead = water elevation in the overland model (ft)
//  Output:  nothing
//  Purpose: determine the coupling type of an opening
//           according the the relative water elevations in node and surface
//
{
    int overflow, drainage;
    int overflowOrifice, drainageOrifice, submergedWeir, freeWeir;
    double surfaceDepth, weirRatio;
    double overflowArea = opening->area;
    double weirWidth = opening->length;

    surfaceDepth = overlandHead - crestElev;
    weirRatio = overflowArea / weirWidth;

    // --- boolean cases. See DOI 10.1016/j.jhydrol.2017.06.024
    overflow = nodeHead > overlandHead;
    drainage = nodeHead < overlandHead;
    overflowOrifice = overflow;
    drainageOrifice = drainage &&
                      ((nodeHead > crestElev) && (surfaceDepth > weirRatio));
    submergedWeir = drainage &&
                    ((nodeHead > crestElev) && (surfaceDepth < weirRatio));
    freeWeir = drainage && (nodeHead < crestElev);

    // --- set the coupling type
    if ( !overflow && !drainage ) opening->couplingType = NO_COUPLING_FLOW;
    else if ( overflowOrifice || drainageOrifice )
        opening->couplingType = ORIFICE_COUPLING;
    else if ( submergedWeir ) opening->couplingType = SUBMERGED_WEIR_COUPLING;
    else if ( freeWeir ) opening->couplingType = FREE_WEIR_COUPLING;
    else opening->couplingType = NO_COUPLING_FLOW;
}

//=============================================================================

void opening_findCouplingInflow(TCoverOpening* opening, double crestElev,
                                double nodeHead, double overlandHead)
//
//  Input:   opening = a node opening data structure
//           nodeHead = water elevation in the node (ft)
//           crestElev = elevation of the node crest (= ground) (ft)
//           overlandHead = water elevation in the overland model (ft)
//  Output:  the flow entering through the opening (ft3/s)
//  Purpose: computes the coupling flow of the opening
//
{
    double orificeCoeff, freeWeirCoeff, subWeirCoeff, overflowArea, weirWidth;
    double headUp, headDown, headDiff, depthUp;
    double orif_v, sweir_v, u_couplingFlow;

    orificeCoeff = opening->coeffOrifice;
    freeWeirCoeff = opening->coeffFreeWeir;
    subWeirCoeff = opening->coeffSubWeir;
    overflowArea = opening->area;
    weirWidth = opening->length;

    headUp = fmax(overlandHead, nodeHead);
    headDown = fmin(overlandHead, nodeHead);
    headDiff = headUp - headDown;
    depthUp =  headUp - crestElev;

    switch(opening->couplingType)
    {
        case ORIFICE_COUPLING:
            orif_v = sqrt(2 * GRAVITY * headDiff);
            u_couplingFlow = orificeCoeff * overflowArea * orif_v;
        case FREE_WEIR_COUPLING:
            u_couplingFlow = (2/3.) * freeWeirCoeff * weirWidth *
                             pow(depthUp, 3/2.) * sqrt(2 * GRAVITY);
        case SUBMERGED_WEIR_COUPLING:
            sweir_v = sqrt(2. * GRAVITY * depthUp);
            u_couplingFlow = subWeirCoeff * weirWidth * depthUp * sweir_v;
        default: u_couplingFlow =  0.0;
    }
    // --- Flow into the node is positive
    opening->newInflow = copysign(u_couplingFlow, overlandHead - nodeHead);
}
