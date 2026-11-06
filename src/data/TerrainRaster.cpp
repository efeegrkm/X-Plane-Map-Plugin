// TerrainRaster.cpp
#include "data/TerrainRaster.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>

#include "data/DsfReader.h"
#include "data/LandColorField.h"
#include "data/TerrainNoise.h"
#include "data/TerrainTriangles.h"

namespace
{

const char kMagic[4] = {'M', 'P', 'T', 'R'};
const uint32_t kFileVersion = 2;  // 2: rakim raster'i eklendi (VFR). Eski surum yeniden uretilir.

const double kPi = 3.14159265358979323846;
const double kMetersPerDegreeLat = 110574.0;
const double kMetersPerDegree = 111320.0;  // enlem derecesi (yaklasik; burada yeterli)
const double kSunAzimuthDeg = 315.0;  // kuzeybati: kartografyadaki alisilmis isik yonu
const double kSunAltitudeDeg = 45.0;
const double kExaggeration = 2.5;     // kucuk olcekte kabartma gorunsun diye
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
    mShade.clear();
    mElevation.clear();
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

void TerrainRaster::ComputeShading(const DsfRaster& pElevation)
{
    const int tWidth = pElevation.mWidth;
    const int tHeight = pElevation.mHeight;
    if (tWidth < 3 || tHeight < 3)
    {
        return;
    }
    // DEM hucre boyu (metre); kose esasli DEM'de 1 derecede (N - 1) aralik vardir
    const double tLatMid = (mSouth + 0.5) * kPi / 180.0;
    const double tIntervalsX = pElevation.mPostCentric ? tWidth - 1 : tWidth;
    const double tIntervalsY = pElevation.mPostCentric ? tHeight - 1 : tHeight;
    const double tCellX = kMetersPerDegreeLat * std::cos(tLatMid) / tIntervalsX;
    const double tCellY = kMetersPerDegreeLat / tIntervalsY;

    const double tAzimuth = kSunAzimuthDeg * kPi / 180.0;
    const double tAltitude = kSunAltitudeDeg * kPi / 180.0;
    const double tSunX = std::sin(tAzimuth) * std::cos(tAltitude);  // dogu bileseni
    const double tSunY = std::cos(tAzimuth) * std::cos(tAltitude);  // kuzey bileseni
    const double tSunZ = std::sin(tAltitude);

    auto tElevation = [&](int pX, int pY)
    {
        return static_cast<double>(pElevation.mValues[static_cast<size_t>(ClampInt(pY, 0, tHeight - 1)) * tWidth +
                                                      ClampInt(pX, 0, tWidth - 1)]);
    };

    mShade.assign(static_cast<size_t>(kShadeSize) * kShadeSize, 0);
    for (int tRow = 0; tRow < kShadeSize; ++tRow)
    {
        // Golge raster'i satir 0 = kuzey; DEM satir 0 = guney
        const double tV = (tRow + 0.5) / kShadeSize;  // 0 = kuzey
        const int tDemY = ClampInt(static_cast<int>((1.0 - tV) * (tHeight - 1) + 0.5), 0, tHeight - 1);
        for (int tColumn = 0; tColumn < kShadeSize; ++tColumn)
        {
            const double tU = (tColumn + 0.5) / kShadeSize;
            const int tDemX = ClampInt(static_cast<int>(tU * (tWidth - 1) + 0.5), 0, tWidth - 1);
            const double tDzDx =
                (tElevation(tDemX + 1, tDemY) - tElevation(tDemX - 1, tDemY)) / (2.0 * tCellX) * kExaggeration;
            const double tDzDy =
                (tElevation(tDemX, tDemY + 1) - tElevation(tDemX, tDemY - 1)) / (2.0 * tCellY) * kExaggeration;
            // Yuzey normali (-dz/dx, -dz/dy, 1) ile gunes yonunun acisi
            const double tLength = std::sqrt(tDzDx * tDzDx + tDzDy * tDzDy + 1.0);
            double tLight = (-tDzDx * tSunX - tDzDy * tSunY + tSunZ) / tLength;
            tLight = tLight < 0.0 ? 0.0 : tLight;
            mShade[static_cast<size_t>(tRow) * kShadeSize + tColumn] = static_cast<uint8_t>(tLight * 255.0 + 0.5);
        }
    }

    // Rakim raster'i: hucre merkezindeki DEM degeri (satir 0 = kuzey)
    mElevation.assign(static_cast<size_t>(kElevationSize) * kElevationSize, 0);
    for (int tRow = 0; tRow < kElevationSize; ++tRow)
    {
        const double tV = (tRow + 0.5) / kElevationSize;
        const int tDemY = ClampInt(static_cast<int>((1.0 - tV) * (tHeight - 1) + 0.5), 0, tHeight - 1);
        for (int tColumn = 0; tColumn < kElevationSize; ++tColumn)
        {
            const double tU = (tColumn + 0.5) / kElevationSize;
            const int tDemX = ClampInt(static_cast<int>(tU * (tWidth - 1) + 0.5), 0, tWidth - 1);
            const double tMeters = std::min(std::max(tElevation(tDemX, tDemY), -1000.0), 9000.0);
            mElevation[static_cast<size_t>(tRow) * kElevationSize + tColumn] = static_cast<int16_t>(tMeters);
        }
    }
}

bool TerrainRaster::HasElevation() const
{
    return !mElevation.empty();
}

double TerrainRaster::ElevationAt(double pU, double pV) const
{
    if (mElevation.empty())
    {
        return 0.0;
    }
    const double tX = pU * kElevationSize - 0.5;
    const double tY = pV * kElevationSize - 0.5;
    const int tX0 = ClampInt(static_cast<int>(std::floor(tX)), 0, kElevationSize - 1);
    const int tY0 = ClampInt(static_cast<int>(std::floor(tY)), 0, kElevationSize - 1);
    const int tX1 = ClampInt(tX0 + 1, 0, kElevationSize - 1);
    const int tY1 = ClampInt(tY0 + 1, 0, kElevationSize - 1);
    const double tFx = std::min(std::max(tX - tX0, 0.0), 1.0);
    const double tFy = std::min(std::max(tY - tY0, 0.0), 1.0);
    auto tAt = [&](int pColumn, int pRow)
    {
        return static_cast<double>(mElevation[static_cast<size_t>(pRow) * kElevationSize + pColumn]);
    };
    const double tTop = tAt(tX0, tY0) * (1.0 - tFx) + tAt(tX1, tY0) * tFx;
    const double tBottom = tAt(tX0, tY1) * (1.0 - tFx) + tAt(tX1, tY1) * tFx;
    return tTop * (1.0 - tFy) + tBottom * tFy;
}

bool TerrainRaster::HasShading() const
{
    return !mShade.empty();
}

TerrainClass TerrainRaster::ClassAt(int pX, int pY) const
{
    return static_cast<TerrainClass>(mClasses[static_cast<size_t>(pY) * kSize + pX]);
}

int TerrainRaster::CountOf(TerrainClass pClass) const
{
    return static_cast<int>(std::count(mClasses.begin(), mClasses.end(), static_cast<uint8_t>(pClass)));
}

double TerrainRaster::ShadeAt(double pU, double pV) const
{
    const double tX = pU * kShadeSize - 0.5;
    const double tY = pV * kShadeSize - 0.5;
    const int tX0 = ClampInt(static_cast<int>(std::floor(tX)), 0, kShadeSize - 1);
    const int tY0 = ClampInt(static_cast<int>(std::floor(tY)), 0, kShadeSize - 1);
    const int tX1 = ClampInt(tX0 + 1, 0, kShadeSize - 1);
    const int tY1 = ClampInt(tY0 + 1, 0, kShadeSize - 1);
    const double tFx = std::min(std::max(tX - tX0, 0.0), 1.0);
    const double tFy = std::min(std::max(tY - tY0, 0.0), 1.0);
    auto tAt = [&](int pColumn, int pRow)
    {
        return mShade[static_cast<size_t>(pRow) * kShadeSize + pColumn];
    };
    const double tTop = tAt(tX0, tY0) * (1.0 - tFx) + tAt(tX1, tY0) * tFx;
    const double tBottom = tAt(tX0, tY1) * (1.0 - tFx) + tAt(tX1, tY1) * tFx;
    return (tTop * (1.0 - tFy) + tBottom * tFy) / 255.0;
}

double TerrainRaster::ShadeAtSmooth(double pU, double pV) const
{
    // Catmull-Rom: 4x4 komsu; cift dogrusalin yakinda gorunen kirik (elmas) deseni olmaz
    auto tCubic = [](double pP0, double pP1, double pP2, double pP3, double pT)
    {
        return 0.5 * (2.0 * pP1 + (pP2 - pP0) * pT + (2.0 * pP0 - 5.0 * pP1 + 4.0 * pP2 - pP3) * pT * pT +
                      (3.0 * pP1 - pP0 - 3.0 * pP2 + pP3) * pT * pT * pT);
    };
    const double tX = pU * kShadeSize - 0.5;
    const double tY = pV * kShadeSize - 0.5;
    const int tBaseX = static_cast<int>(std::floor(tX));
    const int tBaseY = static_cast<int>(std::floor(tY));
    const double tFx = tX - tBaseX;
    const double tFy = tY - tBaseY;
    double tRows[4];
    for (int tRow = 0; tRow < 4; ++tRow)
    {
        const int tY0 = ClampInt(tBaseY + tRow - 1, 0, kShadeSize - 1);
        double tValues[4];
        for (int tColumn = 0; tColumn < 4; ++tColumn)
        {
            const int tX0 = ClampInt(tBaseX + tColumn - 1, 0, kShadeSize - 1);
            tValues[tColumn] = mShade[static_cast<size_t>(tY0) * kShadeSize + tX0];
        }
        tRows[tRow] = tCubic(tValues[0], tValues[1], tValues[2], tValues[3], tFx);
    }
    const double tValue = tCubic(tRows[0], tRows[1], tRows[2], tRows[3], tFy) / 255.0;
    return std::min(std::max(tValue, 0.0), 1.0);
}

void TerrainRaster::BuildImage(int pX0, int pY0, int pX1, int pY1, int pOutSize, const TerrainPalette& pPalette,
                               const TerrainImageStyle& pStyle, std::vector<uint8_t>* pRgba) const
{
    pRgba->assign(static_cast<size_t>(pOutSize) * pOutSize * 4, 0);
    const double tBlockX = static_cast<double>(pX1 - pX0) / pOutSize;
    const double tBlockY = static_cast<double>(pY1 - pY0) / pOutSize;
    const int tSamplesX = std::max(1, std::min(kMaxSamplesPerAxis, static_cast<int>(tBlockX)));
    const int tSamplesY = std::max(1, std::min(kMaxSamplesPerAxis, static_cast<int>(tBlockY)));

    // 0) Planlar: yumusatma (blur) ve sinir bukmesi; pad ikisinin toplami
    const double tCenterLat = mSouth + 1.0 - (pY0 + pY1) * 0.5 / kSize;  // satir 0 = kuzey
    const double tCos = std::max(0.05, std::cos(tCenterLat * kPi / 180.0));
    const double tMppX = tBlockX / kSize * kMetersPerDegree * tCos;  // pikselin yerdeki boyu (metre)
    const double tMppY = tBlockY / kSize * kMetersPerDegree;
    const LandBlendPlan tPlan = PlanForImage(pPalette, pStyle, tBlockX / kSize, tBlockY / kSize, tCenterLat);
    const WarpPlan tWarpPlan = PlanWarp(pPalette, pStyle, tMppX, tMppY);
    const int tPad = tPlan.mPad + tWarpPlan.mPad;
    // Bu doku icin sinif renkleri (yakin zoomda referans kara tonuna dogru karisik)
    const auto tColors = pPalette.MixedColors(pPalette.CloseupMixAt(tMppY));
    LandColorField tField;
    if (tPlan.mPad > 0)
    {
        tField.Reset(pOutSize + 2 * tPad, tPlan.mFactor);
    }
    // Kayma alani: cikti piksel izgarasinda, izgara adimli (gurultu cagrisi maliyeti icin)
    const int tPadded = pOutSize + 2 * tPad;
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

    // 1) Payli alanda kara orneklerini alana ekle (su ve None eklenmez; karonun disi veri yok)
    if (tPlan.mPad > 0)
    {
        for (int tRow = -tPad; tRow < pOutSize + tPad; ++tRow)
        {
            for (int tColumn = -tPad; tColumn < pOutSize + tPad; ++tColumn)
            {
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
                        if (tClass != TerrainClass::None && tClass != TerrainClass::Water)
                        {
                            tField.Add(tColumn + tPad, tRow + tPad, tColors[static_cast<int>(tClass)], 1.0f);
                        }
                    }
                }
            }
        }
        tField.Blur(tPlan.mHalfX, tPlan.mHalfY);
    }

    // 2) Piksel renkleri: kara ornekleri ayrı toplanir; plan doluysa kara rengi alandan gelir
    const TerrainColor& tWaterColor = pPalette.ColorOf(TerrainClass::Water);
    for (int tRow = 0; tRow < pOutSize; ++tRow)
    {
        for (int tColumn = 0; tColumn < pOutSize; ++tColumn)
        {
            double tLandR = 0.0;
            double tLandG = 0.0;
            double tLandB = 0.0;
            int tCovered = 0;
            int tLand = 0;
            for (int tSampleY = 0; tSampleY < tSamplesY; ++tSampleY)
            {
                const int tY = ClampInt(pY0 + static_cast<int>((tRow + (tSampleY + 0.5) / tSamplesY) * tBlockY), 0,
                                        kSize - 1);
                for (int tSampleX = 0; tSampleX < tSamplesX; ++tSampleX)
                {
                    const int tX = ClampInt(
                        pX0 + static_cast<int>((tColumn + (tSampleX + 0.5) / tSamplesX) * tBlockX), 0, kSize - 1);
                    const TerrainClass tClass = tWarpedClass(tX, tY, tColumn, tRow);
                    if (tClass == TerrainClass::None)
                    {
                        continue;
                    }
                    ++tCovered;
                    if (tClass != TerrainClass::Water)
                    {
                        ++tLand;
                        const TerrainColor& tColor = tColors[static_cast<int>(tClass)];
                        tLandR += tColor.mR;
                        tLandG += tColor.mG;
                        tLandB += tColor.mB;
                    }
                }
            }
            if (tPlan.mPad > 0 && tLand > 0)
            {
                double tBlended[3];
                if (tField.ColorAt(tColumn + tPad, tRow + tPad, tBlended))
                {
                    tLandR = tBlended[0] * tLand;
                    tLandG = tBlended[1] * tLand;
                    tLandB = tBlended[2] * tLand;
                }
            }
            const int tWater = tCovered - tLand;
            const double tR = tLandR + static_cast<double>(tWaterColor.mR) * tWater;
            const double tG = tLandG + static_cast<double>(tWaterColor.mG) * tWater;
            const double tB = tLandB + static_cast<double>(tWaterColor.mB) * tWater;
            const double tU = (pX0 + (tColumn + 0.5) * tBlockX) / kSize;
            const double tV = (pY0 + (tRow + 0.5) * tBlockY) / kSize;
            uint8_t* tPixel = pRgba->data() + (static_cast<size_t>(tRow) * pOutSize + tColumn) * 4;
            FinishPixel(tR, tG, tB, tCovered, tLand, tSamplesX * tSamplesY, tU, tV, pPalette, pStyle, false, tPixel);
        }
    }
}

void TerrainRaster::BuildDetailImage(const TerrainTriangles& pTriangles, int pChunksPerSide, int pChunk,
                                     int pOutSize, const TerrainPalette& pPalette, const TerrainImageStyle& pStyle,
                                     std::vector<uint8_t>* pRgba) const
{
    const int tChunks = pChunksPerSide;
    const int tChunkColumn = pChunk % tChunks;
    const int tChunkRow = pChunk / tChunks;

    // 0) Planlar: yumusatma (blur) ve sinir bukmesi; toplam pay ikisinin toplami (ucgen toplama payli calisir)
    const double tDegreesPerPixel = 1.0 / (static_cast<double>(tChunks) * pOutSize);
    const double tCenterLat = mSouth + 1.0 - (tChunkRow + 0.5) / tChunks;
    const double tMppX = tDegreesPerPixel * kMetersPerDegree * std::max(0.05, std::cos(tCenterLat * kPi / 180.0));
    const double tMppY = tDegreesPerPixel * kMetersPerDegree;
    const LandBlendPlan tPlan = PlanForImage(pPalette, pStyle, tDegreesPerPixel, tDegreesPerPixel, tCenterLat);
    const WarpPlan tWarpPlan = PlanWarp(pPalette, pStyle, tMppX, tMppY);
    const int tPad = tPlan.mPad + tWarpPlan.mPad;
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

    // 5) Renk: sadece ic pikseller (golge / rakim bu raster'dan); kara rengi alandan.
    // Kara/su sayimi tWarped'tan: bukme kara-suyu degistirmedigi icin tCells ile ayni.
    pRgba->assign(static_cast<size_t>(pOutSize) * pOutSize * 4, 0);
    for (int tRow = 0; tRow < pOutSize; ++tRow)
    {
        for (int tColumn = 0; tColumn < pOutSize; ++tColumn)
        {
            const TerrainClass tClass =
                static_cast<TerrainClass>(tWarped[static_cast<size_t>(tRow + tPad) * tPadded + (tColumn + tPad)]);
            if (tClass == TerrainClass::None)
            {
                continue;  // saydam: alttaki kaba seviye gorunur
            }
            const TerrainColor& tOwnColor = tColors[static_cast<int>(tClass)];
            double tRgb[3] = {static_cast<double>(tOwnColor.mR), static_cast<double>(tOwnColor.mG),
                              static_cast<double>(tOwnColor.mB)};
            if (tPlan.mPad > 0 && tClass != TerrainClass::Water)
            {
                tField.ColorAt(tColumn + tPad, tRow + tPad, tRgb);  // false ise kendi rengi kalir
            }
            const double tU = (tChunkColumn + (tColumn + 0.5) / pOutSize) / tChunks;
            const double tV = (tChunkRow + (tRow + 0.5) / pOutSize) / tChunks;
            uint8_t* tPixel = pRgba->data() + (static_cast<size_t>(tRow) * pOutSize + tColumn) * 4;
            FinishPixel(tRgb[0], tRgb[1], tRgb[2], 1, tClass == TerrainClass::Water ? 0 : 1, 1, tU, tV, pPalette,
                        pStyle, true, tPixel);
        }
    }
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

void TerrainRaster::FinishPixel(double pR, double pG, double pB, int pCovered, int pLand, int pSamples, double pU,
                                double pV, const TerrainPalette& pPalette, const TerrainImageStyle& pStyle,
                                bool pSmoothShade, uint8_t* pPixel) const
{
    if (pCovered == 0)
    {
        return;  // saydam (pPixel zaten 0)
    }
    const double tStrength = pStyle.mStyle == TerrainStyle::LandWater ? 0.0 : pPalette.ShadingStrength();
    const double tFlatLight = std::sin(kSunAltitudeDeg * kPi / 180.0);  // duz arazinin isigi
    double tR = pR;
    double tG = pG;
    double tB = pB;
    if (pStyle.mStyle != TerrainStyle::Classes)
    {
        // Kara rengi (rakim rampasi veya duz) ve su rengi, kara oraninda karistirilir
        TerrainColor tLandColor = pStyle.mLand;
        if (pStyle.mStyle == TerrainStyle::Elevation && pLand > 0)
        {
            tLandColor = ElevationColor(ElevationAt(pU, pV));
        }
        const int tWater = pCovered - pLand;
        tR = static_cast<double>(tLandColor.mR) * pLand + static_cast<double>(pStyle.mWater.mR) * tWater;
        tG = static_cast<double>(tLandColor.mG) * pLand + static_cast<double>(pStyle.mWater.mG) * tWater;
        tB = static_cast<double>(tLandColor.mB) * pLand + static_cast<double>(pStyle.mWater.mB) * tWater;
    }
    double tFactor = 1.0;
    if (HasShading() && pLand > 0 && tStrength > 0.0)
    {
        // Golge: duz araziye gore aydinlik farki; sadece kara oraninda
        const double tShade = pSmoothShade ? ShadeAtSmooth(pU, pV) : ShadeAt(pU, pV);
        const double tDelta = (tShade - tFlatLight) * 1.8 * tStrength;
        tFactor = 1.0 + tDelta * pLand / pCovered;
        tFactor = std::min(std::max(tFactor, 0.45), 1.55);
    }
    pPixel[0] = static_cast<uint8_t>(std::min(255.0, tR / pCovered * tFactor));
    pPixel[1] = static_cast<uint8_t>(std::min(255.0, tG / pCovered * tFactor));
    pPixel[2] = static_cast<uint8_t>(std::min(255.0, tB / pCovered * tFactor));
    pPixel[3] = static_cast<uint8_t>(255 * pCovered / pSamples);
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
    WriteU32(tFile, static_cast<uint32_t>(mShade.size()));
    tFile.write(reinterpret_cast<const char*>(mShade.data()), static_cast<std::streamsize>(mShade.size()));
    WriteU32(tFile, static_cast<uint32_t>(mElevation.size()));
    tFile.write(reinterpret_cast<const char*>(mElevation.data()),
                static_cast<std::streamsize>(mElevation.size() * sizeof(int16_t)));
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
    if (mClasses.size() != static_cast<size_t>(kSize) * kSize)
    {
        return false;
    }
    uint32_t tShadeBytes = 0;
    if (!ReadU32(tFile, &tShadeBytes) ||
        (tShadeBytes != 0 && tShadeBytes != static_cast<uint32_t>(kShadeSize) * kShadeSize))
    {
        return false;
    }
    mShade.resize(tShadeBytes);
    if (tShadeBytes > 0 &&
        !tFile.read(reinterpret_cast<char*>(mShade.data()), static_cast<std::streamsize>(tShadeBytes)))
    {
        return false;
    }
    uint32_t tElevationCount = 0;
    if (!ReadU32(tFile, &tElevationCount) ||
        (tElevationCount != 0 && tElevationCount != static_cast<uint32_t>(kElevationSize) * kElevationSize))
    {
        return false;
    }
    mElevation.resize(tElevationCount);
    if (tElevationCount == 0)
    {
        return true;  // golgesiz / rakimsiz karo (DEM yoktu)
    }
    return static_cast<bool>(tFile.read(reinterpret_cast<char*>(mElevation.data()),
                                        static_cast<std::streamsize>(tElevationCount * sizeof(int16_t))));
}
