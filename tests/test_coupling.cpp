/*
 *   test_coupling.cpp
 *
 *   Created: 09/06/2020
 *   Authors: Velitchko G. Tzatchkov
 *            Laurent Courty
 *            IMTA, Mexico
 *
 *   Unit testing for SWMM coupling functions using Boost Test.
 */

#define BOOST_TEST_MODULE "coupling"
#include <boost/test/included/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include "test_toolkitapi_coupling.hpp"

struct CoverOpening
{
   int            ID;              // opening index number
   int            type;            // type of opening (grate, etc). From an enum
   int            couplingType;    // type of surface coupling (enum SurfaceCouplingType)
   double         area;            // area of the opening (ft2)
   double         length;          // length of the opening (~circumference, ft)
   double         coeffOrifice;    // orifice coefficient
   double         coeffFreeWeir;   // free weir coefficient
   double         coeffSubWeir;    // submerged weir coefficient
   double         oldInflow;       // inflow during last time-step
   double         newInflow;       // current inflow
   struct CoverOpening* next;      // pointer to next opening data object
};
typedef struct CoverOpening TCoverOpening;

struct ExtInflow
{
   int            param;         // pollutant index (flow = -1)
   int            type;          // CONCEN or MASS
   int            tSeries;       // index of inflow time series
   int            basePat;       // baseline time pattern
   double         cFactor;       // units conversion factor for mass inflow
   double         baseline;      // constant baseline value
   double         sFactor;       // time series scaling factor
   double         extIfaceInflow;// external interfacing inflow
   struct ExtInflow* next;       // pointer to next inflow data object
};
typedef struct ExtInflow TExtInflow;

struct DwfInflow
{
   int            param;          // pollutant index (flow = -1)
   double         avgValue;       // average value (cfs or concen.)
   int            patterns[4];    // monthly, daily, hourly, weekend time patterns
   struct DwfInflow* next;        // pointer to next inflow data object
};
typedef struct DwfInflow TDwfInflow;

typedef struct
{
   int           unitHyd;         // index of unit hydrograph
   double        area;            // area of sewershed (ft2)
}  TRdiiInflow;

//  Node in a tokenized math expression list
struct ExprNode
{
    int    opcode;                // operator code
    int    ivar;                  // variable index
    double fvalue;                // numerical value
	struct ExprNode *prev;        // previous node
    struct ExprNode *next;        // next node
};
typedef struct ExprNode MathExpr;

typedef struct
{
    int          treatType;       // treatment equation type: REMOVAL/CONCEN
    MathExpr*    equation;        // treatment eqn. as tokenized math terms
} TTreatment;

typedef struct
{
   char*         ID;              // node ID
   int           type;            // node type code
   int           subIndex;        // index of node's sub-category
   char          rptFlag;         // reporting flag
   double        invertElev;      // invert elevation (ft)
   double        initDepth;       // initial storage level (ft)
   double        fullDepth;       // dist. from invert to surface (ft)
   double        surDepth;        // added depth under surcharge (ft)
   double        pondedArea;      // area filled by ponded water (ft2)
   double        surfaceArea;     // area used to calculate node's volume (ft2)
   TExtInflow*   extInflow;       // pointer to external inflow data
   TDwfInflow*   dwfInflow;       // pointer to dry weather flow inflow data
   TRdiiInflow*  rdiiInflow;      // pointer to RDII inflow data
   TTreatment*   treatment;       // array of treatment data
   //-----------------------------
   TCoverOpening* coverOpening;   // pointer to node opening data
   double        couplingArea;    // coupling area in the overland model (ft2)
   double        overlandDepth;   // water depth in the overland model (ft)
   double        couplingInflow;  // flow from the overland model (cfs)
   //-----------------------------
   int           degree;          // number of outflow links
   char          updated;         // true if state has been updated
   double        crownElev;       // top of highest flowing closed conduit (ft)
   double        inflow;          // total inflow (cfs)
   double        outflow;         // total outflow (cfs)
   double        losses;          // evap + exfiltration loss (ft3)
   double        oldVolume;       // previous volume (ft3)
   double        newVolume;       // current volume (ft3)
   double        fullVolume;      // max. storage available (ft3)
   double        overflow;        // overflow rate (cfs)
   double        oldDepth;        // previous water depth (ft)
   double        newDepth;        // current water depth (ft)
   double        oldLatFlow;      // previous lateral inflow (cfs)
   double        newLatFlow;      // current lateral inflow (cfs)
   double*       oldQual;         // previous quality state
   double*       newQual;         // current quality state
   double        oldFlowInflow;   // previous flow inflow
   double        oldNetInflow;    // previous net inflow
}  TNode;

enum  OverlandCouplingType {
      NO_COUPLING,
      NO_COUPLING_FLOW,
      ORIFICE_COUPLING,
      FREE_WEIR_COUPLING,
      SUBMERGED_WEIR_COUPLING};

#define   GRAVITY            32.2           // accel. of gravity in US units
#define MAX_OBJ_TYPES 16
#define ERR_NONE 0
#define ERR_API_OBJECT_INDEX 505
#define ERR_MEMORY 101

TNode* Node = new TNode;
        
BOOST_AUTO_TEST_SUITE(test_coupling)

    BOOST_FIXTURE_TEST_CASE(NodeOpeningTests, FixtureOpenClose) 
    {
        int error, node_ind, no_of_deleted_openings, coupling;
        int no_of_openings;
        double area, width;
        double NodeOpeningFlowInflow = 0.0;
        char node_id[] = "J1";
        error = swmm_getObjectIndex(SM_NODE, node_id, &node_ind);
        BOOST_REQUIRE(error == ERR_NONE);
        BOOST_REQUIRE(node_ind == 1);
        Node[node_ind].coverOpening = nullptr;

        // Add a first opening to Node[node_ind]
        area = 25.0;
        width = 1.0;
        error = swmm_setNodeOpening(node_ind, 0, 0, area, width, 0.167, 0.54, 0.056);
        BOOST_CHECK_EQUAL(error, ERR_NONE);
        // One opening added
        error = swmm_getOpeningsNum(node_ind, &no_of_openings);
        BOOST_REQUIRE(error == ERR_NONE);
        BOOST_CHECK_EQUAL(no_of_openings, 1);
        error = swmm_getOpeningCouplingType(node_ind, 0, &coupling);
        BOOST_REQUIRE(error == ERR_NONE);
        BOOST_CHECK_EQUAL(coupling, NO_COUPLING_FLOW);
        error = swmm_getNodeOpeningFlow(node_ind, 0, &NodeOpeningFlowInflow);
        BOOST_REQUIRE(error == ERR_NONE);
        BOOST_CHECK_EQUAL(NodeOpeningFlowInflow, 0.0);
        

        // Reset area and width of the first opening
        area = 25.0;
        width = 1.0;
        error = swmm_setNodeOpening(node_ind, 0, 0, area, width, 0.167, 0.54, 0.056);
        BOOST_CHECK_EQUAL(error, ERR_NONE);
        // Add a second opening, with different area and width
        area = 10.0;
        width = 5.0;
        error = swmm_setNodeOpening(node_ind, 1, 0, area, width, 0.167, 0.54, 0.056);
        BOOST_CHECK_EQUAL(error, ERR_NONE);
        // Two openings added
        error = swmm_getOpeningsNum(node_ind, &no_of_openings);
        BOOST_REQUIRE(error == ERR_NONE);
        BOOST_CHECK_EQUAL(no_of_openings, 2);

        // Reset area and width of the first opening
        area = 10.0;
        width = 5.0;
        error = swmm_setNodeOpening(node_ind, 0, 0, area, width, 0.167, 0.54, 0.056);
        BOOST_CHECK_EQUAL(error, ERR_NONE);
        // Change area and width of the second opening, to be different test from case 6
        area = 25.0;
        width = 1.0;
        error = swmm_setNodeOpening(node_ind, 1, 0, area, width, 0.167, 0.54, 0.056);
        BOOST_CHECK_EQUAL(error, ERR_NONE);

        //Close the second opening
        error = swmm_closeOpening(node_ind, 1);
        BOOST_CHECK_EQUAL(error, ERR_NONE);
        // Reset area and width
        area = 25.0;
        width = 1.0;
        error = swmm_setNodeOpening(node_ind, 0, 0, area, width, 0.167, 0.54, 0.056);
        BOOST_CHECK_EQUAL(error, ERR_NONE);
        // There are two openings
        error = swmm_getOpeningsNum(node_ind, &no_of_openings);
        BOOST_CHECK_EQUAL(no_of_openings, 2);

        // Reopen the second opening
        error = swmm_openOpening(node_ind, 1);
        BOOST_CHECK_EQUAL(error, ERR_NONE);
        // Reset area and width of the second opening
        area = 10.0;
        width = 5.0;
        error = swmm_setNodeOpening(node_ind, 1, 0, area, width, 0.167, 0.54, 0.056);
        BOOST_CHECK_EQUAL(error, ERR_NONE);

        // Add a third opening to Node[node_ind]
        error = swmm_setNodeOpening(node_ind, 2, 0, area, width, 0.167, 0.54, 0.056);
        BOOST_CHECK_EQUAL(error, ERR_NONE);
        // Three openings added
        error = swmm_getOpeningsNum(node_ind, &no_of_openings);
        BOOST_CHECK_EQUAL(no_of_openings, 3);
        // Delete the second opening
        error = swmm_deleteNodeOpening(node_ind, 1);
        BOOST_CHECK_EQUAL(error, ERR_NONE);
        error = swmm_getOpeningsNum(node_ind, &no_of_openings);
        BOOST_CHECK_EQUAL(error, ERR_NONE);
        // Two remaininig openings
        error = swmm_getOpeningsNum(node_ind, &no_of_openings);
        BOOST_CHECK_EQUAL(no_of_openings, 2);
        // Delete first opening
        error = swmm_deleteNodeOpening(node_ind, 0);
        BOOST_CHECK_EQUAL(error, ERR_NONE);
        // One remaininig opening
        error = swmm_getOpeningsNum(node_ind, &no_of_openings);
        BOOST_CHECK_EQUAL(no_of_openings, 1);
        // Delete the remaininig opening
        error = swmm_deleteNodeOpening(node_ind, 0);
        // No remaininig opening
        error = swmm_getOpeningsNum(node_ind, &no_of_openings);
        BOOST_CHECK_EQUAL(no_of_openings, 0);
        // Try to delete with no remaininig opening
        no_of_deleted_openings = swmm_deleteNodeOpening(node_ind, 0);
        BOOST_CHECK_EQUAL(error, ERR_NONE);
        BOOST_CHECK_EQUAL(no_of_deleted_openings, 0);

        // Add a first opening to Node[node_ind]
        area = 25.0;
        width = 1.0;
        error = swmm_setNodeOpening(node_ind, 0, 0, area, width, 0.167, 0.54, 0.056);
        BOOST_CHECK_EQUAL(error, ERR_NONE);
        // One opening added
        error = swmm_getOpeningsNum(node_ind, &no_of_openings);
        BOOST_CHECK_EQUAL(no_of_openings, 1);
        // Add a second opening, with different area and width
        area = 10.0;
        width = 5.0;
        error = swmm_setNodeOpening(node_ind, 1, 0, area, width, 0.167, 0.54, 0.056);
        BOOST_CHECK_EQUAL(error, ERR_NONE);
        // Two openings added
        error = swmm_getOpeningsNum(node_ind, &no_of_openings);
        BOOST_CHECK_EQUAL(no_of_openings, 2);
        // Delete openings
        error = swmm_deleteNodeOpenings(node_ind);
        BOOST_CHECK_EQUAL(error, ERR_NONE);
        // Check if there are remaining openings
        error = swmm_getOpeningsNum(node_ind, &no_of_openings);
        BOOST_CHECK_EQUAL(no_of_openings, 0);

    }

BOOST_AUTO_TEST_SUITE_END()
