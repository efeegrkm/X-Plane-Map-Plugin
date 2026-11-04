// GridLayer.cpp
#include "layers/GridLayer.h"

#include <cmath>

#include "MapFormat.h"
#include "MapView.h"
#include "gfx/Canvas.h"
#include "ui/UiDraw.h"

namespace
{

const double kMinSpacing = 90.0;  // iki izgara cizgisi arasi en az bu kadar boxel

// Bir dongude [pFrom, pTo] araligindaki pStep katlarini gezmek icin
// baslangic ve bitis indeksi (kayan nokta birikimi olmasin diye indeksle gezilir)
void StepRange(double pFrom, double pTo, double pStep, long long* pFirst, long long* pLast)
{
    *pFirst = static_cast<long long>(std::ceil(pFrom / pStep - 1e-9));
    *pLast = static_cast<long long>(std::floor(pTo / pStep + 1e-9));
}

// Boylami -180..180 araligina getir (etiket icin)
double NormalizeLon(double pLon)
{
    double tLon = std::fmod(pLon + 180.0, 360.0);
    if (tLon < 0.0)
    {
        tLon += 360.0;
    }
    return tLon - 180.0;
}

}  // namespace

GridLayer::GridLayer()
    : Layer("grid", "Lat / lon grid", true)
{
}

void GridLayer::Draw(const DrawContext& pContext)
{
    const MapView& tView = *pContext.mView;
    const GeoBounds tBounds = tView.VisibleBounds();
    const double tStep = StepFor(pContext);
    const LatLon tCenter = tView.Center();

    const float tLeft = static_cast<float>(pContext.mArea.mLeft);
    const float tRight = static_cast<float>(pContext.mArea.mRight);
    const float tBottom = static_cast<float>(pContext.mArea.mBottom);
    const float tTop = static_cast<float>(pContext.mArea.mTop);
    Gfx::Mesh tLines;

    // Boylam cizgileri (dikey). 1 boxel kalinlik, piksel sinirina oturur.
    long long tFirst = 0;
    long long tLast = 0;
    StepRange(tBounds.mWest, tBounds.mEast, tStep, &tFirst, &tLast);
    for (long long tIndex = tFirst; tIndex <= tLast; ++tIndex)
    {
        const float tX = std::floor(tView.ToScreen({tCenter.mLat, tIndex * tStep}).mX) + 0.5f;
        tLines.AddLine({tX, tBottom}, {tX, tTop}, 1.0f, pContext.mTheme->mGridLine);
    }

    // Enlem cizgileri (yatay)
    StepRange(tBounds.mSouth, tBounds.mNorth, tStep, &tFirst, &tLast);
    for (long long tIndex = tFirst; tIndex <= tLast; ++tIndex)
    {
        const float tY = std::floor(tView.ToScreen({tIndex * tStep, tCenter.mLon}).mY) + 0.5f;
        tLines.AddLine({tLeft, tY}, {tRight, tY}, 1.0f, pContext.mTheme->mGridLine);
    }

    pContext.mCanvas->Draw(tLines);
}

void GridLayer::DrawLabels(const DrawContext& pContext)
{
    const MapView& tView = *pContext.mView;
    const GeoBounds tBounds = tView.VisibleBounds();
    const double tStep = StepFor(pContext);
    const LatLon tCenter = tView.Center();
    const RectD& tClear = pContext.mClearArea;

    // Boylam etiketleri: alt kenarda, cizginin hemen saginda
    long long tFirst = 0;
    long long tLast = 0;
    StepRange(tBounds.mWest, tBounds.mEast, tStep, &tFirst, &tLast);
    for (long long tIndex = tFirst; tIndex <= tLast; ++tIndex)
    {
        const float tX = tView.ToScreen({tCenter.mLat, tIndex * tStep}).mX;
        if (tX < tClear.mLeft || tX > tClear.mRight - 40)
        {
            continue;
        }
        Ui::Text(static_cast<int>(tX) + 4, static_cast<int>(tClear.mBottom) + 5,
                 FormatGridLabel(NormalizeLon(tIndex * tStep), false, tStep), pContext.mTheme->mGridLabel);
    }

    // Enlem etiketleri: sol kenarda, cizginin hemen ustunde
    StepRange(tBounds.mSouth, tBounds.mNorth, tStep, &tFirst, &tLast);
    for (long long tIndex = tFirst; tIndex <= tLast; ++tIndex)
    {
        const float tY = tView.ToScreen({tIndex * tStep, tCenter.mLon}).mY;
        if (tY < tClear.mBottom + 20 || tY > tClear.mTop - 14)
        {
            continue;
        }
        Ui::Text(static_cast<int>(tClear.mLeft) + 5, static_cast<int>(tY) + 4,
                 FormatGridLabel(tIndex * tStep, true, tStep), pContext.mTheme->mGridLabel);
    }
}

double GridLayer::StepFor(const DrawContext& pContext)
{
    // Boylam yonunde 1 derece kac boxel (Mercator'da her enlemde ayni)
    const double tBoxelsPerDegree = pContext.mView->WorldSize() / 360.0;
    return ChooseGridStepDeg(tBoxelsPerDegree, kMinSpacing);
}
