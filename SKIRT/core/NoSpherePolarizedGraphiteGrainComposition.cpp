/*//////////////////////////////////////////////////////////////////
////     The SKIRT project -- advanced radiative transfer       ////
////       © Astronomical Observatory, Ghent University         ////
///////////////////////////////////////////////////////////////// */

#include "NoSpherePolarizedGraphiteGrainComposition.hpp"
#include "DraineGraphiteGrainComposition.hpp"

//////////////////////////////////////////////////////////////////////

void NoSpherePolarizedGraphiteGrainComposition::setupSelfBefore()
{
    GrainComposition::setupSelfBefore();

    setBulkDensity(2.24e3);
    calculateEnthalpyGrid(DraineGraphiteGrainComposition::enthalpyFunction);
    loadPolarizedOpticalGrid(true, "NoSphereGraphite_STOKES_Sxx_001.DAT");
}

//////////////////////////////////////////////////////////////////////

string NoSpherePolarizedGraphiteGrainComposition::name() const
{
    return "NoSphere_Polarized_Draine_Graphite";
}

//////////////////////////////////////////////////////////////////////
