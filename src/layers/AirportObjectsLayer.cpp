// AirportObjectsLayer.cpp
#include "layers/AirportObjectsLayer.h"

#include <cctype>
#include <cmath>
#include <format>

#include "LabelPlacer.h"
#include "MapView.h"
#include "data/AirportDetailStore.h"
#include "data/AirportShapes.h"
#include "data/AirportGeometryStore.h"
#include "gfx/Canvas.h"
#include "layers/AirportVisibility.h"
#include "layers/MapSymbols.h"

namespace
{

const double kPi = 3.14159265358979323846;

const double kHelipadMinZoom = 13.0;
const double kBeaconMinZoom = 13.0;
const double kWindsockMinZoom = 14.0;
const double kStartMinZoom = 15.0;
const double kStartLabelMinZoom = 16.5;
const double kLightMinZoom = 15.0;
const double kSignMinZoom = 16.0;
const double kLampMinZoom = 15.0;  // cizgi boyunca isiklar (101-108)
const float kHitSlop = 3.0f;  // fare isabeti icin semboller biraz buyuk sayilir

const Rgba kGate = {0.35f, 0.75f, 1.00f, 1.0f};
const Rgba kTieDown = {0.45f, 0.90f, 0.45f, 1.0f};
const Rgba kHangar = {0.90f, 0.75f, 0.45f, 1.0f};
const Rgba kMiscStart = {0.75f, 0.78f, 0.82f, 1.0f};
const Rgba kHelipad = {0.95f, 0.95f, 0.95f, 1.0f};
const Rgba kHelipadFill = {0.20f, 0.45f, 0.30f, 0.85f};
const Rgba kBeaconRay = {1.00f, 1.00f, 1.00f, 0.95f};
const Rgba kGreen = {0.20f, 0.90f, 0.35f, 1.0f};
const Rgba kYellow = {1.00f, 0.85f, 0.20f, 1.0f};
const Rgba kWhite = {1.00f, 1.00f, 1.00f, 1.0f};
const Rgba kOrange = {1.00f, 0.50f, 0.10f, 1.0f};
const Rgba kRed = {0.95f, 0.20f, 0.18f, 1.0f};
const Rgba kAmber = {1.00f, 0.70f, 0.10f, 1.0f};
const Rgba kSignBlack = {0.08f, 0.08f, 0.08f, 1.0f};
const Rgba kPole = {0.80f, 0.80f, 0.80f, 1.0f};
const Rgba kLabel = {0.80f, 0.90f, 1.00f, 1.0f};

// Sembol koordinati: (pRight, pForward) birimi boxel; ileri = pHeadingDeg yonu
// (saat yonunde, kuzeyden). Web Mercator acilari korudugu icin gercek yon dogrudan kullanilir.
PointF Rotated(const PointF& pCenter, double pHeadingDeg, float pRight, float pForward)
{
    const float tSin = static_cast<float>(std::sin(pHeadingDeg * kPi / 180.0));
    const float tCos = static_cast<float>(std::cos(pHeadingDeg * kPi / 180.0));
    return {pCenter.mX + pRight * tCos + pForward * tSin, pCenter.mY - pRight * tSin + pForward * tCos};
}

// Isik turu (101-108) -> renk. Iki renkli dizilerde (105, 108) lambalar sirayla degisir.
Rgba LampColor(int pType, size_t pIndex)
{
    const Rgba tGreen = {0.30f, 1.00f, 0.40f, 1.0f};
    const Rgba tBlue = {0.35f, 0.55f, 1.00f, 1.0f};
    switch (pType)
    {
        case 101:  // taksi orta hatti
        case 107:  // pistten cikis
            return tGreen;
        case 102:  // taksi kenari
            return tBlue;
        case 103:  // bekleme cizgisi
        case 104:  // pist bekleme (yanip sonen)
            return kAmber;
        case 105:  // pist emniyet bolgesi orta hatti
        case 108:  // piste giris
            return (pIndex % 2 == 0) ? tGreen : kAmber;
        case 106:  // tehlikeli bolge kenari
            return kRed;
        default:
            return kWhite;
    }
}

// Burun yonune donuk ok (ucak / park noktasi)
void AddArrow(Gfx::Mesh& pFill, Gfx::Mesh& pLines, const PointF& pCenter, double pHeadingDeg, float pSize,
              const Rgba& pColor, bool pFilled)
{
    const PointF tTip = Rotated(pCenter, pHeadingDeg, 0.0f, pSize);
    const PointF tLeft = Rotated(pCenter, pHeadingDeg, -pSize * 0.7f, -pSize * 0.7f);
    const PointF tNotch = Rotated(pCenter, pHeadingDeg, 0.0f, -pSize * 0.3f);
    const PointF tRight = Rotated(pCenter, pHeadingDeg, pSize * 0.7f, -pSize * 0.7f);
    if (pFilled)
    {
        pFill.AddTriangle(tTip, tLeft, tNotch, pColor);
        pFill.AddTriangle(tTip, tNotch, tRight, pColor);
    }
    pLines.AddPolyline({tTip, tLeft, tNotch, tRight}, true, 1.2f, pFilled ? Gfx::kOutline : pColor);
}

std::string StartTypeName(const std::string& pType)
{
    if (pType == "gate")
    {
        return "Gate";
    }
    if (pType == "hangar")
    {
        return "Hangar";
    }
    if (pType == "tie_down" || pType == "tie-down")
    {
        return "Tie-down";
    }
    if (pType.empty())
    {
        return "Start location";  // eski 15 satiri: tur bilgisi yok
    }
    return "Ramp start";
}

const Rgba& BeaconColor(int pType)
{
    switch (pType)
    {
        case 2:
            return kYellow;  // deniz ussu: beyaz-sari
        case 3:
            return kGreen;   // heliport: yesil-sari-beyaz
        default:
            return kGreen;   // sivil (beyaz-yesil), askeri (beyaz-beyaz-yesil)
    }
}

const char* BeaconName(int pType)
{
    switch (pType)
    {
        case 1:
            return "white-green flashing (civil airport)";
        case 2:
            return "white-yellow flashing (seaplane base)";
        case 3:
            return "green-yellow-white flashing (heliport)";
        case 4:
            return "white-white-green flashing (military)";
        default:
            return "none";
    }
}

const char* LightObjectName(int pType)
{
    switch (pType)
    {
        case 1:
            return "VASI";
        case 2:
            return "PAPI-4L";
        case 3:
            return "PAPI-4R";
        case 4:
            return "Space Shuttle PAPI";
        case 5:
            return "Tri-colour VASI";
        case 6:
            return "Runway guard (wig-wag)";
        default:
            return "Lighting object";
    }
}

// "jets|turboprops" -> "jets, turboprops"
std::string ReadableList(const std::string& pList, bool pUpper)
{
    std::string tResult;
    for (char tChar : pList)
    {
        if (tChar == '|')
        {
            tResult += ", ";
        }
        else
        {
            tResult += pUpper ? static_cast<char>(std::toupper(static_cast<unsigned char>(tChar))) : tChar;
        }
    }
    return tResult;
}

// Tabela metni "{@L}A1{@R}31R-13L" -> okunur hal "A1 31R-13L"
std::string SignText(const std::string& pText)
{
    std::string tResult;
    size_t tAt = 0;
    while (tAt < pText.size())
    {
        if (pText[tAt] == '{')
        {
            const size_t tClose = pText.find('}', tAt);
            if (tClose == std::string::npos)
            {
                break;
            }
            const std::string tGlyph = pText.substr(tAt + 1, tClose - tAt - 1);
            if (tGlyph.find("@@") != std::string::npos)
            {
                tResult += " / back: ";
            }
            else if (tGlyph.find('^') != std::string::npos)
            {
                tResult += (tGlyph.find("^l") != std::string::npos) ? "<" : ">";
            }
            else if (tGlyph.find('@') == std::string::npos)
            {
                tResult += "[" + tGlyph + "]";  // critical, safety, hazard, no-entry, r1..r3
            }
            else if (!tResult.empty() && tResult.back() != ' ')
            {
                tResult += ' ';  // stil degisti: yeni bolum
            }
            tAt = tClose + 1;
            continue;
        }
        tResult += (pText[tAt] == '_') ? ' ' : pText[tAt];
        ++tAt;
    }
    return tResult;
}

// Tabelanin ilk stil talimati -> zemin rengi
const Rgba& SignColor(const std::string& pText)
{
    const size_t tStyle = pText.find('@');
    if (tStyle == std::string::npos || tStyle + 1 >= pText.size())
    {
        return kYellow;
    }
    switch (pText[tStyle + 1])
    {
        case 'R':
            return kRed;
        case 'L':
        case 'B':
            return kSignBlack;
        default:
            return kYellow;
    }
}

}  // namespace

AirportObjectsLayer::AirportObjectsLayer(const AirportGeometryStore& pSummaries, AirportDetailStore& pDetails)
    : Layer("airport_objects", "Airport objects", true)
    , mSummaries(pSummaries)
    , mDetails(pDetails)
{
}

void AirportObjectsLayer::Draw(const DrawContext& pContext)
{
    mMarkers.clear();
    const MapView& tView = *pContext.mView;
    const double tZoom = tView.Zoom();
    if (tZoom < kHelipadMinZoom)
    {
        return;
    }
    FindVisibleAirports(tView, mSummaries, mQuery, mVisible);

    Gfx::Mesh tFills;  // dolgular (ustune konturlu cizgiler)
    Gfx::OutlinedLines tLines(MapSymbols::kOutlineExtra);
    Gfx::Mesh tThin;   // konturu olmayan ince cizgiler
    Gfx::Mesh tLamps;  // isik noktalari (en altta: semboller ustune binsin)
    const RectD& tArea = pContext.mArea;
    auto tOnScreen = [&](const PointF& pPoint)
    {
        return tArea.Contains(pPoint.mX, pPoint.mY);
    };

    for (int tIndex : mVisible)
    {
        const LoadedAirport* tLoaded = mDetails.Use(tIndex);
        if (!tLoaded)
        {
            continue;  // yuzey katmani "yukleniyor" kutusunu zaten ciziyor
        }
        const AirportDetail& tDetail = tLoaded->mDetail;

        // Isik dizileri: lambalar GPU'daki sekillerle ayni donusumle (kaydirma + olcek)
        if (tZoom >= kLampMinZoom)
        {
            const AirportShapes& tShapes = tLoaded->mShapes;
            const PointF tOffset = tView.MercatorToScreen(tShapes.mOriginX, tShapes.mOriginY);
            const float tScale = static_cast<float>(tView.WorldSize());
            const double tHalf = tZoom >= 16.5 ? 1.4 : 1.0;
            for (size_t tLamp = 0; tLamp < tShapes.mLights.size(); ++tLamp)
            {
                const LightPoint& tLight = tShapes.mLights[tLamp];
                const PointF tPoint = {tOffset.mX + tLight.mPoint.mX * tScale, tOffset.mY + tLight.mPoint.mY * tScale};
                if (!tOnScreen(tPoint))
                {
                    continue;
                }
                RectD tBox;
                tBox.mLeft = tPoint.mX - tHalf;
                tBox.mRight = tPoint.mX + tHalf;
                tBox.mBottom = tPoint.mY - tHalf;
                tBox.mTop = tPoint.mY + tHalf;
                tLamps.AddRect(tBox, LampColor(tLight.mType, tLamp));
            }
        }
        const double tMetersToBoxels = 1.0 / tView.MetersPerBoxelAt(mSummaries.At(tIndex).mReference.mLat);

        // Helipad: gercek boyunda (en az 9 boxel) cember + yone donuk "H"
        for (const AptHelipad& tPad : tDetail.mHelipads)
        {
            const PointF tCenter = tView.ToScreen(tPad.mCenter);
            if (!tOnScreen(tCenter))
            {
                continue;
            }
            const float tRadius = std::fmax(static_cast<float>(std::fmax(tPad.mLengthM, tPad.mWidthM) * 0.5 *
                                                              tMetersToBoxels),
                                            9.0f);
            tFills.AddDisc(tCenter, tRadius, kHelipadFill);
            tLines.AddRing(tCenter, tRadius, 1.2f, kHelipad);
            const float tH = tRadius * 0.5f;
            tLines.AddLine(Rotated(tCenter, tPad.mHeadingDeg, -tH * 0.6f, -tH),
                           Rotated(tCenter, tPad.mHeadingDeg, -tH * 0.6f, tH), 1.6f, kHelipad);
            tLines.AddLine(Rotated(tCenter, tPad.mHeadingDeg, tH * 0.6f, -tH),
                           Rotated(tCenter, tPad.mHeadingDeg, tH * 0.6f, tH), 1.6f, kHelipad);
            tLines.AddLine(Rotated(tCenter, tPad.mHeadingDeg, -tH * 0.6f, 0.0f),
                           Rotated(tCenter, tPad.mHeadingDeg, tH * 0.6f, 0.0f), 1.6f, kHelipad);
            mMarkers.push_back({tCenter, tRadius,
                                std::format("Helipad {}  {:.0f} x {:.0f} m, {}, heading {:.0f}", tPad.mName,
                                            tPad.mLengthM, tPad.mWidthM, SurfaceName(tPad.mSurfaceCode),
                                            tPad.mHeadingDeg),
                                ""});
        }

        // Beacon: 8 isinli yildiz, ortada beacon renginde nokta
        for (const AptBeacon& tBeacon : tDetail.mBeacons)
        {
            const PointF tCenter = tView.ToScreen(tBeacon.mPosition);
            if (tBeacon.mType == 0 || !tOnScreen(tCenter))
            {
                continue;
            }
            for (int tRay = 0; tRay < 8; ++tRay)
            {
                const double tAngle = tRay * 45.0;
                tLines.AddLine(Rotated(tCenter, tAngle, 0.0f, 4.0f), Rotated(tCenter, tAngle, 0.0f, 8.0f), 1.2f,
                               kBeaconRay);
            }
            tFills.AddDisc(tCenter, 3.5f, BeaconColor(tBeacon.mType));
            mMarkers.push_back({tCenter, 8.0f, std::format("Beacon: {}", BeaconName(tBeacon.mType)), ""});
        }

        // Ruzgar tulumu: direk + turuncu/beyaz seritli koni
        if (tZoom >= kWindsockMinZoom)
        {
            for (const AptWindsock& tSock : tDetail.mWindsocks)
            {
                const PointF tBase = tView.ToScreen(tSock.mPosition);
                if (!tOnScreen(tBase))
                {
                    continue;
                }
                const PointF tTop = {tBase.mX, tBase.mY + 12.0f};
                tLines.AddLine(tBase, tTop, 1.2f, kPole);
                // Koni direkten saga dogru daralir: uc serit (turuncu, beyaz, turuncu)
                for (int tBand = 0; tBand < 3; ++tBand)
                {
                    const float tX0 = tTop.mX + tBand * 4.0f;
                    const float tX1 = tX0 + 4.0f;
                    const float tHalf0 = 3.0f - tBand * 0.7f;
                    const float tHalf1 = tHalf0 - 0.7f;
                    tFills.AddQuad({tX0, tTop.mY - tHalf0}, {tX1, tTop.mY - tHalf1}, {tX1, tTop.mY + tHalf1},
                                   {tX0, tTop.mY + tHalf0}, tBand == 1 ? kWhite : kOrange);
                }
                mMarkers.push_back({{tBase.mX + 6.0f, tBase.mY + 8.0f}, 9.0f,
                                    tSock.mLit ? "Windsock (lit)" : "Windsock (unlit)", ""});
            }
        }

        // Kalkis / park noktalari: burun yonune donuk ok
        if (tZoom >= kStartMinZoom)
        {
            for (const AptStartLocation& tStart : tDetail.mStartLocations)
            {
                const PointF tCenter = tView.ToScreen(tStart.mPosition);
                if (!tOnScreen(tCenter))
                {
                    continue;
                }
                const std::string tTypeName = StartTypeName(tStart.mType);
                if (tStart.mType == "gate")
                {
                    AddArrow(tFills, tThin, tCenter, tStart.mHeadingDeg, 6.0f, kGate, true);
                }
                else if (tStart.mType == "tie_down" || tStart.mType == "tie-down")
                {
                    AddArrow(tFills, tThin, tCenter, tStart.mHeadingDeg, 5.0f, kTieDown, false);
                }
                else if (tStart.mType == "hangar")
                {
                    tFills.AddQuad(Rotated(tCenter, tStart.mHeadingDeg, -4.0f, -4.0f),
                                   Rotated(tCenter, tStart.mHeadingDeg, 4.0f, -4.0f),
                                   Rotated(tCenter, tStart.mHeadingDeg, 4.0f, 4.0f),
                                   Rotated(tCenter, tStart.mHeadingDeg, -4.0f, 4.0f), kHangar);
                    tThin.AddLine(tCenter, Rotated(tCenter, tStart.mHeadingDeg, 0.0f, 9.0f), 1.5f, kHangar);
                }
                else
                {
                    AddArrow(tFills, tThin, tCenter, tStart.mHeadingDeg, 5.0f, kMiscStart, true);
                }
                std::string tText = tTypeName + " " + tStart.mName;
                if (!tStart.mAircraftTypes.empty())
                {
                    tText += " (" + ReadableList(tStart.mAircraftTypes, false) + ")";
                }
                if (!tStart.mOperation.empty() && tStart.mOperation != "none")
                {
                    tText += ", " + tStart.mOperation;
                }
                if (!tStart.mAirlines.empty())
                {
                    tText += ": " + ReadableList(tStart.mAirlines, true);
                }
                mMarkers.push_back({tCenter, 6.0f, tText, tStart.mName});
            }
        }

        // PAPI / VASI / wig-wag: yaklasma yonune dik isik sirasi
        if (tZoom >= kLightMinZoom)
        {
            for (const AptLightObject& tLight : tDetail.mLightObjects)
            {
                const PointF tCenter = tView.ToScreen(tLight.mPosition);
                if (!tOnScreen(tCenter))
                {
                    continue;
                }
                if (tLight.mType == 6)
                {
                    tFills.AddDisc(Rotated(tCenter, tLight.mHeadingDeg, -2.5f, 0.0f), 2.0f, kAmber);
                    tFills.AddDisc(Rotated(tCenter, tLight.mHeadingDeg, 2.5f, 0.0f), 2.0f, kAmber);
                }
                else
                {
                    // 4 lamba: soldan ikisi beyaz, ikisi kirmizi (dogru suzulus aci gorunumu)
                    for (int tLamp = 0; tLamp < 4; ++tLamp)
                    {
                        tFills.AddDisc(Rotated(tCenter, tLight.mHeadingDeg, -6.0f + tLamp * 4.0f, 0.0f), 1.7f,
                                       tLamp < 2 ? kWhite : kRed);
                    }
                }
                const std::string tAngle =
                    tLight.mGlideslopeDeg > 0.0 ? std::format(", {:.2f} deg glide path", tLight.mGlideslopeDeg) : "";
                mMarkers.push_back({tCenter, 8.0f, std::string(LightObjectName(tLight.mType)) + tAngle, ""});
            }
        }

        // Tabelalar: rengine gore kucuk dikdortgen; metin fareyle
        if (tZoom >= kSignMinZoom)
        {
            for (const AptSign& tSign : tDetail.mSigns)
            {
                const PointF tCenter = tView.ToScreen(tSign.mPosition);
                if (!tOnScreen(tCenter))
                {
                    continue;
                }
                const float tWidth = tSign.mSize >= 3 ? 5.0f : 3.5f;
                tFills.AddQuad(Rotated(tCenter, tSign.mHeadingDeg, -tWidth, -1.5f),
                               Rotated(tCenter, tSign.mHeadingDeg, tWidth, -1.5f),
                               Rotated(tCenter, tSign.mHeadingDeg, tWidth, 1.5f),
                               Rotated(tCenter, tSign.mHeadingDeg, -tWidth, 1.5f), SignColor(tSign.mText));
                tThin.AddPolyline({Rotated(tCenter, tSign.mHeadingDeg, -tWidth, -1.5f),
                                   Rotated(tCenter, tSign.mHeadingDeg, tWidth, -1.5f),
                                   Rotated(tCenter, tSign.mHeadingDeg, tWidth, 1.5f),
                                   Rotated(tCenter, tSign.mHeadingDeg, -tWidth, 1.5f)},
                                  true, 1.0f, kYellow);
                mMarkers.push_back({tCenter, 5.0f, "Sign: " + SignText(tSign.mText), ""});
            }
        }
    }

    Gfx::Canvas& tCanvas = *pContext.mCanvas;
    tCanvas.Draw(tLamps);
    tCanvas.Draw(tFills);
    tCanvas.Draw(tLines);
    tCanvas.Draw(tThin);
}

void AirportObjectsLayer::DrawLabels(const DrawContext& pContext)
{
    if (!pContext.mLabels || pContext.mView->Zoom() < kStartLabelMinZoom)
    {
        return;
    }
    for (const Marker& tMarker : mMarkers)
    {
        if (!tMarker.mLabel.empty())
        {
            MapSymbols::DrawLabelNear(*pContext.mLabels, tMarker.mPoint.mX, tMarker.mPoint.mY, tMarker.mRadius + 3.0f,
                                      tMarker.mLabel, kLabel);
        }
    }
}

std::string AirportObjectsLayer::Describe(const DrawContext& /*pContext*/, double pX, double pY) const
{
    // En yakin isabet (uste binen semboller arasinda imlece en yakini)
    const Marker* tBest = nullptr;
    double tBestDistance = 0.0;
    for (const Marker& tMarker : mMarkers)
    {
        const double tDistance = std::hypot(tMarker.mPoint.mX - pX, tMarker.mPoint.mY - pY);
        if (tDistance <= tMarker.mRadius + kHitSlop && (!tBest || tDistance < tBestDistance))
        {
            tBest = &tMarker;
            tBestDistance = tDistance;
        }
    }
    return tBest ? tBest->mText : "";
}
