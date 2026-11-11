// TerrainRelief.cpp
#include "data/TerrainRelief.h"

#include <algorithm>
#include <cmath>

namespace
{

const double kPi = 3.14159265358979323846;
const double kSunAltitudeDeg = 45.0;
const double kShadeGain = 1.8;          // isik farkinin parlakliga cevrimi (0.15'teki formulle ayni)
const double kMinFactor = 0.45;
const double kMaxFactor = 1.55;
const double kSlopeToneHalf = 0.5;      // egim bu degerdeyken (~27 derece) slope_tone'un yarisi uygulanir
const double kFeetPerMeter = 3.28084;
const double kNoDepthSeaLevel = -10.0;  // GERCEK VERI: verisi olmayan deniz hucresi -10
const double kContourHalfWidth = 0.6;   // kontur cizgisinin yari genisligi (piksel)
const double kContourDarkening = 0.55;  // tam guclu konturda renk bu oranda koyulasir

struct Light
{
    double mAzimuthDeg;  // kuzeyden saat yonunde (315 = kuzeybati)
    double mWeight;
};

const Light kMultiLights[] = {{225.0, 0.10}, {270.0, 0.25}, {315.0, 0.40}, {360.0, 0.25}};
const Light kSingleLight[] = {{315.0, 1.0}};

}  // namespace

double ReliefExaggeration(const TerrainReliefParams& pParams, double pCellMeters, double pFinestCellMeters)
{
    if (pFinestCellMeters <= 0.0 || pCellMeters <= pFinestCellMeters)
    {
        return pParams.mExaggeration;
    }
    return pParams.mExaggeration * std::pow(pCellMeters / pFinestCellMeters, pParams.mCoarseBoost);
}

double ReliefFactor(double pSlopeEast, double pSlopeNorth, double pStrength, double pExaggeration,
                    const TerrainReliefParams& pParams)
{
    // Duz arazi esigi: DEM tam metre saklanir; neredeyse duz ovada 1 m'lik basamaklar (~%1 egim)
    // abartmayla cizgi cizgi dalga gibi gorunuyordu (onizleme, Gustavus). Kucuk egim yumusakca bastirilir:
    // s -> s * s^2 / (s^2 + s0^2). s0 = %2 iken (olculdu): %1,1 basamak etkisi %23'e iner, %5 yamac
    // %86'sini, %15 ve ustu tamamini korur.
    double tSlopeEast = pSlopeEast;
    double tSlopeNorth = pSlopeNorth;
    const double tSlopeSquared = pSlopeEast * pSlopeEast + pSlopeNorth * pSlopeNorth;
    if (pParams.mFlatSlope > 0.0)
    {
        const double tKeep = tSlopeSquared / (tSlopeSquared + pParams.mFlatSlope * pParams.mFlatSlope);
        tSlopeEast *= tKeep;
        tSlopeNorth *= tKeep;
    }

    // Yuzey normali (-dz/dx, -dz/dy, 1) / uzunluk; x dogu, y kuzey
    const double tDzDx = tSlopeEast * pExaggeration;
    const double tDzDy = tSlopeNorth * pExaggeration;
    const double tLength = std::sqrt(tDzDx * tDzDx + tDzDy * tDzDy + 1.0);
    const double tAltitude = kSunAltitudeDeg * kPi / 180.0;
    const double tSunZ = std::sin(tAltitude);

    const Light* tLights = pParams.mMultiDirectional ? kMultiLights : kSingleLight;
    const int tCount = pParams.mMultiDirectional ? 4 : 1;
    double tLight = 0.0;
    for (int tIndex = 0; tIndex < tCount; ++tIndex)
    {
        const double tAzimuth = tLights[tIndex].mAzimuthDeg * kPi / 180.0;
        const double tSunX = std::sin(tAzimuth) * std::cos(tAltitude);  // dogu bileseni
        const double tSunY = std::cos(tAzimuth) * std::cos(tAltitude);  // kuzey bileseni
        const double tDot = (-tDzDx * tSunX - tDzDy * tSunY + tSunZ) / tLength;
        tLight += tLights[tIndex].mWeight * std::max(0.0, tDot);
    }
    // Duz arazinin isigi (agirliklar toplami 1): sin(yukseklik). Fark parlakliga cevrilir.
    double tFactor = 1.0 + (tLight - tSunZ) * kShadeGain * pStrength;

    // Egim tonu: dik yamac isiktan bagimsiz biraz koyu (daglar duz renk gibi durmasin)
    const double tSlope = std::sqrt(tSlopeEast * tSlopeEast + tSlopeNorth * tSlopeNorth);
    tFactor *= 1.0 - pParams.mSlopeTone * tSlope / (tSlope + kSlopeToneHalf);
    return std::clamp(tFactor, kMinFactor, kMaxFactor);
}

void VfrBandColor(double pMeters, double pFeetPerPixel, const TerrainVfrParams& pParams, double pRgb[3])
{
    const double tFeet = pMeters * kFeetPerMeter;
    int tBand = 0;
    while (tBand + 1 < kVfrBandCount && tFeet >= pParams.mBandFeet[tBand + 1])
    {
        ++tBand;
    }
    auto tSet = [&](int pBand, double pWeight)
    {
        const TerrainColor& tColor = pParams.mBandColors[pBand];
        pRgb[0] += tColor.mR * pWeight;
        pRgb[1] += tColor.mG * pWeight;
        pRgb[2] += tColor.mB * pWeight;
    };
    pRgb[0] = 0.0;
    pRgb[1] = 0.0;
    pRgb[2] = 0.0;
    if (pFeetPerPixel <= 1e-9)
    {
        tSet(tBand, 1.0);  // egim yok / bilinmiyor: keskin bant
        return;
    }
    // Alt ve ust sinira uzaklik (piksel). Ilk bandin alt siniri yok (deniz seviyesi alti da ilk bant).
    const double tToLower = tBand > 0 ? (tFeet - pParams.mBandFeet[tBand]) / pFeetPerPixel : 1e9;
    const double tToUpper =
        tBand + 1 < kVfrBandCount ? (pParams.mBandFeet[tBand + 1] - tFeet) / pFeetPerPixel : 1e9;
    double tOwn = 1.0;
    if (tToLower < 0.5)
    {
        tOwn = 0.5 + tToLower;  // sinirda 0.5: iki bant esit karisir
        tSet(tBand - 1, 1.0 - tOwn);
    }
    else if (tToUpper < 0.5)
    {
        tOwn = 0.5 + tToUpper;
        tSet(tBand + 1, 1.0 - tOwn);
    }
    tSet(tBand, tOwn);

    if (pParams.mContourLines)
    {
        const double tNearest = std::min(tToLower, tToUpper);
        const double tAlpha = std::clamp(1.0 - tNearest / kContourHalfWidth, 0.0, 1.0) * pParams.mContourStrength;
        for (int tChannel = 0; tChannel < 3; ++tChannel)
        {
            pRgb[tChannel] *= 1.0 - tAlpha * kContourDarkening;
        }
    }
}

double DepthDarkening(double pSeaLevel, const TerrainWaterParams& pParams)
{
    if (pParams.mDepthTone <= 0.0 || pSeaLevel >= kNoDepthSeaLevel)
    {
        return 0.0;
    }
    const double tDepth = -pSeaLevel;
    const double tT = (tDepth + kNoDepthSeaLevel) / (pParams.mDepthFullMeters + kNoDepthSeaLevel);
    return pParams.mDepthTone * std::clamp(tT, 0.0, 1.0);
}

double CoastBandLightening(double pDistancePixels, double pBandPixels, const TerrainWaterParams& pParams)
{
    // Serit kiyi cizgisinin (1 piksel) arkasindan baslar: cizginin hemen yani acilirsa kiyi
    // kontrastini kaybeder, kucuk goller bulanik gorunur (onizlemede goruldu)
    const double tFromLine = pDistancePixels - 1.0;
    if (pBandPixels < 0.5 || tFromLine >= pBandPixels || pParams.mBandStrength <= 0.0)
    {
        return 0.0;
    }
    const double tNear = 1.0 - std::max(0.0, tFromLine) / pBandPixels;
    return pParams.mBandStrength * tNear * tNear;
}

void DistanceToLand(const std::vector<uint8_t>& pLand, int pSize, float pMaxDistance, std::vector<float>* pOut)
{
    // 3-4 chamfer: dik komsu 3, capraz komsu 4 birim; iki gecis (ileri, geri). Sonuc / 3 = piksel.
    const float tInfinity = pMaxDistance * 3.0f + 4.0f;
    const size_t tCount = static_cast<size_t>(pSize) * pSize;
    std::vector<float> tUnits(tCount);
    for (size_t tIndex = 0; tIndex < tCount; ++tIndex)
    {
        tUnits[tIndex] = pLand[tIndex] ? 0.0f : tInfinity;
    }
    auto tAt = [&](int pColumn, int pRow) -> float&
    {
        return tUnits[static_cast<size_t>(pRow) * pSize + pColumn];
    };
    for (int tRow = 0; tRow < pSize; ++tRow)
    {
        for (int tColumn = 0; tColumn < pSize; ++tColumn)
        {
            float& tValue = tAt(tColumn, tRow);
            if (tColumn > 0)
            {
                tValue = std::min(tValue, tAt(tColumn - 1, tRow) + 3.0f);
            }
            if (tRow > 0)
            {
                tValue = std::min(tValue, tAt(tColumn, tRow - 1) + 3.0f);
                if (tColumn > 0)
                {
                    tValue = std::min(tValue, tAt(tColumn - 1, tRow - 1) + 4.0f);
                }
                if (tColumn + 1 < pSize)
                {
                    tValue = std::min(tValue, tAt(tColumn + 1, tRow - 1) + 4.0f);
                }
            }
        }
    }
    for (int tRow = pSize - 1; tRow >= 0; --tRow)
    {
        for (int tColumn = pSize - 1; tColumn >= 0; --tColumn)
        {
            float& tValue = tAt(tColumn, tRow);
            if (tColumn + 1 < pSize)
            {
                tValue = std::min(tValue, tAt(tColumn + 1, tRow) + 3.0f);
            }
            if (tRow + 1 < pSize)
            {
                tValue = std::min(tValue, tAt(tColumn, tRow + 1) + 3.0f);
                if (tColumn + 1 < pSize)
                {
                    tValue = std::min(tValue, tAt(tColumn + 1, tRow + 1) + 4.0f);
                }
                if (tColumn > 0)
                {
                    tValue = std::min(tValue, tAt(tColumn - 1, tRow + 1) + 4.0f);
                }
            }
        }
    }
    pOut->resize(tCount);
    for (size_t tIndex = 0; tIndex < tCount; ++tIndex)
    {
        (*pOut)[tIndex] = std::min(tUnits[tIndex] / 3.0f, pMaxDistance);
    }
}

void CoastlineStrength(const std::vector<float>& pLandFraction, int pSize, std::vector<float>* pOut)
{
    // Cizgi suyun tarafinda tek piksel (ilk surumde 3x3 ortalama 2-3 piksel genis ve bulanikti):
    //   kara agirlikli piksel (oran >= 0.5)       : karisik ise (1 - oran) * 2, tam kara ise 0
    //   su agirlikli piksel, 4 komsudan biri kara : en buyuk komsu kara orani
    // Kaba seviyelerde oran ondalikli oldugu icin cizgi kendiliginden yumusak kenarlidir.
    pOut->assign(static_cast<size_t>(pSize) * pSize, 0.0f);
    auto tAt = [&](int pColumn, int pRow)
    {
        if (pColumn < 0 || pRow < 0 || pColumn >= pSize || pRow >= pSize)
        {
            return -1.0f;
        }
        return pLandFraction[static_cast<size_t>(pRow) * pSize + pColumn];
    };
    for (int tRow = 0; tRow < pSize; ++tRow)
    {
        for (int tColumn = 0; tColumn < pSize; ++tColumn)
        {
            const float tOwn = tAt(tColumn, tRow);
            if (tOwn < 0.0f)
            {
                continue;  // veri yok
            }
            float tStrength = 0.0f;
            if (tOwn >= 0.5f)
            {
                tStrength = (1.0f - tOwn) * 2.0f;
            }
            else
            {
                const float tNeighbor = std::max(std::max(tAt(tColumn - 1, tRow), tAt(tColumn + 1, tRow)),
                                                 std::max(tAt(tColumn, tRow - 1), tAt(tColumn, tRow + 1)));
                tStrength = std::max(tNeighbor, tOwn * 2.0f);
            }
            (*pOut)[static_cast<size_t>(tRow) * pSize + tColumn] = std::clamp(tStrength, 0.0f, 1.0f);
        }
    }
}
