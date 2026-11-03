// GridLayer.cpp
#include "layers/GridLayer.h"

#include <cmath>

#include "GLInclude.h"
#include "MapFormat.h"
#include "MapView.h"
#include "ui/UiDraw.h"

namespace
{

const double kMinSpacing = 90.0;  // iki izgara cizgisi arasi en az bu kadar boxel
const Rgba kLineColor = {0.78f, 0.84f, 0.92f, 0.22f};
const Rgba kLabelColor = {0.70f, 0.76f, 0.84f, 1.0f};

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

    Ui::UseFlatState();
    glColor4f(kLineColor.mR, kLineColor.mG, kLineColor.mB, kLineColor.mA);
    glLineWidth(1.0f);
    glBegin(GL_LINES);

    // Boylam cizgileri (dikey)
    long long tFirst = 0;
    long long tLast = 0;
    StepRange(tBounds.mWest, tBounds.mEast, tStep, &tFirst, &tLast);
    for (long long tIndex = tFirst; tIndex <= tLast; ++tIndex)
    {
        double tX = 0.0;
        double tY = 0.0;
        tView.LatLonToBoxel({tCenter.mLat, tIndex * tStep}, &tX, &tY);
        glVertex2f(static_cast<float>(tX) + 0.5f, static_cast<float>(pContext.mArea.mBottom));
        glVertex2f(static_cast<float>(tX) + 0.5f, static_cast<float>(pContext.mArea.mTop));
    }

    // Enlem cizgileri (yatay)
    StepRange(tBounds.mSouth, tBounds.mNorth, tStep, &tFirst, &tLast);
    for (long long tIndex = tFirst; tIndex <= tLast; ++tIndex)
    {
        double tX = 0.0;
        double tY = 0.0;
        tView.LatLonToBoxel({tIndex * tStep, tCenter.mLon}, &tX, &tY);
        glVertex2f(static_cast<float>(pContext.mArea.mLeft), static_cast<float>(tY) + 0.5f);
        glVertex2f(static_cast<float>(pContext.mArea.mRight), static_cast<float>(tY) + 0.5f);
    }

    glEnd();
}

void GridLayer::DrawLabels(const DrawContext& pContext)
{
    const MapView& tView = *pContext.mView;
    const GeoBounds tBounds = tView.VisibleBounds();
    const double tStep = StepFor(pContext);
    const LatLon tCenter = tView.Center();
    const RectD& tClear = pContext.mClearArea;
    char tLabel[32];

    // Boylam etiketleri: alt kenarda, cizginin hemen saginda
    long long tFirst = 0;
    long long tLast = 0;
    StepRange(tBounds.mWest, tBounds.mEast, tStep, &tFirst, &tLast);
    for (long long tIndex = tFirst; tIndex <= tLast; ++tIndex)
    {
        double tX = 0.0;
        double tY = 0.0;
        tView.LatLonToBoxel({tCenter.mLat, tIndex * tStep}, &tX, &tY);
        if (tX < tClear.mLeft || tX > tClear.mRight - 40)
        {
            continue;
        }
        FormatGridLabel(NormalizeLon(tIndex * tStep), false, tStep, tLabel, sizeof(tLabel));
        Ui::Text(static_cast<int>(tX) + 4, static_cast<int>(tClear.mBottom) + 5, tLabel, kLabelColor);
    }

    // Enlem etiketleri: sol kenarda, cizginin hemen ustunde
    StepRange(tBounds.mSouth, tBounds.mNorth, tStep, &tFirst, &tLast);
    for (long long tIndex = tFirst; tIndex <= tLast; ++tIndex)
    {
        double tX = 0.0;
        double tY = 0.0;
        tView.LatLonToBoxel({tIndex * tStep, tCenter.mLon}, &tX, &tY);
        if (tY < tClear.mBottom + 20 || tY > tClear.mTop - 14)
        {
            continue;
        }
        FormatGridLabel(tIndex * tStep, true, tStep, tLabel, sizeof(tLabel));
        Ui::Text(static_cast<int>(tClear.mLeft) + 5, static_cast<int>(tY) + 4, tLabel, kLabelColor);
    }
}

double GridLayer::StepFor(const DrawContext& pContext)
{
    // Boylam yonunde 1 derece kac boxel (Mercator'da her enlemde ayni)
    const double tBoxelsPerDegree = pContext.mView->WorldSize() / 360.0;
    return ChooseGridStepDeg(tBoxelsPerDegree, kMinSpacing);
}
