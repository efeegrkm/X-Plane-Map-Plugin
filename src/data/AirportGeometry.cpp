// AirportGeometry.cpp
#include "data/AirportGeometry.h"

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
