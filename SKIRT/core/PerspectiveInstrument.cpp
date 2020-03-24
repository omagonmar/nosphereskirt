/*//////////////////////////////////////////////////////////////////
////     The SKIRT project -- advanced radiative transfer       ////
////       © Astronomical Observatory, Ghent University         ////
///////////////////////////////////////////////////////////////// */

#include "PerspectiveInstrument.hpp"
#include "FatalError.hpp"
#include "FITSInOut.hpp"
#include "LockFree.hpp"
#include "Log.hpp"
#include "PhotonPackage.hpp"
#include "Units.hpp"
#include "WavelengthGrid.hpp"

////////////////////////////////////////////////////////////////////

namespace
{
    double norm(double x, double y, double z)
    {
        return sqrt(x*x + y*y + z*z);
    }
}

////////////////////////////////////////////////////////////////////

void PerspectiveInstrument::setupSelfBefore()
{
    Instrument::setupSelfBefore();

    // verify attribute values
    if (_Ux == 0 && _Uy == 0 && _Uz == 0) throw FATALERROR("Upwards direction cannot be null vector");

    // unit vector in direction from crosshair position to viewport origin
    double G = norm(_Vx - _Cx, _Vy - _Cy, _Vz - _Cz);
    if (G < 1e-20) throw FATALERROR("Crosshair is too close to viewport origin");
    double a = (_Vx - _Cx) / G;
    double b = (_Vy - _Cy) / G;
    double c = (_Vz - _Cz) / G;

    // pixel width and height (assuming square pixels)
    _s = _Sx/_Nx;

    // eye position
    _Ex = _Vx + _Fe * a;
    _Ey = _Vy + _Fe * b;
    _Ez = _Vz + _Fe * c;

    // unit vectors along viewport's x and y axes
    Vec kn(_Vx-_Cx, _Vy-_Cy, _Vz-_Cz);
    Vec ku(_Ux, _Uy, _Uz);
    Vec ky = Vec::cross(kn,Vec::cross(ku,kn));
    Vec kx = Vec::cross(ky,kn);
    _bfkx = Direction(kx/kx.norm());
    _bfky = Direction(ky/ky.norm());

    // the perspective transformation

    // from world to eye coordinates
    _transform.translate(-_Ex, -_Ey, -_Ez);
    double v = sqrt(b*b+c*c);
    if (v > 0.3)
    {
        _transform.rotateX(c/v, -b/v);
        _transform.rotateY(v, -a);
        double k = (b*b+c*c)*_Ux - a*b*_Uy - a*c*_Uz;
        double l = c*_Uy - b*_Uz;
        double u = sqrt(k*k+l*l);
        _transform.rotateZ(l/u, -k/u);
    }
    else
    {
        v = sqrt(a*a+c*c);
        _transform.rotateY(c/v, -a/v);
        _transform.rotateX(v, -b);
        double k = c*_Ux - a*_Uz;
        double l = (a*a+c*c)*_Uy - a*b*_Ux - b*c*_Uz;
        double u = sqrt(k*k+l*l);
        _transform.rotateZ(l/u, -k/u);
    }
    _transform.scale(1., 1., -1.);

    // from eye to viewport coordinates
    _transform.perspectiveZ(_Fe);

    // from viewport to pixel coordinates
    _transform.scale(1./_s, 1./_s, 1.);
    _transform.translate(_Nx/2., _Ny/2., 0);

    // the data cube
    _ftotv.initialize("Instrument " + instrumentName() + " total flux", _Nx*_Ny, this);
}

////////////////////////////////////////////////////////////////////

Direction PerspectiveInstrument::bfkobs(const Position& bfr) const
{
    // distance from launch to eye
    double Px, Py, Pz;
    bfr.cartesian(Px, Py, Pz);
    double D = norm(_Ex - Px, _Ey - Py, _Ez - Pz);

    // if the distance is very small, return something silly - the photon package is behind the viewport anyway
    if (D < 1e-20) return Direction();

    // otherwise return a unit vector in the direction from launch to eye
    return Direction((_Ex - Px)/D, (_Ey - Py)/D, (_Ez - Pz)/D);
}

////////////////////////////////////////////////////////////////////

Direction PerspectiveInstrument::bfkx() const
{
    return _bfkx;
}

////////////////////////////////////////////////////////////////////

Direction PerspectiveInstrument::bfky() const
{
    return _bfky;
}

////////////////////////////////////////////////////////////////////

void PerspectiveInstrument::detect(PhotonPackage* pp)
{
    // get the position
    double x, y, z;
    pp->position().cartesian(x,y,z);

    // transform from world to pixel coordinates
    double xp, yp, zp, wp;
    _transform.transform(x, y, z, 1.,  xp, yp, zp, wp);
    int i = static_cast<int>(xp/wp);
    int j = static_cast<int>(yp/wp);

    // ignore photon packages arriving outside the viewport, originating from behind the viewport,
    // or originating from very close to the viewport
    if (i>=0 && i<_Nx && j>=0 && j<_Ny && zp > _s/10.)
    {
        double d = zp;

        // determine the photon package's luminosity, attenuated for the absorption along its path to the instrument
        double taupath = opticalDepth(pp,d);
        double L = pp->luminosity() * exp(-taupath);

        // adjust the luminosity for the distance from the launch position to the instrument
        double r = _s / (2.*d);
        double rar = r / atan(r);
        L *= rar*rar;

        // add the adjusted luminosity to the appropriate pixel in the data cube
        int ell = pp->ell();
        int l = i + _Nx*j;
        LockFree::add(_ftotv(ell,l), L);
    }
}

////////////////////////////////////////////////////////////////////

void PerspectiveInstrument::write()
{
    Units* units = find<Units>();
    WavelengthGrid* lambdagrid = find<WavelengthGrid>();
    int Nlambda = find<WavelengthGrid>()->numWavelengths();

    // Collect the partial data cubes into one big cube at process 0
    std::shared_ptr<Array> completeCube = _ftotv.constructCompleteCube();

    // Multiply each sample by lambda/dlamdba and by the constant factor 1/(4 pi s^2)
    // to obtain the surface brightness and convert to output units (such as W/m2/arcsec2)
    double front = 1. / (4.*M_PI*_s*_s);
    for (size_t m = 0; m < completeCube->size(); m++)
    {
        size_t ell = m / (_Nx*_Ny);
        double lambda = lambdagrid->lambda(ell);
        double dlambda = lambdagrid->dlambda(ell);
        (*completeCube)[m] = units->osurfacebrightness(lambda, (*completeCube)[m]*front/dlambda);
    }

    // Write a FITS file containing the data cube
    string filename = instrumentName() + "_total";
    string description = "total flux";
    FITSInOut::write(this, description, filename, *completeCube, _Nx, _Ny, Nlambda,
                     units->olength(_s), units->olength(_s), 0., 0.,
                     units->usurfacebrightness(), units->ulength());
}

////////////////////////////////////////////////////////////////////
