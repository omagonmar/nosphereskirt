/*//////////////////////////////////////////////////////////////////
////     The SKIRT project -- advanced radiative transfer       ////
////       © Astronomical Observatory, Ghent University         ////
///////////////////////////////////////////////////////////////// */

#include "SpheGeometry.hpp"
#include "Random.hpp"

//////////////////////////////////////////////////////////////////////

int SpheGeometry::dimension() const
{
    return 1;
}

//////////////////////////////////////////////////////////////////////

double SpheGeometry::density(Position bfr) const
{
    return density(bfr.radius());
}

//////////////////////////////////////////////////////////////////////

Position SpheGeometry::generatePosition() const
{
    // generate the random numbers in separate statements to guarantee evaluation order
    // (function arguments are evaluated in different order depending on the compiler)
    double r = randomRadius();
    Direction bfk = random()->direction();
    return Position(r,bfk);
}

//////////////////////////////////////////////////////////////////////

double SpheGeometry::SigmaX() const
{
    return 2.0*Sigmar();
}

//////////////////////////////////////////////////////////////////////

double SpheGeometry::SigmaY() const
{
    return 2.0*Sigmar();
}

//////////////////////////////////////////////////////////////////////

double SpheGeometry::SigmaZ() const
{
    return 2.0*Sigmar();
}

//////////////////////////////////////////////////////////////////////
