#include "data/AirportGeometry.h"

const char* surfaceName(int pCode)
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
        return "concrete";
    }
    return "asphalt";
}

RunwaySurface surfaceFromAptCode(int pCode)
{
    switch (pCode)
    {
        case 3:
        case 4:
        case 5:
        case 12:
            return RunwaySurface::Unpaved;
        case 13:
            return RunwaySurface::Water;
        case 14:
            return RunwaySurface::Snow;
        case 15:
            return RunwaySurface::Transparent;
        default:
            return RunwaySurface::Paved;
    }
}
