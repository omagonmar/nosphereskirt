/*//////////////////////////////////////////////////////////////////
////     The SKIRT project -- advanced radiative transfer       ////
////       © Astronomical Observatory, Ghent University         ////
///////////////////////////////////////////////////////////////// */

#include "NoSpherePolarizedSilicateGrainComposition.hpp"
#include "DraineSilicateGrainComposition.hpp"

//////////////////////////////////////////////////////////////////////

void NoSpherePolarizedSilicateGrainComposition::setupSelfBefore()
{
    GrainComposition::setupSelfBefore();

    setBulkDensity(3.0e3);
    calculateEnthalpyGrid(DraineSilicateGrainComposition::enthalpyFunction);
    loadPolarizedOpticalGrid(true, "NoSphereSilicate_STOKES_Sxx.DAT");
}

//////////////////////////////////////////////////////////////////////

string NoSpherePolarizedSilicateGrainComposition::name() const
{
    return "NoSphere_Polarized_Draine_Silicate";
}

//////////////////////////////////////////////////////////////////////
