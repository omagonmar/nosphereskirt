/*//////////////////////////////////////////////////////////////////
////     The SKIRT project -- advanced radiative transfer       ////
////       © Astronomical Observatory, Ghent University         ////
///////////////////////////////////////////////////////////////// */

#include "SPHDustDistribution.hpp"
#include "Constants.hpp"
#include "FatalError.hpp"
#include "FilePaths.hpp"
#include "Log.hpp"
#include "NR.hpp"
#include "Random.hpp"
#include "StringUtils.hpp"
#include "SPHGasParticleGrid.hpp"
#include "TextInFile.hpp"

////////////////////////////////////////////////////////////////////

SPHDustDistribution::~SPHDustDistribution()
{
    delete _grid;
}

//////////////////////////////////////////////////////////////////////

void SPHDustDistribution::setupSelfBefore()
{
    DustDistribution::setupSelfBefore();

    // get conversion factors for units
    const double pc = Constants::pc();
    const double Msun = Constants::Msun();

    // load the SPH gas particles
    TextInFile infile(this, _filename, "SPH gas particles");
    int Nignored = 0;
    double Mtot = 0;
    double Mmetal = 0;
    double x, y, z, h, M, Z, T;
    while (infile.readRow(1, x, y, z, h, M, Z, T))
    {
        // ignore particle if the temperature is higher than the maximum (assuming both T and Tmax are valid)
        if (T > 0 && _maxTemperature > 0 && T > _maxTemperature)
        {
            Nignored++;
        }
        else
        {
            // add a particle
            _pv.push_back(SPHGasParticle(Vec(x, y,z)*pc, h*pc, M*Msun, Z));
            Mtot += M;
            Mmetal += M * Z;

            // remember whether there are any negative masses
            if (M<0) _negativeMasses = true;
        }
    }

    // if the total cold and/or metallic gas mass is negative, suppress the complete dust distribution
    if (Mtot<0 || Mmetal<0)
    {
        find<Log>()->warning("  Total cold and/or metallic gas mass is negative; suppressing all dust");
        _pv.clear();
        Mtot = 0;
        Mmetal = 0;
    }

    // show some statistics
    find<Log>()->info("  Number of high-temperature particles ignored: " + std::to_string(Nignored));
    find<Log>()->info("  Number of SPH gas particles containing dust: " + std::to_string(_pv.size()));
    find<Log>()->info("  Total gas mass: " + std::to_string(Mtot) + " Msun");
    find<Log>()->info("  Total metal mass: " + StringUtils::toString(Mmetal) + " Msun");

    // construct a 3D-grid over the particle space, and create a list of particles that overlap each grid cell
    const int GRIDSIZE = 20;
    string size = std::to_string(GRIDSIZE);
    find<Log>()->info("Constructing intermediate " + size + "x" + size + "x" + size + " grid for particles...");
    _grid = new SPHGasParticleGrid(_pv, GRIDSIZE);
    find<Log>()->info("  Smallest number of particles per cell: " + std::to_string(_grid->minParticlesPerCell()));
    find<Log>()->info("  Largest  number of particles per cell: " + std::to_string(_grid->maxParticlesPerCell()));
    find<Log>()->info("  Average  number of particles per cell: "
                      + StringUtils::toString(_grid->totalParticles() / double(GRIDSIZE*GRIDSIZE*GRIDSIZE),'f',1));

    // construct a vector with the normalized cumulative particle densities
    NR::cdf(_cumrhov, _pv.size(), [this](int i){return _pv[i].metalMass();} );
}

//////////////////////////////////////////////////////////////////////

int SPHDustDistribution::dimension() const
{
    return 3;
}

//////////////////////////////////////////////////////////////////////

int SPHDustDistribution::numComponents() const
{
    return 1;
}

//////////////////////////////////////////////////////////////////////

DustMix* SPHDustDistribution::mix(int h) const
{
    if (h!=0) throw FATALERROR("Wrong value for h (" + std::to_string(h) + ")");
    return _dustMix;
}

//////////////////////////////////////////////////////////////////////

double SPHDustDistribution::density(int h, Position bfr) const
{
    if (h!=0) throw FATALERROR("Wrong value for h (" + std::to_string(h) + ")");
    return density(bfr);
}

//////////////////////////////////////////////////////////////////////

double SPHDustDistribution::density(Position bfr) const
{
    const vector<const SPHGasParticle*>& particles = _grid->particlesFor(bfr);

    double sum = 0.0;
    int n = particles.size();
    for (int i=0; i<n; i++)
        sum += particles[i]->metalDensity(bfr);  // sum contains the total density in metals
    sum *= _dustFraction;    // sum now contains the total density in metals locked up in dust grains
    return max(sum,0.);  // guard against negative dust masses
}

//////////////////////////////////////////////////////////////////////

Position SPHDustDistribution::generatePosition() const
{
    Random* random = find<Random>();
    int i = NR::locateClip(_cumrhov, random->uniform());
    double x = random->gauss();
    double y = random->gauss();
    double z = random->gauss();
    return Position( _pv[i].center() + Vec(x,y,z) * (_pv[i].radius() / 2.42 / M_SQRT2) );
}

//////////////////////////////////////////////////////////////////////

double SPHDustDistribution::massInBox(int h, const Box& box) const
{
    if (h!=0) throw FATALERROR("Wrong value for h (" + std::to_string(h) + ")");
    return massInBox(box);
}

//////////////////////////////////////////////////////////////////////

double SPHDustDistribution::massInBox(const Box& box) const
{
    const vector<const SPHGasParticle*>& particles = _grid->particlesFor(box);

    double sum = 0.0;
    int n = particles.size();
    for (int i=0; i<n; i++)
        sum += particles[i]->metalMassInBox(box);  // total mass in metals
    sum *= _dustFraction;    // total mass in metals locked up in dust grains
    return max(sum,0.);  // guard against negative dust masses
}

//////////////////////////////////////////////////////////////////////

double SPHDustDistribution::mass(int h) const
{
    if (h!=0) throw FATALERROR("Wrong value for h (" + std::to_string(h) + ")");
    return mass();
}

//////////////////////////////////////////////////////////////////////

double SPHDustDistribution::mass() const
{
    double sum = 0.0;
    int n = _pv.size();
    for (int i=0; i<n; i++)
        sum += _pv[i].metalMass();  // sum contains the total mass in metals
    sum *= _dustFraction;    // sum now contains the total mass in metals locked up in dust grains
    return max(sum,0.);  // guard against negative dust masses
}

//////////////////////////////////////////////////////////////////////

double SPHDustDistribution::SigmaX() const
{
    const int NSAMPLES = 10000;
    double sum = 0;
    double xmin = _grid->xmin();
    double xmax = _grid->xmax();
    for (int k = 0; k < NSAMPLES; k++)
    {
        sum += density(Position(xmin + k*(xmax-xmin)/NSAMPLES, 0, 0));
    }
    return (sum/NSAMPLES)*(xmax-xmin);
}

//////////////////////////////////////////////////////////////////////

double SPHDustDistribution::SigmaY() const
{
    const int NSAMPLES = 10000;
    double sum = 0;
    double ymin = _grid->ymin();
    double ymax = _grid->ymax();
    for (int k = 0; k < NSAMPLES; k++)
    {
        sum += density(Position(0, ymin + k*(ymax-ymin)/NSAMPLES, 0));
    }
    return (sum/NSAMPLES)*(ymax-ymin);
}

//////////////////////////////////////////////////////////////////////

double SPHDustDistribution::SigmaZ() const
{
    const int NSAMPLES = 10000;
    double sum = 0;
    double zmin = _grid->zmin();
    double zmax = _grid->zmax();
    for (int k = 0; k < NSAMPLES; k++)
    {
        sum += density(Position(0, 0, zmin + k*(zmax-zmin)/NSAMPLES));
    }
    return (sum/NSAMPLES)*(zmax-zmin);
}

//////////////////////////////////////////////////////////////////////

int SPHDustDistribution::numParticles() const
{
    return _pv.size();
}

//////////////////////////////////////////////////////////////////////

Vec SPHDustDistribution::particleCenter(int index) const
{
    int n = _pv.size();
    if (index<0 || index>=n) throw FATALERROR("Particle index out of range: " + std::to_string(index));

    return _pv[index].center();
}

//////////////////////////////////////////////////////////////////////

vector<SimulationItem*> SPHDustDistribution::interfaceCandidates(const std::type_info& interfaceTypeInfo)
{
    if (interfaceTypeInfo == typeid(DustMassInBoxInterface) && _negativeMasses)
        return vector<SimulationItem*>();
    return DustDistribution::interfaceCandidates(interfaceTypeInfo);
}

//////////////////////////////////////////////////////////////////////
