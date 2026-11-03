// AircraftLayer.cpp
#include "layers/AircraftLayer.h"

#include <cmath>

#include "GLInclude.h"
#include "LabelPlacer.h"
#include "MapView.h"
#include "data/AircraftData.h"
#include "ui/UiDraw.h"

namespace
{

const double kPi = 3.14159265358979323846;
const float kSymbolScale = 1.1f;     // sembol birimi -> boxel
const float kHeadingLineLength = 40.0f;

struct Vertex
{
    float mX;
    float mY;
};

// Ucak silueti, burun yukarida (+y). Birim: yaklasik 1 boxel.
// Disaridaki koyu kontur bu sirayla cizilir (saat yonunde).
const Vertex kOutline[] = {
    {0.0f, 14.0f},  {1.6f, 11.0f},  {1.6f, 4.0f},    {12.0f, -1.0f},   {12.0f, -3.5f},  {1.6f, -1.0f},
    {1.6f, -8.0f},  {5.5f, -11.0f}, {5.5f, -13.0f},  {0.0f, -12.0f},   {-5.5f, -13.0f}, {-5.5f, -11.0f},
    {-1.6f, -8.0f}, {-1.6f, -1.0f}, {-12.0f, -3.5f}, {-12.0f, -1.0f},  {-1.6f, 4.0f},   {-1.6f, 11.0f},
};

// Ayni siluetin ucgenleri (dolgu). Her satir bir ucgen.
const Vertex kFill[] = {
    // burun
    {0.0f, 14.0f}, {1.6f, 11.0f}, {-1.6f, 11.0f},
    // govde
    {-1.6f, 11.0f}, {1.6f, 11.0f}, {1.6f, -8.0f},
    {-1.6f, 11.0f}, {1.6f, -8.0f}, {-1.6f, -8.0f},
    {-1.6f, -8.0f}, {1.6f, -8.0f}, {0.0f, -12.0f},
    // sag kanat
    {1.6f, 4.0f}, {12.0f, -1.0f}, {12.0f, -3.5f},
    {1.6f, 4.0f}, {12.0f, -3.5f}, {1.6f, -1.0f},
    // sol kanat
    {-1.6f, 4.0f}, {-12.0f, -3.5f}, {-12.0f, -1.0f},
    {-1.6f, 4.0f}, {-1.6f, -1.0f}, {-12.0f, -3.5f},
    // sag yatay kuyruk
    {1.6f, -8.0f}, {5.5f, -11.0f}, {5.5f, -13.0f},
    {1.6f, -8.0f}, {5.5f, -13.0f}, {0.0f, -12.0f},
    // sol yatay kuyruk
    {-1.6f, -8.0f}, {-5.5f, -13.0f}, {-5.5f, -11.0f},
    {-1.6f, -8.0f}, {0.0f, -12.0f}, {-5.5f, -13.0f},
};

// Sembol noktasini burun yonune dondurup ekrandaki yerine tasir.
// Yon saat yonunde, kuzeyden (0 = kuzey, 90 = dogu). Web Mercator acilari
// korudugu icin gercek yon dogrudan kullanilabilir.
void EmitRotated(const Vertex& pVertex, float pCenterX, float pCenterY, float pSin, float pCos)
{
    const float tX = pVertex.mX * kSymbolScale;
    const float tY = pVertex.mY * kSymbolScale;
    glVertex2f(pCenterX + tX * pCos + tY * pSin, pCenterY - tX * pSin + tY * pCos);
}

}  // namespace

AircraftLayer::AircraftLayer(const AircraftData& pAircraft)
    : Layer("ucak", "Ucak", true)
    , mAircraft(pAircraft)
{
}

void AircraftLayer::Draw(const DrawContext& pContext)
{
    const AircraftState& tState = mAircraft.State();
    if (!tState.mValid)
    {
        return;
    }

    double tX = 0.0;
    double tY = 0.0;
    pContext.mView->LatLonToBoxel(tState.mPosition, &tX, &tY);
    const float tCenterX = static_cast<float>(tX);
    const float tCenterY = static_cast<float>(tY);
    const double tHeadingRad = tState.mHeadingDeg * kPi / 180.0;
    const float tSin = static_cast<float>(std::sin(tHeadingRad));
    const float tCos = static_cast<float>(std::cos(tHeadingRad));

    // Etiketler ucagin ustune yazilmasin
    if (pContext.mLabels)
    {
        const double tHalf = 16.0 * kSymbolScale;
        RectD tBox;
        tBox.mLeft = tX - tHalf;
        tBox.mRight = tX + tHalf;
        tBox.mBottom = tY - tHalf;
        tBox.mTop = tY + tHalf;
        pContext.mLabels->Reserve(tBox);
    }

    Ui::UseFlatState();

    // Yon cizgisi: burnun onunden ileri
    glLineWidth(2.0f);
    glColor4f(1.0f, 0.85f, 0.1f, 0.8f);
    glBegin(GL_LINES);
    EmitRotated({0.0f, 14.0f}, tCenterX, tCenterY, tSin, tCos);
    EmitRotated({0.0f, 14.0f + kHeadingLineLength}, tCenterX, tCenterY, tSin, tCos);
    glEnd();

    // Koyu kontur (her zemin renginde okunur kalsin)
    glLineWidth(3.0f);
    glColor4f(0.0f, 0.0f, 0.0f, 0.75f);
    glBegin(GL_LINE_LOOP);
    for (const Vertex& tVertex : kOutline)
    {
        EmitRotated(tVertex, tCenterX, tCenterY, tSin, tCos);
    }
    glEnd();

    // Sari dolgu
    glColor4f(1.0f, 0.85f, 0.1f, 1.0f);
    glBegin(GL_TRIANGLES);
    for (const Vertex& tVertex : kFill)
    {
        EmitRotated(tVertex, tCenterX, tCenterY, tSin, tCos);
    }
    glEnd();

    glLineWidth(1.0f);
}
