// TerrainRaster.cpp
#include "data/TerrainRaster.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <optional>

#include "data/LandColorField.h"
#include "data/TerrainElevation.h"
#include "data/TerrainNoise.h"
#include "data/TerrainRelief.h"
#include "data/TerrainTriangles.h"

namespace
{

const char kMagic[4] = {'M', 'P', 'T', 'R'};
// 2: rakim raster'i eklendi (VFR). 3 (0.16): golge ve rakim raster'lari cikti (rakim ayri
// dosyada, TerrainElevation). Eski surum yeniden uretilir.
const uint32_t kFileVersion = 3;

const double kPi = 3.14159265358979323846;
const double kMetersPerDegreeLat = 110574.0;
const double kMetersPerDegree = 111320.0;  // enlem derecesi (yaklasik; burada yeterli)
const double kFeetPerMeter = 3.28084;
const double kCoastlineShade = 0.45;  // kiyi cizgisi rengi: su renginin bu orani (koyu su tonu)
// Derinlik koyulasmasinin kanal agirliklari (R, G, B): esit koyulasma suyu griye ceviriyordu
// (onizlemede kirli leke); mavi az koyulasinca derin su doygun koyu mavi kalir
const double kDepthChannelWeight[3] = {1.0, 0.8, 0.5};
const int kMaxSamplesPerAxis = 4;     // kucultmede piksel basina en fazla 4x4 ornek

int ClampInt(int pValue, int pLow, int pHigh)
{
    return pValue < pLow ? pLow : (pValue > pHigh ? pHigh : pValue);
}

void WriteU32(std::ofstream& pFile, uint32_t pValue)
{
    pFile.write(reinterpret_cast<const char*>(&pValue), sizeof(pValue));  // Windows/x86: kucuk-endian
}

bool ReadU32(std::ifstream& pFile, uint32_t* pValue)
{
    return static_cast<bool>(pFile.read(reinterpret_cast<char*>(pValue), sizeof(*pValue)));
}

// Piksel koordinatli ucgeni pCells'e (pSize x pSize) boyar: merkezi ucgenin
// icinde kalan pikseller pValue olur. Sinif raster'i ve yakin zoom parcalari
// ayni kurali kullanir.
void FillCells(std::vector<uint8_t>& pCells, int pSize, const double pX[3], const double pY[3], uint8_t pValue)
{
    // Isaretli alan: yonu (saat yonu / tersi) ne olursa olsun ayni testle doldurmak icin
    const double tArea = (pX[1] - pX[0]) * (pY[2] - pY[0]) - (pX[2] - pX[0]) * (pY[1] - pY[0]);
    if (std::fabs(tArea) < 1e-12)
    {
        return;
    }
    const double tSign = tArea > 0 ? 1.0 : -1.0;

    const int tMinX = ClampInt(static_cast<int>(std::floor(std::min({pX[0], pX[1], pX[2]}))), 0, pSize - 1);
    const int tMaxX = ClampInt(static_cast<int>(std::ceil(std::max({pX[0], pX[1], pX[2]}))), 0, pSize - 1);
    const int tMinY = ClampInt(static_cast<int>(std::floor(std::min({pY[0], pY[1], pY[2]}))), 0, pSize - 1);
    const int tMaxY = ClampInt(static_cast<int>(std::ceil(std::max({pY[0], pY[1], pY[2]}))), 0, pSize - 1);

    for (int tRow = tMinY; tRow <= tMaxY; ++tRow)
    {
        const double tPy = tRow + 0.5;
        for (int tColumn = tMinX; tColumn <= tMaxX; ++tColumn)
        {
            const double tPx = tColumn + 0.5;
            // Uc kenarin hepsinin ayni tarafindaysa nokta icerdedir (kenar fonksiyonlari)
            const double tE0 = ((pX[1] - pX[0]) * (tPy - pY[0]) - (pY[1] - pY[0]) * (tPx - pX[0])) * tSign;
            const double tE1 = ((pX[2] - pX[1]) * (tPy - pY[1]) - (pY[2] - pY[1]) * (tPx - pX[1])) * tSign;
            const double tE2 = ((pX[0] - pX[2]) * (tPy - pY[2]) - (pY[0] - pY[2]) * (tPx - pX[2])) * tSign;
            if (tE0 >= 0 && tE1 >= 0 && tE2 >= 0)
            {
                pCells[static_cast<size_t>(tRow) * pSize + tColumn] = pValue;
            }
        }
    }
}

}  // namespace

// --- 0.16: sinir bukmesi (domain warp) yardimcilari ---

// Bukme plani: sayisal kisim TerrainPlanWarp'ta (testli); burada stil kontrolu
struct WarpPlan
{
    bool mActive = false;
    int mPad = 0;   // cikti pikseli: en buyuk kayma + 1
    int mStep = 1;  // kayma alaninin hesaplandigi izgara adimi (piksel), arasi cift dogrusal
};

WarpPlan PlanWarp(const TerrainPalette& pPalette, const TerrainImageStyle& pStyle, double pMetersPerPixelX,
                  double pMetersPerPixelY)
{
    WarpPlan tPlan;
    if (pStyle.mStyle != TerrainStyle::Classes)
    {
        return tPlan;
    }
    const TerrainWarpPlan tNumbers = TerrainPlanWarp(pPalette.Warp(), pMetersPerPixelX, pMetersPerPixelY);
    tPlan.mActive = tNumbers.mActive;
    tPlan.mPad = tNumbers.mPad;
    tPlan.mStep = tNumbers.mStep;
    return tPlan;
}

// Payli dokunun (pPadded x pPadded) piksel basina kaymasi (piksel; +x dogu, +y GUNEY = satir artar).
// Izgara pStep adimla hesaplanir, arasi cift dogrusal okunur. Gurultu sadece izgara noktalarinda
// hesaplanir: seviye 5'te bile dokunun ~%1'i, tam piksel basina cagrilmaz.
class WarpField
{
public:
    // pLonOf / pLatOf: payli dokunun (sutun, satir) piksel MERKEZININ boylam / enlemi
    template <typename TLonOf, typename TLatOf>
    void Build(int pPadded, int pStep, const TerrainWarpParams& pParams, double pMetersPerPixelX,
               double pMetersPerPixelY, TLonOf pLonOf, TLatOf pLatOf)
    {
        mStep = std::max(1, pStep);
        mCells = pPadded / mStep + 2;
        mDx.assign(static_cast<size_t>(mCells) * mCells, 0.0f);
        mDy.assign(static_cast<size_t>(mCells) * mCells, 0.0f);
        for (int tRow = 0; tRow < mCells; ++tRow)
        {
            for (int tColumn = 0; tColumn < mCells; ++tColumn)
            {
                const int tX = std::min(tColumn * mStep, pPadded - 1);
                const int tRowPix = std::min(tRow * mStep, pPadded - 1);
                double tEast = 0.0;
                double tNorth = 0.0;
                TerrainWarpOffset(pLonOf(tX), pLatOf(tRowPix), pParams, &tEast, &tNorth);
                mDx[static_cast<size_t>(tRow) * mCells + tColumn] = static_cast<float>(tEast / pMetersPerPixelX);
                mDy[static_cast<size_t>(tRow) * mCells + tColumn] = static_cast<float>(-tNorth / pMetersPerPixelY);
            }
        }
    }

    // Kayma (piksel); izgara disinda kenar degeri
    void OffsetAt(int pColumn, int pRow, double* pDx, double* pDy) const
    {
        const double tGX = static_cast<double>(pColumn) / mStep;
        const double tGY = static_cast<double>(pRow) / mStep;
        const int tX = ClampInt(static_cast<int>(std::floor(tGX)), 0, mCells - 2);
        const int tY = ClampInt(static_cast<int>(std::floor(tGY)), 0, mCells - 2);
        const double tU = std::clamp(tGX - tX, 0.0, 1.0);
        const double tV = std::clamp(tGY - tY, 0.0, 1.0);
        const size_t tBase = static_cast<size_t>(tY) * mCells + tX;
        const double tDx0 = mDx[tBase] + (mDx[tBase + 1] - mDx[tBase]) * tU;
        const double tDx1 = mDx[tBase + mCells] + (mDx[tBase + mCells + 1] - mDx[tBase + mCells]) * tU;
        const double tDy0 = mDy[tBase] + (mDy[tBase + 1] - mDy[tBase]) * tU;
        const double tDy1 = mDy[tBase + mCells] + (mDy[tBase + mCells + 1] - mDy[tBase + mCells]) * tU;
        *pDx = tDx0 + (tDx1 - tDx0) * tV;
        *pDy = tDy0 + (tDy1 - tDy0) * tV;
    }

private:
    int mCells = 0;
    int mStep = 1;
    std::vector<float> mDx;
    std::vector<float> mDy;
};

// Yakin zoom sinif karisimi ve karisik renk tablosu TerrainPalette::CloseupMixAt / MixedColors icinde (testli)

// Dokunun bir pikselinin yerdeki boyu. pDegreesPerPixelX/Y: derece; pLatitude: dokunun orta enlemi
LandBlendPlan PlanForImage(const TerrainPalette& pPalette, const TerrainImageStyle& pStyle,
                           double pDegreesPerPixelX, double pDegreesPerPixelY, double pLatitude)
{
    if (pStyle.mStyle != TerrainStyle::Classes)
    {
        return LandBlendPlan();  // VFR / IFR: sinif rengi yok, yumusatma yok
    }
    const double tCos = std::max(0.05, std::cos(pLatitude * kPi / 180.0));
    return PlanLandBlend(pPalette.BlendMeters(), pDegreesPerPixelX * kMetersPerDegree * tCos,
                         pDegreesPerPixelY * kMetersPerDegree, pPalette.BlendMaxPixels());
}

void TerrainRaster::Reset(int pWest, int pSouth)
{
    mWest = pWest;
    mSouth = pSouth;
    mClasses.assign(static_cast<size_t>(kSize) * kSize, static_cast<uint8_t>(TerrainClass::None));
}

int TerrainRaster::West() const
{
    return mWest;
}

int TerrainRaster::South() const
{
    return mSouth;
}

void TerrainRaster::FillTriangle(const double pLon[3], const double pLat[3], TerrainClass pClass)
{
    // Piksel koordinati: x = (boylam - bati) * kSize, y = (kuzey - enlem) * kSize
    double tX[3];
    double tY[3];
    for (int tCorner = 0; tCorner < 3; ++tCorner)
    {
        tX[tCorner] = (pLon[tCorner] - mWest) * kSize;
        tY[tCorner] = (mSouth + 1 - pLat[tCorner]) * kSize;
    }
    FillCells(mClasses, kSize, tX, tY, static_cast<uint8_t>(pClass));
}

TerrainClass TerrainRaster::WarpedClassAt(int pX, int pY, double pOffsetCellsX, double pOffsetCellsY) const
{
    const TerrainClass tOwn = ClassAt(pX, pY);
    // Kara-kara kurali: su ve veri yok kayma ile yer degistirmez, kiyi aynen kalir
    if (tOwn == TerrainClass::None || tOwn == TerrainClass::Water)
    {
        return tOwn;
    }
    const int tOtherX = ClampInt(pX + static_cast<int>(std::lround(pOffsetCellsX)), 0, kSize - 1);
    const int tOtherY = ClampInt(pY + static_cast<int>(std::lround(pOffsetCellsY)), 0, kSize - 1);
    const TerrainClass tOther = ClassAt(tOtherX, tOtherY);
    if (tOther == TerrainClass::None || tOther == TerrainClass::Water)
    {
        return tOwn;
    }
    return tOther;
}

TerrainClass TerrainRaster::ClassAt(int pX, int pY) const
{
    return static_cast<TerrainClass>(mClasses[static_cast<size_t>(pY) * kSize + pX]);
}

int TerrainRaster::CountOf(TerrainClass pClass) const
{
    return static_cast<int>(std::count(mClasses.begin(), mClasses.end(), static_cast<uint8_t>(pClass)));
}

// --- 0.16: goruntunun son adimi (iki uretici de kullanir) ---

namespace
{

// Goruntu uretiminin ara sonuclari. Payli alan pPadded x pPadded; cikti pikseli (c, r) = payli (c + mPad, r + mPad).
struct ImageBuffers
{
    int mOutSize = 0;
    int mPad = 0;
    int mPadded = 0;
    std::vector<float> mLandFraction;  // payli: kara orani 0..1, -1 = veri yok (None)
    std::vector<float> mCoverage;      // payli: kaplama orani 0..1 (alfa)
    std::vector<float> mLandRgb;       // cikti: Classes stilinde kara rengi (3 kanal, 0..255)
    double mMetersPerPixelX = 1.0;
    double mMetersPerPixelY = 1.0;
    double mLatitude = 0.0;
    // Cikti pikselinin karodaki yeri (karo birimi): u = mU0 + (c + 0.5) * mDu (batidan),
    // v = mV0 + (r + 0.5) * mDv (kuzeyden)
    double mU0 = 0.0;
    double mV0 = 0.0;
    double mDu = 0.0;
    double mDv = 0.0;

    void Reset(int pOutSize, int pPad)
    {
        mOutSize = pOutSize;
        mPad = pPad;
        mPadded = pOutSize + 2 * pPad;
        mLandFraction.assign(static_cast<size_t>(mPadded) * mPadded, -1.0f);
        mCoverage.assign(static_cast<size_t>(mPadded) * mPadded, 0.0f);
        mLandRgb.assign(static_cast<size_t>(pOutSize) * pOutSize * 3, 0.0f);
    }

    size_t PaddedIndex(int pColumn, int pRow) const
    {
        return static_cast<size_t>(pRow + mPad) * mPadded + (pColumn + mPad);
    }
};

// Kiyi seridinin genisligi (doku pikseli): metre, piksel tavaniyla sinirli. IFR (LandWater) ve kapaliysa 0.
double CoastBandPixels(const TerrainPalette& pPalette, const TerrainImageStyle& pStyle, double pMetersPerPixel)
{
    const TerrainWaterParams& tWater = pPalette.Water();
    if (pStyle.mStyle == TerrainStyle::LandWater || tWater.mBandMeters <= 0.0 || tWater.mBandStrength <= 0.0)
    {
        return 0.0;
    }
    const double tPixels = std::min(tWater.mBandMeters / pMetersPerPixel, tWater.mBandMaxPixels);
    return tPixels >= 0.5 ? tPixels : 0.0;
}

// Su islemlerinin istedigi pay (piksel): kiyi cizgisi 3x3'e bakar (1), serit kendi genisligi kadar
int CoastPad(const TerrainPalette& pPalette, const TerrainImageStyle& pStyle, double pMetersPerPixel)
{
    int tPad = pPalette.Water().mCoastline ? 1 : 0;
    const double tBand = CoastBandPixels(pPalette, pStyle, pMetersPerPixel);
    if (tBand > 0.0)
    {
        tPad = std::max(tPad, static_cast<int>(std::ceil(tBand)) + 1);
    }
    return tPad;
}

TerrainColor Scaled(const TerrainColor& pColor, double pFactor)
{
    return {static_cast<uint8_t>(std::clamp(pColor.mR * pFactor, 0.0, 255.0)),
            static_cast<uint8_t>(std::clamp(pColor.mG * pFactor, 0.0, 255.0)),
            static_cast<uint8_t>(std::clamp(pColor.mB * pFactor, 0.0, 255.0))};
}

// Ara sonuclardan RGBA: kabartma, VFR bantlari, su tonu, kiyi cizgisi (sinif: TerrainRaster.h basi, adim 3-4)
void ComposeImage(const ImageBuffers& pBuffers, const TerrainPalette& pPalette, const TerrainImageStyle& pStyle,
                  const ElevationTiles* pElevation, std::vector<uint8_t>* pRgba)
{
    const int tSize = pBuffers.mOutSize;
    pRgba->assign(static_cast<size_t>(tSize) * tSize * 4, 0);
    const TerrainWaterParams& tWater = pPalette.Water();
    const bool tWaterTone = pStyle.mStyle != TerrainStyle::LandWater;  // IFR: duz su, sadece kiyi cizgisi

    // 1) Kiyi cizgisi gucu ve kiyiya uzaklik (payli alanda: dokunun kenarinda da dogru)
    std::vector<float> tLine;
    if (tWater.mCoastline && tWater.mCoastlineStrength > 0.0)
    {
        CoastlineStrength(pBuffers.mLandFraction, pBuffers.mPadded, &tLine);
    }
    const double tBandPixels = CoastBandPixels(pPalette, pStyle, pBuffers.mMetersPerPixelY);
    std::vector<float> tDistance;
    if (tBandPixels > 0.0)
    {
        std::vector<uint8_t> tLand(pBuffers.mLandFraction.size());
        for (size_t tIndex = 0; tIndex < tLand.size(); ++tIndex)
        {
            tLand[tIndex] = pBuffers.mLandFraction[tIndex] >= 0.5f ? 1 : 0;
        }
        DistanceToLand(tLand, pBuffers.mPadded, static_cast<float>(tBandPixels) + 1.0f, &tDistance);
    }

    // 2) Rakim: piksel boyuna uygun piramit seviyesi (uzakta kaba seviye: ortusme / parazit yok)
    std::optional<ElevationSampler> tSampler;
    double tExaggeration = 0.0;
    const ElevationGrid* tCenter = pElevation ? pElevation->mTiles[1][1] : nullptr;
    if (tCenter && !tCenter->IsEmpty())
    {
        tSampler.emplace(pElevation->mTiles, tCenter->LevelFor(pBuffers.mMetersPerPixelY));
        tExaggeration = ReliefExaggeration(pPalette.Relief(), tSampler->CellMeters(), tCenter->CellMeters(0));
    }
    const bool tRelief = tSampler.has_value() && pStyle.mStyle != TerrainStyle::LandWater &&
                         pPalette.ShadingStrength() > 0.0;
    const double tMetersPerU = kMetersPerDegree * std::max(0.05, std::cos(pBuffers.mLatitude * kPi / 180.0));
    const double tMetersPerV = kMetersPerDegreeLat;

    const TerrainColor tWaterBase =
        pStyle.mStyle == TerrainStyle::Classes ? pPalette.ColorOf(TerrainClass::Water) : pStyle.mWater;
    const TerrainColor tLineColor = Scaled(tWaterBase, kCoastlineShade);

    // 3) Cikti pikselleri
    for (int tRow = 0; tRow < tSize; ++tRow)
    {
        for (int tColumn = 0; tColumn < tSize; ++tColumn)
        {
            const size_t tPadded = pBuffers.PaddedIndex(tColumn, tRow);
            const double tLandFraction = pBuffers.mLandFraction[tPadded];
            if (tLandFraction < 0.0)
            {
                continue;  // veri yok: saydam (alttaki kaba seviye gorunur)
            }
            const double tU = pBuffers.mU0 + (tColumn + 0.5) * pBuffers.mDu;
            const double tV = pBuffers.mV0 + (tRow + 0.5) * pBuffers.mDv;
            double tHeight = 0.0;
            double tSlopeEast = 0.0;
            double tSlopeNorth = 0.0;
            if (tSampler)
            {
                double tDhDu = 0.0;
                double tDhDv = 0.0;
                tSampler->Sample(tU, tV, &tHeight, &tDhDu, &tDhDv);
                tSlopeEast = tDhDu / tMetersPerU;
                tSlopeNorth = -tDhDv / tMetersPerV;  // v guneye artar
            }

            // Kara rengi (stile gore) ve kabartma
            double tLand[3] = {0.0, 0.0, 0.0};
            if (tLandFraction > 0.0)
            {
                if (pStyle.mStyle == TerrainStyle::Classes)
                {
                    const size_t tOut = (static_cast<size_t>(tRow) * tSize + tColumn) * 3;
                    tLand[0] = pBuffers.mLandRgb[tOut];
                    tLand[1] = pBuffers.mLandRgb[tOut + 1];
                    tLand[2] = pBuffers.mLandRgb[tOut + 2];
                }
                else if (pStyle.mStyle == TerrainStyle::Elevation && pPalette.Vfr().mBands)
                {
                    const double tSlope = std::sqrt(tSlopeEast * tSlopeEast + tSlopeNorth * tSlopeNorth);
                    VfrBandColor(tHeight, tSlope * pBuffers.mMetersPerPixelY * kFeetPerMeter, pPalette.Vfr(), tLand);
                }
                else
                {
                    const TerrainColor tColor =
                        pStyle.mStyle == TerrainStyle::Elevation ? ElevationColor(tHeight) : pStyle.mLand;
                    tLand[0] = tColor.mR;
                    tLand[1] = tColor.mG;
                    tLand[2] = tColor.mB;
                }
                if (tRelief)
                {
                    const double tFactor = ReliefFactor(tSlopeEast, tSlopeNorth, pPalette.ShadingStrength(),
                                                        tExaggeration, pPalette.Relief());
                    for (double& tChannel : tLand)
                    {
                        tChannel *= tFactor;
                    }
                }
            }

            // Su rengi: derinlik tonu (sea_level) ve kiyi seridi (sadece Scenery / VFR)
            double tWaterRgb[3] = {static_cast<double>(tWaterBase.mR), static_cast<double>(tWaterBase.mG),
                                   static_cast<double>(tWaterBase.mB)};
            if (tLandFraction < 1.0 && tWaterTone)
            {
                double tSeaLevel = 0.0;
                const double tDarken =
                    (tSampler && tSampler->SeaLevelAt(tU, tV, &tSeaLevel)) ? DepthDarkening(tSeaLevel, tWater) : 0.0;
                const double tLighten =
                    tDistance.empty() ? 0.0 : CoastBandLightening(tDistance[tPadded], tBandPixels, tWater);
                for (int tChannel = 0; tChannel < 3; ++tChannel)
                {
                    // Derin su griye degil koyu maviye gitsin: kirmizi en cok, mavi en az koyulasir
                    tWaterRgb[tChannel] *= 1.0 - tDarken * kDepthChannelWeight[tChannel];
                    tWaterRgb[tChannel] += (255.0 - tWaterRgb[tChannel]) * tLighten;
                }
            }

            double tRgb[3];
            for (int tChannel = 0; tChannel < 3; ++tChannel)
            {
                tRgb[tChannel] = tLand[tChannel] * tLandFraction + tWaterRgb[tChannel] * (1.0 - tLandFraction);
            }
            if (!tLine.empty() && tLine[tPadded] > 0.0f)
            {
                const double tAlpha = tLine[tPadded] * tWater.mCoastlineStrength;
                tRgb[0] = tRgb[0] * (1.0 - tAlpha) + tLineColor.mR * tAlpha;
                tRgb[1] = tRgb[1] * (1.0 - tAlpha) + tLineColor.mG * tAlpha;
                tRgb[2] = tRgb[2] * (1.0 - tAlpha) + tLineColor.mB * tAlpha;
            }
            uint8_t* tPixel = pRgba->data() + (static_cast<size_t>(tRow) * tSize + tColumn) * 4;
            for (int tChannel = 0; tChannel < 3; ++tChannel)
            {
                tPixel[tChannel] = static_cast<uint8_t>(std::clamp(tRgb[tChannel], 0.0, 255.0));
            }
            tPixel[3] = static_cast<uint8_t>(std::lround(255.0 * pBuffers.mCoverage[tPadded]));
        }
    }
}

}  // namespace

void TerrainRaster::BuildImage(int pX0, int pY0, int pX1, int pY1, int pOutSize, const TerrainPalette& pPalette,
                               const TerrainImageStyle& pStyle, std::vector<uint8_t>* pRgba,
                               const ElevationTiles* pElevation) const
{
    const double tBlockX = static_cast<double>(pX1 - pX0) / pOutSize;
    const double tBlockY = static_cast<double>(pY1 - pY0) / pOutSize;
    const int tSamplesX = std::max(1, std::min(kMaxSamplesPerAxis, static_cast<int>(tBlockX)));
    const int tSamplesY = std::max(1, std::min(kMaxSamplesPerAxis, static_cast<int>(tBlockY)));

    // 0) Planlar: yumusatma (blur), sinir bukmesi ve su; pay hepsinin toplami
    const double tCenterLat = mSouth + 1.0 - (pY0 + pY1) * 0.5 / kSize;  // satir 0 = kuzey
    const double tCos = std::max(0.05, std::cos(tCenterLat * kPi / 180.0));
    const double tMppX = tBlockX / kSize * kMetersPerDegree * tCos;  // pikselin yerdeki boyu (metre)
    const double tMppY = tBlockY / kSize * kMetersPerDegree;
    const LandBlendPlan tPlan = PlanForImage(pPalette, pStyle, tBlockX / kSize, tBlockY / kSize, tCenterLat);
    const WarpPlan tWarpPlan = PlanWarp(pPalette, pStyle, tMppX, tMppY);
    const int tPad = tPlan.mPad + tWarpPlan.mPad + CoastPad(pPalette, pStyle, tMppY);
    const int tPadded = pOutSize + 2 * tPad;
    // Bu doku icin sinif renkleri (yakin zoomda referans kara tonuna dogru karisik)
    const auto tColors = pPalette.MixedColors(pPalette.CloseupMixAt(tMppY));
    LandColorField tField;
    if (tPlan.mPad > 0)
    {
        tField.Reset(tPadded, tPlan.mFactor);
    }
    // Kayma alani: cikti piksel izgarasinda, izgara adimli (gurultu cagrisi maliyeti icin)
    WarpField tWarpField;
    if (tWarpPlan.mActive)
    {
        auto tLonOf = [&](int pColumn)
        {
            return mWest + (pX0 + (pColumn - tPad + 0.5) * tBlockX) / kSize;
        };
        auto tLatOf = [&](int pRow)
        {
            return mSouth + 1.0 - (pY0 + (pRow - tPad + 0.5) * tBlockY) / kSize;
        };
        tWarpField.Build(tPadded, tWarpPlan.mStep, pPalette.Warp(), tMppX, tMppY, tLonOf, tLatOf);
    }
    // Ornek hucrenin bukulmus sinifi (kayma cikti pikselinden, hucre cinsine cevrilmis)
    auto tWarpedClass = [&](int tX, int tY, int tColumn, int tRow)
    {
        double tDx = 0.0;
        double tDy = 0.0;
        if (tWarpPlan.mActive)
        {
            tWarpField.OffsetAt(tColumn + tPad, tRow + tPad, &tDx, &tDy);
        }
        return WarpedClassAt(tX, tY, tDx * tBlockX, tDy * tBlockY);
    };

    ImageBuffers tBuffers;
    tBuffers.Reset(pOutSize, tPad);
    tBuffers.mMetersPerPixelX = tMppX;
    tBuffers.mMetersPerPixelY = tMppY;
    tBuffers.mLatitude = tCenterLat;
    tBuffers.mU0 = static_cast<double>(pX0) / kSize;
    tBuffers.mV0 = static_cast<double>(pY0) / kSize;
    tBuffers.mDu = tBlockX / kSize;
    tBuffers.mDv = tBlockY / kSize;

    // 1) Payli alanin her pikseli: ornekleri say (karonun disi veri yok); kara renklerini topla.
    // Kara ornekleri yumusatma alanina da eklenir (su ve None eklenmez).
    std::vector<int> tLandCounts(static_cast<size_t>(pOutSize) * pOutSize, 0);
    for (int tRow = -tPad; tRow < pOutSize + tPad; ++tRow)
    {
        for (int tColumn = -tPad; tColumn < pOutSize + tPad; ++tColumn)
        {
            const bool tOutput = tRow >= 0 && tRow < pOutSize && tColumn >= 0 && tColumn < pOutSize;
            const size_t tOut = tOutput ? static_cast<size_t>(tRow) * pOutSize + tColumn : 0;
            int tCovered = 0;
            int tLand = 0;
            for (int tSampleY = 0; tSampleY < tSamplesY; ++tSampleY)
            {
                const int tY = pY0 + static_cast<int>(std::floor((tRow + (tSampleY + 0.5) / tSamplesY) * tBlockY));
                if (tY < 0 || tY >= kSize)
                {
                    continue;
                }
                for (int tSampleX = 0; tSampleX < tSamplesX; ++tSampleX)
                {
                    const int tX =
                        pX0 + static_cast<int>(std::floor((tColumn + (tSampleX + 0.5) / tSamplesX) * tBlockX));
                    if (tX < 0 || tX >= kSize)
                    {
                        continue;
                    }
                    const TerrainClass tClass = tWarpedClass(tX, tY, tColumn, tRow);
                    if (tClass == TerrainClass::None)
                    {
                        continue;
                    }
                    ++tCovered;
                    if (tClass == TerrainClass::Water)
                    {
                        continue;
                    }
                    ++tLand;
                    const TerrainColor& tColor = tColors[static_cast<int>(tClass)];
                    if (tPlan.mPad > 0)
                    {
                        tField.Add(tColumn + tPad, tRow + tPad, tColor, 1.0f);
                    }
                    if (tOutput)
                    {
                        tBuffers.mLandRgb[tOut * 3] += tColor.mR;
                        tBuffers.mLandRgb[tOut * 3 + 1] += tColor.mG;
                        tBuffers.mLandRgb[tOut * 3 + 2] += tColor.mB;
                    }
                }
            }
            const size_t tIndex = tBuffers.PaddedIndex(tColumn, tRow);
            if (tCovered > 0)
            {
                tBuffers.mLandFraction[tIndex] = static_cast<float>(tLand) / tCovered;
                tBuffers.mCoverage[tIndex] = static_cast<float>(tCovered) / (tSamplesX * tSamplesY);
            }
            if (tOutput)
            {
                tLandCounts[tOut] = tLand;
            }
        }
    }

    // 2) Kara rengi: ornek ortalamasi; yumusatma plani doluysa alandan
    if (tPlan.mPad > 0)
    {
        tField.Blur(tPlan.mHalfX, tPlan.mHalfY);
    }
    for (size_t tOut = 0; tOut < tLandCounts.size(); ++tOut)
    {
        const int tLand = tLandCounts[tOut];
        if (tLand == 0)
        {
            continue;
        }
        float* tRgb = &tBuffers.mLandRgb[tOut * 3];
        tRgb[0] /= tLand;
        tRgb[1] /= tLand;
        tRgb[2] /= tLand;
        double tBlended[3];
        const int tColumn = static_cast<int>(tOut % pOutSize);
        const int tRow = static_cast<int>(tOut / pOutSize);
        if (tPlan.mPad > 0 && tField.ColorAt(tColumn + tPad, tRow + tPad, tBlended))
        {
            tRgb[0] = static_cast<float>(tBlended[0]);
            tRgb[1] = static_cast<float>(tBlended[1]);
            tRgb[2] = static_cast<float>(tBlended[2]);
        }
    }

    // 3-4) Kabartma, VFR bantlari, su, kiyi cizgisi
    ComposeImage(tBuffers, pPalette, pStyle, pElevation, pRgba);
}

void TerrainRaster::BuildDetailImage(const TerrainTriangles& pTriangles, int pChunksPerSide, int pChunk,
                                     int pOutSize, const TerrainPalette& pPalette, const TerrainImageStyle& pStyle,
                                     std::vector<uint8_t>* pRgba, const ElevationTiles* pElevation) const
{
    const int tChunks = pChunksPerSide;
    const int tChunkColumn = pChunk % tChunks;
    const int tChunkRow = pChunk / tChunks;

    // 0) Planlar: yumusatma (blur), sinir bukmesi ve su; toplam pay (ucgen toplama payli calisir)
    const double tDegreesPerPixel = 1.0 / (static_cast<double>(tChunks) * pOutSize);
    const double tCenterLat = mSouth + 1.0 - (tChunkRow + 0.5) / tChunks;
    const double tMppX = tDegreesPerPixel * kMetersPerDegree * std::max(0.05, std::cos(tCenterLat * kPi / 180.0));
    const double tMppY = tDegreesPerPixel * kMetersPerDegree;
    const LandBlendPlan tPlan = PlanForImage(pPalette, pStyle, tDegreesPerPixel, tDegreesPerPixel, tCenterLat);
    const WarpPlan tWarpPlan = PlanWarp(pPalette, pStyle, tMppX, tMppY);
    const int tPad = tPlan.mPad + tWarpPlan.mPad + CoastPad(pPalette, pStyle, tMppY);
    const int tPadded = pOutSize + 2 * tPad;

    // 1) Ucgen listesi: pay, komsu 1/32 derece parcalara tasidigi icin payli dikdortgenin
    // kapsadigi tum ebeveyn parcalar toplanir (ayni ucgen birden cok parcade olabilir: tekrarlar atilir)
    const double tPadDegrees = tPad * tDegreesPerPixel;
    const double tLeft = static_cast<double>(tChunkColumn) / tChunks - tPadDegrees;
    const double tRight = static_cast<double>(tChunkColumn + 1) / tChunks + tPadDegrees;
    const double tTop = static_cast<double>(tChunkRow) / tChunks - tPadDegrees;
    const double tBottom = static_cast<double>(tChunkRow + 1) / tChunks + tPadDegrees;
    const int kParents = TerrainTriangles::kChunksPerSide;
    auto tParentIndex = [&](double pValue)
    {
        return std::clamp(static_cast<int>(std::floor(pValue * kParents)), 0, kParents - 1);
    };
    std::vector<uint32_t> tTriangleList;
    for (int tParentRow = tParentIndex(tTop); tParentRow <= tParentIndex(tBottom); ++tParentRow)
    {
        for (int tParentColumn = tParentIndex(tLeft); tParentColumn <= tParentIndex(tRight); ++tParentColumn)
        {
            const std::vector<uint32_t>& tIn = pTriangles.TrianglesIn(tParentRow * kParents + tParentColumn);
            tTriangleList.insert(tTriangleList.end(), tIn.begin(), tIn.end());
        }
    }
    std::sort(tTriangleList.begin(), tTriangleList.end());
    tTriangleList.erase(std::unique(tTriangleList.begin(), tTriangleList.end()), tTriangleList.end());

    // 2) Payli sinif hucresi: parcanin siniflarini ucgenlerden boya (piksel basina 1 sinif;
    // payli dikdortgenin disi FillCells'te kirpilir, karonun disi None = veri yok)
    std::vector<uint8_t> tCells(static_cast<size_t>(tPadded) * tPadded, static_cast<uint8_t>(TerrainClass::None));
    for (const uint32_t tIndex : tTriangleList)
    {
        double tLon[3];
        double tLat[3];
        pTriangles.Corners(tIndex, tLon, tLat);  // karonun kosesine gore derece
        double tX[3];
        double tY[3];
        for (int tCorner = 0; tCorner < 3; ++tCorner)
        {
            tX[tCorner] = (tLon[tCorner] * tChunks - tChunkColumn) * pOutSize + tPad;
            tY[tCorner] = ((1.0 - tLat[tCorner]) * tChunks - tChunkRow) * pOutSize + tPad;
        }
        FillCells(tCells, tPadded, tX, tY, static_cast<uint8_t>(pTriangles.ClassOf(tIndex)));
    }

    // 3) Bukulmus sinif tamponu: kara-kara kurali (su ve None kayma ile yer degistirmez, kiyi aynen kalir).
    // Pay bolgesinin en dis tWarpPlan.mPad pikselinde kayma tamponun disina dusebilir (sinirlanir);
    // bu pikseller sadece blur kaynagidir, ic piksellerin sonucunu etkilemez.
    std::vector<uint8_t> tWarped;
    WarpField tWarpField;
    if (tWarpPlan.mActive)
    {
        auto tLonOf = [&](int pColumn)
        {
            return mWest + (tChunkColumn + (pColumn - tPad + 0.5) / pOutSize) / tChunks;
        };
        auto tLatOf = [&](int pRow)
        {
            return mSouth + 1.0 - (tChunkRow + (pRow - tPad + 0.5) / pOutSize) / tChunks;
        };
        tWarpField.Build(tPadded, tWarpPlan.mStep, pPalette.Warp(), tMppX, tMppY, tLonOf, tLatOf);
        tWarped.assign(static_cast<size_t>(tPadded) * tPadded, static_cast<uint8_t>(TerrainClass::None));
        for (int tRow = 0; tRow < tPadded; ++tRow)
        {
            for (int tColumn = 0; tColumn < tPadded; ++tColumn)
            {
                const TerrainClass tOwn =
                    static_cast<TerrainClass>(tCells[static_cast<size_t>(tRow) * tPadded + tColumn]);
                TerrainClass tResult = tOwn;
                if (tOwn != TerrainClass::None && tOwn != TerrainClass::Water)
                {
                    double tDx = 0.0;
                    double tDy = 0.0;
                    tWarpField.OffsetAt(tColumn, tRow, &tDx, &tDy);
                    const int tOtherX = ClampInt(tColumn + static_cast<int>(std::lround(tDx)), 0, tPadded - 1);
                    const int tOtherY = ClampInt(tRow + static_cast<int>(std::lround(tDy)), 0, tPadded - 1);
                    const TerrainClass tOther =
                        static_cast<TerrainClass>(tCells[static_cast<size_t>(tOtherY) * tPadded + tOtherX]);
                    if (tOther != TerrainClass::None && tOther != TerrainClass::Water)
                    {
                        tResult = tOther;
                    }
                }
                tWarped[static_cast<size_t>(tRow) * tPadded + tColumn] = static_cast<uint8_t>(tResult);
            }
        }
    }
    else
    {
        tWarped = tCells;  // bukme kapali: ayni tampon
    }

    // 4) Kara renkleri alana ekle ve yumusat (su ve None eklenmez; plan bossa alan yok)
    const auto tColors = pPalette.MixedColors(pPalette.CloseupMixAt(tMppY));
    LandColorField tField;
    if (tPlan.mPad > 0)
    {
        tField.Reset(tPadded, tPlan.mFactor);
        for (int tRow = 0; tRow < tPadded; ++tRow)
        {
            for (int tColumn = 0; tColumn < tPadded; ++tColumn)
            {
                const TerrainClass tClass =
                    static_cast<TerrainClass>(tWarped[static_cast<size_t>(tRow) * tPadded + tColumn]);
                if (tClass != TerrainClass::None && tClass != TerrainClass::Water)
                {
                    tField.Add(tColumn, tRow, tColors[static_cast<int>(tClass)], 1.0f);
                }
            }
        }
        tField.Blur(tPlan.mHalfX, tPlan.mHalfY);
    }

    // 5) Ara sonuclar: kara orani / kaplama payli alanda (piksel basina tek sinif: 0 veya 1),
    // kara rengi cikti piksellerinde. Bukme kara-suyu degistirmedigi icin tCells ile ayni.
    ImageBuffers tBuffers;
    tBuffers.Reset(pOutSize, tPad);
    tBuffers.mMetersPerPixelX = tMppX;
    tBuffers.mMetersPerPixelY = tMppY;
    tBuffers.mLatitude = tCenterLat;
    tBuffers.mU0 = static_cast<double>(tChunkColumn) / tChunks;
    tBuffers.mV0 = static_cast<double>(tChunkRow) / tChunks;
    tBuffers.mDu = tDegreesPerPixel;
    tBuffers.mDv = tDegreesPerPixel;
    for (int tRow = 0; tRow < tPadded; ++tRow)
    {
        for (int tColumn = 0; tColumn < tPadded; ++tColumn)
        {
            const size_t tIndex = static_cast<size_t>(tRow) * tPadded + tColumn;
            const TerrainClass tClass = static_cast<TerrainClass>(tWarped[tIndex]);
            if (tClass == TerrainClass::None)
            {
                continue;
            }
            tBuffers.mLandFraction[tIndex] = tClass == TerrainClass::Water ? 0.0f : 1.0f;
            tBuffers.mCoverage[tIndex] = 1.0f;
            const int tOutColumn = tColumn - tPad;
            const int tOutRow = tRow - tPad;
            if (tClass == TerrainClass::Water || tOutColumn < 0 || tOutRow < 0 || tOutColumn >= pOutSize ||
                tOutRow >= pOutSize)
            {
                continue;
            }
            const TerrainColor& tOwnColor = tColors[static_cast<int>(tClass)];
            double tRgb[3] = {static_cast<double>(tOwnColor.mR), static_cast<double>(tOwnColor.mG),
                              static_cast<double>(tOwnColor.mB)};
            if (tPlan.mPad > 0)
            {
                tField.ColorAt(tColumn, tRow, tRgb);  // false ise kendi rengi kalir
            }
            float* tOut = &tBuffers.mLandRgb[(static_cast<size_t>(tOutRow) * pOutSize + tOutColumn) * 3];
            tOut[0] = static_cast<float>(tRgb[0]);
            tOut[1] = static_cast<float>(tRgb[1]);
            tOut[2] = static_cast<float>(tRgb[2]);
        }
    }

    // 6) Kabartma, VFR bantlari, su, kiyi cizgisi
    ComposeImage(tBuffers, pPalette, pStyle, pElevation, pRgba);
}

void TerrainRaster::BleedTransparent(int pSize, std::vector<uint8_t>* pRgba)
{
    const size_t tSize = static_cast<size_t>(pSize);
    if (pSize <= 0 || pRgba->size() < tSize * tSize * 4)
    {
        return;
    }
    uint8_t* tData = pRgba->data();
    auto tPixel = [&](size_t pRow, size_t pColumn)
    {
        return tData + (pRow * tSize + pColumn) * 4;
    };
    // 1) Satir icinde: saydam piksel soldaki ve sagdaki opaklardan yakin olanin rengini alir.
    // Kaynak hep ORIJINAL opak piksel (soldaki opaklarin yeri once isaretlenir).
    std::vector<uint8_t> tRowHasOpaque(tSize, 0);
    std::vector<int> tNearest(tSize, -1);
    for (size_t tRow = 0; tRow < tSize; ++tRow)
    {
        int tLast = -1;
        for (size_t tColumn = 0; tColumn < tSize; ++tColumn)
        {
            if (tPixel(tRow, tColumn)[3] != 0)
            {
                tLast = static_cast<int>(tColumn);
            }
            tNearest[tColumn] = tLast;
        }
        if (tLast < 0)
        {
            continue;  // tamamen saydam satir: 2. adimda
        }
        tRowHasOpaque[tRow] = 1;
        int tNext = -1;
        for (size_t tColumn = tSize; tColumn-- > 0;)
        {
            uint8_t* tTarget = tPixel(tRow, tColumn);
            if (tTarget[3] != 0)
            {
                tNext = static_cast<int>(tColumn);
                continue;
            }
            const int tLeft = tNearest[tColumn];
            int tSource = tLeft;
            if (tLeft < 0 || (tNext >= 0 && tNext - static_cast<int>(tColumn) < static_cast<int>(tColumn) - tLeft))
            {
                tSource = tNext;
            }
            const uint8_t* tFrom = tPixel(tRow, static_cast<size_t>(tSource));
            tTarget[0] = tFrom[0];
            tTarget[1] = tFrom[1];
            tTarget[2] = tFrom[2];
        }
    }
    // 2) Tamamen saydam satirlar: en yakin dolu satirin (1. adimdan sonra tam dolu) renklerini alir
    std::vector<int> tAbove(tSize, -1);
    int tLastFilled = -1;
    for (size_t tRow = 0; tRow < tSize; ++tRow)
    {
        if (tRowHasOpaque[tRow])
        {
            tLastFilled = static_cast<int>(tRow);
        }
        tAbove[tRow] = tLastFilled;
    }
    int tBelow = -1;
    for (size_t tRow = tSize; tRow-- > 0;)
    {
        if (tRowHasOpaque[tRow])
        {
            tBelow = static_cast<int>(tRow);
            continue;
        }
        const int tRowIndex = static_cast<int>(tRow);
        int tSource = tAbove[tRow];
        if (tSource < 0 || (tBelow >= 0 && tBelow - tRowIndex < tRowIndex - tSource))
        {
            tSource = tBelow;
        }
        if (tSource < 0)
        {
            return;  // tamamen saydam goruntu
        }
        for (size_t tColumn = 0; tColumn < tSize; ++tColumn)
        {
            const uint8_t* tFrom = tPixel(static_cast<size_t>(tSource), tColumn);
            std::copy(tFrom, tFrom + 3, tPixel(tRow, tColumn));
        }
    }
}

bool TerrainRaster::Save(const std::string& pPath, const std::string& pKey) const
{
    std::ofstream tFile(pPath, std::ios::binary | std::ios::trunc);
    if (!tFile)
    {
        return false;
    }
    tFile.write(kMagic, 4);
    WriteU32(tFile, kFileVersion);
    WriteU32(tFile, static_cast<uint32_t>(pKey.size()));
    tFile.write(pKey.data(), static_cast<std::streamsize>(pKey.size()));
    WriteU32(tFile, static_cast<uint32_t>(mWest + 1000));  // eksi sayi olmasin
    WriteU32(tFile, static_cast<uint32_t>(mSouth + 1000));

    // Sinif raster'i: (tekrar sayisi uint16, deger uint8) ciftleri. Ayni turden
    // genis alanlar (deniz, orman) cok iyi sikisir.
    std::vector<uint8_t> tRuns;
    size_t tIndex = 0;
    while (tIndex < mClasses.size())
    {
        const uint8_t tValue = mClasses[tIndex];
        size_t tRun = 1;
        while (tIndex + tRun < mClasses.size() && mClasses[tIndex + tRun] == tValue && tRun < 65535)
        {
            ++tRun;
        }
        tRuns.push_back(static_cast<uint8_t>(tRun & 0xFF));
        tRuns.push_back(static_cast<uint8_t>(tRun >> 8));
        tRuns.push_back(tValue);
        tIndex += tRun;
    }
    WriteU32(tFile, static_cast<uint32_t>(tRuns.size()));
    tFile.write(reinterpret_cast<const char*>(tRuns.data()), static_cast<std::streamsize>(tRuns.size()));
    return static_cast<bool>(tFile);
}

bool TerrainRaster::Load(const std::string& pPath, const std::string& pKey)
{
    std::ifstream tFile(pPath, std::ios::binary);
    char tMagic[4] = {0, 0, 0, 0};
    uint32_t tVersion = 0;
    uint32_t tKeySize = 0;
    if (!tFile.read(tMagic, 4) || std::memcmp(tMagic, kMagic, 4) != 0 || !ReadU32(tFile, &tVersion) ||
        tVersion != kFileVersion || !ReadU32(tFile, &tKeySize) || tKeySize > (1u << 20))
    {
        return false;
    }
    std::string tKey(tKeySize, '\0');
    if (!tFile.read(&tKey[0], tKeySize) || tKey != pKey)
    {
        return false;  // kaynak DSF'ler degismis: yeniden uretilmeli
    }
    uint32_t tWest = 0;
    uint32_t tSouth = 0;
    uint32_t tRunBytes = 0;
    if (!ReadU32(tFile, &tWest) || !ReadU32(tFile, &tSouth) || !ReadU32(tFile, &tRunBytes) ||
        tRunBytes % 3 != 0)
    {
        return false;
    }
    std::vector<uint8_t> tRuns(tRunBytes);
    if (!tFile.read(reinterpret_cast<char*>(tRuns.data()), static_cast<std::streamsize>(tRunBytes)))
    {
        return false;
    }
    mWest = static_cast<int>(tWest) - 1000;
    mSouth = static_cast<int>(tSouth) - 1000;
    mClasses.clear();
    mClasses.reserve(static_cast<size_t>(kSize) * kSize);
    for (size_t tIndex = 0; tIndex + 2 < tRuns.size(); tIndex += 3)
    {
        const size_t tRun = tRuns[tIndex] | (static_cast<size_t>(tRuns[tIndex + 1]) << 8);
        if (tRuns[tIndex + 2] >= kTerrainClassCount || mClasses.size() + tRun > static_cast<size_t>(kSize) * kSize)
        {
            return false;
        }
        mClasses.insert(mClasses.end(), tRun, tRuns[tIndex + 2]);
    }
    return mClasses.size() == static_cast<size_t>(kSize) * kSize;
}
