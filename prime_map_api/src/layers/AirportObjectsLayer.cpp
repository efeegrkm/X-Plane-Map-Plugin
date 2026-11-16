#include "layers/AirportObjectsLayer.h"

#include <cctype>
#include <cmath>
#include <format>

#include "LabelPlacer.h"
#include "XMapView.h"
#include "data/AirportDetailStore.h"
#include "data/AirportShapes.h"
#include "data/AirportGeometryStore.h"
#include "gfx/Canvas.h"
#include "layers/AirportVisibility.h"
#include "layers/XMapSymbols.h"

namespace
{

const double kPi = 3.14159265358979323846;

const double kHelipadMinZoom = 12.5;
const double kBeaconMinZoom = 12.5;
const double kWindsockMinZoom = 13.5;
const double kStartMinZoom = 14.0;    // 15'ten 2 tik erken: kapilar uzaktan gorunsun
const double kStartLabelMinZoom = 15.5;
const double kLightMinZoom = 14.5;    // PAPI / VASI: 1 tik erken
const double kSignMinZoom = 15.5;
const double kLampMinZoom = 14.5;     // cizgi boyunca isiklar (101-108): 1 tik erken
const float kHitSlop = 3.0f;  // fare isabeti icin semboller biraz buyuk sayilir

enum ObjectPart
{
    kPartLighting,
    kPartHelipads,
    kPartBeacons,
    kPartWindsocks,
    kPartGates,
    kPartTieDowns,
    kPartHangars,
    kPartOtherStarts,
    kPartStartNames,
    kPartApproachLights,
    kPartSigns,
};

int startPartOf(const std::string& pType)
{
    if (pType == "gate")
    {
        return kPartGates;
    }
    if (pType == "tie_down" || pType == "tie-down")
    {
        return kPartTieDowns;
    }
    if (pType == "hangar")
    {
        return kPartHangars;
    }
    return kPartOtherStarts;
}

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

// Sembol koordinati: (pRight, pForward) birimi boxel; ileri = pHeadingDeg yonu.
// Mercator acilari korudugu icin gercek yon dogrudan kullanilir.
PointF rotated(const PointF& pCenter, double pHeadingDeg, float pRight, float pForward)
{
    const float tSin = static_cast<float>(std::sin(pHeadingDeg * kPi / 180.0));
    const float tCos = static_cast<float>(std::cos(pHeadingDeg * kPi / 180.0));
    return {pCenter.mX + pRight * tCos + pForward * tSin, pCenter.mY - pRight * tSin + pForward * tCos};
}

// Isik turu (101-108) -> renk. Iki renkli dizilerde (105, 108) lambalar sirayla degisir.
Rgba lampColor(int pType, size_t pIndex)
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

void addArrow(Gfx::Mesh& pFill, Gfx::Mesh& pLines, const PointF& pCenter, double pHeadingDeg, float pSize,
              const Rgba& pColor, bool pFilled)
{
    const PointF tTip = rotated(pCenter, pHeadingDeg, 0.0f, pSize);
    const PointF tLeft = rotated(pCenter, pHeadingDeg, -pSize * 0.7f, -pSize * 0.7f);
    const PointF tNotch = rotated(pCenter, pHeadingDeg, 0.0f, -pSize * 0.3f);
    const PointF tRight = rotated(pCenter, pHeadingDeg, pSize * 0.7f, -pSize * 0.7f);
    if (pFilled)
    {
        pFill.addTriangle(tTip, tLeft, tNotch, pColor);
        pFill.addTriangle(tTip, tNotch, tRight, pColor);
    }
    pLines.addPolyline({tTip, tLeft, tNotch, tRight}, true, 1.2f, pFilled ? Gfx::kOutline : pColor);
}

std::string startTypeName(const std::string& pType)
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

const Rgba& beaconColor(int pType)
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

const char* beaconName(int pType)
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

const char* lightObjectName(int pType)
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
std::string readableList(const std::string& pList, bool pUpper)
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
std::string signText(const std::string& pText)
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

const Rgba& signColor(const std::string& pText)
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

}

AirportObjectsLayer::AirportObjectsLayer(const AirportGeometryStore& pSummaries, AirportDetailStore& pDetails)
    : Layer("airport_objects", "Airport objects", true)
    , mSummaries(pSummaries)
    , mDetails(pDetails)
{
    addSubLayer("lighting", "Edge / taxi lights");
    addSubLayer("helipads", "Helipads");
    addSubLayer("beacons", "Beacons");
    addSubLayer("windsocks", "Windsocks");
    addSubLayer("gates", "Gates");
    addSubLayer("tie_downs", "Tie-downs");
    addSubLayer("hangars", "Hangars");
    addSubLayer("other_starts", "Other start locations");
    addSubLayer("start_names", "Parking names");
    addSubLayer("approach_lights", "PAPI / VASI");
    addSubLayer("signs", "Taxi signs");
}

void AirportObjectsLayer::draw(const drawContext& pContext)
{
    mMarkers.clear();
    const XMapView& tView = *pContext.mView;
    const double tZoom = tView.zoom();
    if (tZoom < kHelipadMinZoom)
    {
        return;
    }
    findVisibleAirports(tView, mSummaries, mQuery, mVisible, false);  // dugumsuz heliport'lar da

    Gfx::Mesh tFills;
    Gfx::OutlinedLines tLines(XMapSymbols::kOutlineExtra);
    tLines.setOutlineColor(pContext.mTheme->mHalo);
    Gfx::Mesh tThin;
    Gfx::Mesh tLamps;  // isik noktalari (en altta: semboller ustune binsin)
    const RectD& tArea = pContext.mArea;
    auto tOnScreen = [&](const PointF& pPoint)
    {
        return tArea.contains(pPoint.mX, pPoint.mY);
    };

    for (int tIndex : mVisible)
    {
        const LoadedAirport* tLoaded = mDetails.use(tIndex);
        if (!tLoaded)
        {
            continue;  // yuzey katmani "yukleniyor" kutusunu zaten ciziyor
        }
        const AirportDetail& tDetail = tLoaded->mDetail;

        // Isik dizileri: lambalar GPU'daki sekillerle ayni donusumle (kaydirma + olcek)
        if (tZoom >= kLampMinZoom && isSubLayerDrawn(kPartLighting))
        {
            const AirportShapes& tShapes = tLoaded->mShapes;
            const PointF tOffset = tView.mercatorToScreen(tShapes.mOriginX, tShapes.mOriginY);
            const float tScale = static_cast<float>(tView.worldSize());
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
                tLamps.addRect(tBox, lampColor(tLight.mType, tLamp));
            }
        }
        const double tMetersToBoxels = 1.0 / tView.metersPerBoxelAt(mSummaries.at(tIndex).mReference.mLat);

        // Helipad: gercek boyunda (en az 11 boxel) cember + yone donuk "H"
        for (const AptHelipad& tPad : tDetail.mHelipads)
        {
            const PointF tCenter = tView.toScreen(tPad.mCenter);
            if (!isSubLayerDrawn(kPartHelipads) || !tOnScreen(tCenter))
            {
                continue;
            }
            // Kucukken H cemberle birlesmesin: en az 11 boxel, H yaricapin %42'si
            const float tRadius = std::fmax(static_cast<float>(std::fmax(tPad.mLengthM, tPad.mWidthM) * 0.5 *
                                                              tMetersToBoxels),
                                            11.0f);
            tFills.addDisc(tCenter, tRadius, kHelipadFill);
            tLines.addRing(tCenter, tRadius, 1.4f, kHelipad);
            const float tH = tRadius * 0.42f;
            if (pContext.mLabels)
            {
                RectD tBox;
                tBox.mLeft = tCenter.mX - tRadius;
                tBox.mRight = tCenter.mX + tRadius;
                tBox.mBottom = tCenter.mY - tRadius;
                tBox.mTop = tCenter.mY + tRadius;
                pContext.mLabels->reserve(tBox);  // pist numarasi gibi etiketler ustune binmesin
            }
            tLines.addLine(rotated(tCenter, tPad.mHeadingDeg, -tH * 0.6f, -tH),
                           rotated(tCenter, tPad.mHeadingDeg, -tH * 0.6f, tH), 1.6f, kHelipad);
            tLines.addLine(rotated(tCenter, tPad.mHeadingDeg, tH * 0.6f, -tH),
                           rotated(tCenter, tPad.mHeadingDeg, tH * 0.6f, tH), 1.6f, kHelipad);
            tLines.addLine(rotated(tCenter, tPad.mHeadingDeg, -tH * 0.6f, 0.0f),
                           rotated(tCenter, tPad.mHeadingDeg, tH * 0.6f, 0.0f), 1.6f, kHelipad);
            mMarkers.push_back({tCenter, tRadius,
                                std::format("Helipad {}  {:.0f} x {:.0f} m, {}, heading {:.0f}", tPad.mName,
                                            tPad.mLengthM, tPad.mWidthM, surfaceName(tPad.mSurfaceCode),
                                            tPad.mHeadingDeg),
                                ""});
        }

        // Beacon: 8 isinli yildiz, ortada beacon renginde nokta
        for (const AptBeacon& tBeacon : tDetail.mBeacons)
        {
            const PointF tCenter = tView.toScreen(tBeacon.mPosition);
            if (tBeacon.mType == 0 || !isSubLayerDrawn(kPartBeacons) || !tOnScreen(tCenter))
            {
                continue;
            }
            for (int tRay = 0; tRay < 8; ++tRay)
            {
                const double tAngle = tRay * 45.0;
                tLines.addLine(rotated(tCenter, tAngle, 0.0f, 4.0f), rotated(tCenter, tAngle, 0.0f, 8.0f), 1.2f,
                               kBeaconRay);
            }
            tFills.addDisc(tCenter, 3.5f, beaconColor(tBeacon.mType));
            mMarkers.push_back({tCenter, 8.0f, std::format("Beacon: {}", beaconName(tBeacon.mType)), ""});
        }

        // Ruzgar tulumu
        if (tZoom >= kWindsockMinZoom && isSubLayerDrawn(kPartWindsocks))
        {
            for (const AptWindsock& tSock : tDetail.mWindsocks)
            {
                const PointF tBase = tView.toScreen(tSock.mPosition);
                if (!tOnScreen(tBase))
                {
                    continue;
                }
                const PointF tTop = {tBase.mX, tBase.mY + 12.0f};
                tLines.addLine(tBase, tTop, 1.2f, kPole);
                // Koni direkten saga dogru daralir: uc serit (turuncu, beyaz, turuncu)
                for (int tBand = 0; tBand < 3; ++tBand)
                {
                    const float tX0 = tTop.mX + tBand * 4.0f;
                    const float tX1 = tX0 + 4.0f;
                    const float tHalf0 = 3.0f - tBand * 0.7f;
                    const float tHalf1 = tHalf0 - 0.7f;
                    tFills.addQuad({tX0, tTop.mY - tHalf0}, {tX1, tTop.mY - tHalf1}, {tX1, tTop.mY + tHalf1},
                                   {tX0, tTop.mY + tHalf0}, tBand == 1 ? kWhite : kOrange);
                }
                mMarkers.push_back({{tBase.mX + 6.0f, tBase.mY + 8.0f}, 9.0f,
                                    tSock.mLit ? "Windsock (lit)" : "Windsock (unlit)", ""});
            }
        }

        // Kalkis / park noktalari
        if (tZoom >= kStartMinZoom)
        {
            for (const AptStartLocation& tStart : tDetail.mStartLocations)
            {
                const PointF tCenter = tView.toScreen(tStart.mPosition);
                if (!isSubLayerDrawn(startPartOf(tStart.mType)) || !tOnScreen(tCenter))
                {
                    continue;
                }
                const std::string tTypeName = startTypeName(tStart.mType);
                if (tStart.mType == "gate")
                {
                    addArrow(tFills, tThin, tCenter, tStart.mHeadingDeg, 6.0f, kGate, true);
                }
                else if (tStart.mType == "tie_down" || tStart.mType == "tie-down")
                {
                    addArrow(tFills, tThin, tCenter, tStart.mHeadingDeg, 5.0f, kTieDown, false);
                }
                else if (tStart.mType == "hangar")
                {
                    tFills.addQuad(rotated(tCenter, tStart.mHeadingDeg, -4.0f, -4.0f),
                                   rotated(tCenter, tStart.mHeadingDeg, 4.0f, -4.0f),
                                   rotated(tCenter, tStart.mHeadingDeg, 4.0f, 4.0f),
                                   rotated(tCenter, tStart.mHeadingDeg, -4.0f, 4.0f), kHangar);
                    tThin.addLine(tCenter, rotated(tCenter, tStart.mHeadingDeg, 0.0f, 9.0f), 1.5f, kHangar);
                }
                else
                {
                    addArrow(tFills, tThin, tCenter, tStart.mHeadingDeg, 5.0f, kMiscStart, true);
                }
                std::string tText = tTypeName + " " + tStart.mName;
                if (!tStart.mAircraftTypes.empty())
                {
                    tText += " (" + readableList(tStart.mAircraftTypes, false) + ")";
                }
                if (!tStart.mOperation.empty() && tStart.mOperation != "none")
                {
                    tText += ", " + tStart.mOperation;
                }
                if (!tStart.mAirlines.empty())
                {
                    tText += ": " + readableList(tStart.mAirlines, true);
                }
                mMarkers.push_back({tCenter, 6.0f, tText, isSubLayerDrawn(kPartStartNames) ? tStart.mName : ""});
            }
        }

        // PAPI / VASI / wig-wag
        if (tZoom >= kLightMinZoom && isSubLayerDrawn(kPartApproachLights))
        {
            for (const AptLightObject& tLight : tDetail.mLightObjects)
            {
                const PointF tCenter = tView.toScreen(tLight.mPosition);
                if (!tOnScreen(tCenter))
                {
                    continue;
                }
                if (tLight.mType == 6)
                {
                    tFills.addDisc(rotated(tCenter, tLight.mHeadingDeg, -2.5f, 0.0f), 2.0f, kAmber);
                    tFills.addDisc(rotated(tCenter, tLight.mHeadingDeg, 2.5f, 0.0f), 2.0f, kAmber);
                }
                else
                {
                    // 4 lamba: soldan ikisi beyaz, ikisi kirmizi (dogru suzulus aci gorunumu)
                    for (int tLamp = 0; tLamp < 4; ++tLamp)
                    {
                        tFills.addDisc(rotated(tCenter, tLight.mHeadingDeg, -6.0f + tLamp * 4.0f, 0.0f), 1.7f,
                                       tLamp < 2 ? kWhite : kRed);
                    }
                }
                const std::string tAngle =
                    tLight.mGlideslopeDeg > 0.0 ? std::format(", {:.2f} deg glide path", tLight.mGlideslopeDeg) : "";
                mMarkers.push_back({tCenter, 8.0f, std::string(lightObjectName(tLight.mType)) + tAngle, ""});
            }
        }

        if (tZoom >= kSignMinZoom && isSubLayerDrawn(kPartSigns))
        {
            for (const AptSign& tSign : tDetail.mSigns)
            {
                const PointF tCenter = tView.toScreen(tSign.mPosition);
                if (!tOnScreen(tCenter))
                {
                    continue;
                }
                const float tWidth = tSign.mSize >= 3 ? 5.0f : 3.5f;
                tFills.addQuad(rotated(tCenter, tSign.mHeadingDeg, -tWidth, -1.5f),
                               rotated(tCenter, tSign.mHeadingDeg, tWidth, -1.5f),
                               rotated(tCenter, tSign.mHeadingDeg, tWidth, 1.5f),
                               rotated(tCenter, tSign.mHeadingDeg, -tWidth, 1.5f), signColor(tSign.mText));
                tThin.addPolyline({rotated(tCenter, tSign.mHeadingDeg, -tWidth, -1.5f),
                                   rotated(tCenter, tSign.mHeadingDeg, tWidth, -1.5f),
                                   rotated(tCenter, tSign.mHeadingDeg, tWidth, 1.5f),
                                   rotated(tCenter, tSign.mHeadingDeg, -tWidth, 1.5f)},
                                  true, 1.0f, kYellow);
                mMarkers.push_back({tCenter, 5.0f, "Sign: " + signText(tSign.mText), ""});
            }
        }
    }

    Gfx::Canvas& tCanvas = *pContext.mCanvas;
    tCanvas.draw(tLamps);
    tCanvas.draw(tFills);
    tCanvas.draw(tLines);
    tCanvas.draw(tThin);
}

void AirportObjectsLayer::drawLabels(const drawContext& pContext)
{
    if (!pContext.mLabels || pContext.mView->zoom() < kStartLabelMinZoom)
    {
        return;
    }
    for (const Marker& tMarker : mMarkers)
    {
        if (!tMarker.mLabel.empty())
        {
            XMapSymbols::drawLabelNear(*pContext.mLabels, tMarker.mPoint.mX, tMarker.mPoint.mY, tMarker.mRadius + 3.0f,
                                      tMarker.mLabel, pContext.mTheme->mLight ? pContext.mTheme->mRunwayLabel : kLabel,
                                      pContext.labelShadow());
        }
    }
}

std::string AirportObjectsLayer::describe(const drawContext& /*pContext*/, double pX, double pY) const
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
