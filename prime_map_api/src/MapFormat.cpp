// MapFormat.cpp
#include "MapFormat.h"

#include <array>
#include <cmath>
#include <format>

namespace
{

// Izgara araliklari (derece), kucukten buyuge
const auto kGridSteps = std::to_array<double>({
    0.5 / 60.0, 1.0 / 60.0, 2.0 / 60.0, 5.0 / 60.0, 10.0 / 60.0, 20.0 / 60.0, 30.0 / 60.0,
    1.0,        2.0,        5.0,        10.0,       20.0,        30.0,
});

}  // namespace

std::string FormatLatLon(const LatLon& pPoint)
{
    return std::format("{:.5f} {}  {:.5f} {}", std::fabs(pPoint.mLat), pPoint.mLat >= 0 ? 'N' : 'S',
                       std::fabs(pPoint.mLon), pPoint.mLon >= 0 ? 'E' : 'W');
}

std::string FormatGridLabel(double pDegrees, bool pIsLatitude, double pStepDegrees)
{
    char tHemisphere;
    if (pIsLatitude)
    {
        tHemisphere = pDegrees >= 0 ? 'N' : 'S';
    }
    else
    {
        tHemisphere = pDegrees >= 0 ? 'E' : 'W';
    }

    // Dakikanin 1/10'una yuvarla (kayan nokta hatasi 29.9999 gibi gorunmesin)
    const double tTenthsOfMinute = std::floor(std::fabs(pDegrees) * 600.0 + 0.5);
    const int tWholeDegrees = static_cast<int>(tTenthsOfMinute / 600.0);
    const double tMinutes = (tTenthsOfMinute - tWholeDegrees * 600.0) / 10.0;

    if (pStepDegrees >= 1.0 - 1e-9)
    {
        return std::format("{}{}", tWholeDegrees, tHemisphere);
    }
    if (pStepDegrees >= 1.0 / 60.0 - 1e-9)
    {
        return std::format("{} {:02d}{}", tWholeDegrees, static_cast<int>(tMinutes + 0.5), tHemisphere);
    }
    return std::format("{} {:04.1f}{}", tWholeDegrees, tMinutes, tHemisphere);
}

double ChooseGridStepDeg(double pBoxelsPerDegree, double pMinSpacing)
{
    for (double tStep : kGridSteps)
    {
        if (tStep * pBoxelsPerDegree >= pMinSpacing)
        {
            return tStep;
        }
    }
    return 30.0;
}

double ChooseScaleBarNm(double pNmPerBoxel, double pMaxBoxels)
{
    if (pNmPerBoxel <= 0.0)
    {
        return 0.0;
    }
    const double tMaxNm = pNmPerBoxel * pMaxBoxels;
    // tMaxNm'den kucuk en buyuk 1/2/5 x 10^n
    const double tPower = std::pow(10.0, std::floor(std::log10(tMaxNm)));
    const std::array<double, 3> tMultipliers = {5.0, 2.0, 1.0};
    for (double tMultiplier : tMultipliers)
    {
        if (tMultiplier * tPower <= tMaxNm)
        {
            return tMultiplier * tPower;
        }
    }
    return tPower;
}
