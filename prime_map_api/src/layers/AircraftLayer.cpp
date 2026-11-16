#include "layers/AircraftLayer.h"

#include <array>
#include <cmath>
#include <vector>

#include "LabelPlacer.h"
#include "MapView.h"
#include "data/AircraftData.h"
#include "gfx/Canvas.h"

namespace
{

const double kPi = 3.14159265358979323846;
const float kSymbolScale = 1.1f;     // sembol birimi -> boxel
const float kHeadingLineLength = 40.0f;
const Rgba kFillColor = {1.0f, 0.85f, 0.1f, 1.0f};
const Rgba kHeadingColor = {1.0f, 0.85f, 0.1f, 0.8f};
const Rgba kOutlineColor = {0.0f, 0.0f, 0.0f, 0.75f};

struct Vertex
{
    float mX;
    float mY;
};

// Ucak silueti, burun yukarida (+y). Birim: yaklasik 1 boxel.
// Disaridaki koyu kontur bu sirayla cizilir (saat yonunde).
const auto kOutline = std::to_array<Vertex>({
    {0.0f, 14.0f},  {1.6f, 11.0f},  {1.6f, 4.0f},    {12.0f, -1.0f},   {12.0f, -3.5f},  {1.6f, -1.0f},
    {1.6f, -8.0f},  {5.5f, -11.0f}, {5.5f, -13.0f},  {0.0f, -12.0f},   {-5.5f, -13.0f}, {-5.5f, -11.0f},
    {-1.6f, -8.0f}, {-1.6f, -1.0f}, {-12.0f, -3.5f}, {-12.0f, -1.0f},  {-1.6f, 4.0f},   {-1.6f, 11.0f},
});

// Ayni siluetin ucgenleri (dolgu). Her satir bir ucgen.
const auto kFill = std::to_array<Vertex>({
    {0.0f, 14.0f}, {1.6f, 11.0f}, {-1.6f, 11.0f},
    {-1.6f, 11.0f}, {1.6f, 11.0f}, {1.6f, -8.0f},
    {-1.6f, 11.0f}, {1.6f, -8.0f}, {-1.6f, -8.0f},
    {-1.6f, -8.0f}, {1.6f, -8.0f}, {0.0f, -12.0f},
    {1.6f, 4.0f}, {12.0f, -1.0f}, {12.0f, -3.5f},
    {1.6f, 4.0f}, {12.0f, -3.5f}, {1.6f, -1.0f},
    {-1.6f, 4.0f}, {-12.0f, -3.5f}, {-12.0f, -1.0f},
    {-1.6f, 4.0f}, {-1.6f, -1.0f}, {-12.0f, -3.5f},
    {1.6f, -8.0f}, {5.5f, -11.0f}, {5.5f, -13.0f},
    {1.6f, -8.0f}, {5.5f, -13.0f}, {0.0f, -12.0f},
    {-1.6f, -8.0f}, {-5.5f, -13.0f}, {-5.5f, -11.0f},
    {-1.6f, -8.0f}, {0.0f, -12.0f}, {-5.5f, -13.0f},
});

// Sembol noktasini burun yonune dondurup ekrandaki yerine tasir.
// Yon saat yonunde, kuzeyden (0 = kuzey, 90 = dogu); Mercator acilari
// korudugu icin gercek yon dogrudan kullanilir.
class SymbolTransform
{
public:
    SymbolTransform(const PointF& pCenter, double pHeadingDeg)
        : mCenter(pCenter)
        , mSin(static_cast<float>(std::sin(pHeadingDeg * kPi / 180.0)))
        , mCos(static_cast<float>(std::cos(pHeadingDeg * kPi / 180.0)))
    {
    }

    PointF apply(const Vertex& pVertex) const
    {
        const float tX = pVertex.mX * kSymbolScale;
        const float tY = pVertex.mY * kSymbolScale;
        return {mCenter.mX + tX * mCos + tY * mSin, mCenter.mY - tX * mSin + tY * mCos};
    }

private:
    PointF mCenter;
    float mSin;
    float mCos;
};

}

AircraftLayer::AircraftLayer(const AircraftData& pAircraft)
    : Layer("aircraft", "Aircraft", true)
    , mAircraft(pAircraft)
{
}

void AircraftLayer::draw(const drawContext& pContext)
{
    const AircraftState& tState = mAircraft.state();
    if (!tState.mValid)
    {
        return;
    }

    const PointF tCenter = pContext.mView->toScreen(tState.mPosition);
    const SymbolTransform tTransform(tCenter, tState.mHeadingDeg);

    // Etiketler ucagin ustune yazilmasin
    if (pContext.mLabels)
    {
        const double tHalf = 16.0 * kSymbolScale;
        RectD tBox;
        tBox.mLeft = tCenter.mX - tHalf;
        tBox.mRight = tCenter.mX + tHalf;
        tBox.mBottom = tCenter.mY - tHalf;
        tBox.mTop = tCenter.mY + tHalf;
        pContext.mLabels->reserve(tBox);
    }

    Gfx::Mesh tMesh;

    tMesh.addLine(tTransform.apply({0.0f, 14.0f}), tTransform.apply({0.0f, 14.0f + kHeadingLineLength}), 2.0f,
                  kHeadingColor);

    // Koyu kontur (her zemin renginde okunur kalsin), ustune sari dolgu
    std::vector<PointF> tOutline;
    for (const Vertex& tVertex : kOutline)
    {
        tOutline.push_back(tTransform.apply(tVertex));
    }
    tMesh.addPolyline(tOutline, true, 3.0f, kOutlineColor);

    for (size_t tIndex = 0; tIndex + 2 < kFill.size(); tIndex += 3)
    {
        tMesh.addTriangle(tTransform.apply(kFill[tIndex]), tTransform.apply(kFill[tIndex + 1]),
                          tTransform.apply(kFill[tIndex + 2]), kFillColor);
    }

    pContext.mCanvas->draw(tMesh);
}
