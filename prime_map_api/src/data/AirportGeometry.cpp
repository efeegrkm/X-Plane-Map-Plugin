// AirportGeometry.cpp
#include "data/AirportGeometry.h"

const char* SurfaceName(int pCode)
{
    switch (pCode)
    {
        case 2:
            return "concrete";
        case 3:
            return "grass";
        case 4:
            return "dirt";
        case 5:
            return "gravel";
        case 12:
            return "dry lakebed";
        case 13:
            return "water";
        case 14:
            return "snow / ice";
        case 15:
            return "transparent";
        default:
            break;
    }
    if (pCode >= 50 && pCode <= 57)
    {
        return "concrete";  // X-Plane 12 beton turleri
    }
    return "asphalt";  // 1 ve X-Plane 12 asfalt turleri
}

RunwaySurface SurfaceFromAptCode(int pCode)
{
    switch (pCode)
    {
        case 3:   // cim
        case 4:   // toprak
        case 5:   // cakil
        case 12:  // kuru gol yatagi
            return RunwaySurface::Unpaved;
        case 13:
            return RunwaySurface::Water;
        case 14:
            return RunwaySurface::Snow;
        case 15:
            return RunwaySurface::Transparent;
        default:
            return RunwaySurface::Paved;  // 1, 2 ve XP12'nin asfalt/beton turleri
    }
}
