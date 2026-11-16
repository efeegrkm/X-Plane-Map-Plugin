#include "layers/NavaidLayer.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <format>

#include "Geo.h"
#include "XMapView.h"
#include "data/AirportGeometryStore.h"
#include "data/LocalizerMatch.h"
#include "gfx/Canvas.h"
#include "layers/XMapSymbols.h"

namespace
{

// Katman turlerinin tablosu (sira NavaidType ile ayni olmali)
const auto kStyles = std::to_array<NavaidLayer::Style>({
    {"vor", "VOR / DME", true, {NavKind::Vor, NavKind::Dme}, 2, 5.0, 6.5, 8.0, 20, {0.30f, 0.85f, 0.75f, 1.0f}},
    {"ndb", "NDB", true, {NavKind::Ndb, NavKind::Ndb}, 1, 7.0, 8.0, 9.0, 30, {0.80f, 0.60f, 1.00f, 1.0f}},
    {"ils", "ILS / LOC", true, {NavKind::Ils, NavKind::Localizer}, 2, 8.5, 10.0, 10.0, 40, {1.00f, 0.62f, 0.30f, 1.0f}},
    {"fix", "Fixes", false, {NavKind::Fix, NavKind::Fix}, 1, 9.0, 10.0, 99.0, 60, {0.72f, 0.80f, 0.88f, 1.0f}},
});

const size_t kMaxSymbols = 3000;   // FPS korumasi
const double kEdgeMargin = 20.0;   // boxel
const double kFeatherNm = 10.0;        // ILS tuyunun boyu
const double kFeatherHalfAngle = 3.0;  // derece (gercek localizer ~ +-2.5; okunurluk icin biraz genis)
// Tuy dolgusunun saydamligi zoom'a gore azalir: yaklastikca tuy ekrandan
// buyuk olur, alttaki her seyi kapatmasin.
const float kFeatherFillAlphaFar = 0.40f;   // zoom <= kFeatherFadeStart
const float kFeatherFillAlphaNear = 0.07f;  // zoom >= kFeatherFadeEnd
const double kFeatherFadeStart = 10.0;
const double kFeatherFadeEnd = 13.0;
const double kFeatherOutlineMaxZoom = 12.0;  // bundan yakinda koyu kontur cizilmez

const float kVorRadius = 6.0f;
const float kDmeBoxRadius = 8.5f;  // VOR/DME'nin cevresindeki kare (kosegen yaricapi)
const float kNdbRadius = 7.0f;
const float kFixRadius = 4.5f;

// VOR pusula gulu
const double kRoseMinZoom = 7.5;
const float kRoseRadiusMin = 34.0f;  // boxel; zoom arttikca buyur
const float kRoseRadiusMax = 70.0f;

// Gorunen alani pMarginNm kadar genislet (ILS tuyu antenden uzaga tasar)
GeoBounds expandBounds(const GeoBounds& pBounds, double pMarginNm)
{
    const double tLatMargin = pMarginNm / 60.0;
    const double tMaxLat = std::fmax(std::fabs(pBounds.mSouth), std::fabs(pBounds.mNorth));
    const double tCos = std::cos(std::fmin(tMaxLat, 80.0) * 3.14159265358979 / 180.0);
    GeoBounds tResult = pBounds;
    tResult.mSouth -= tLatMargin;
    tResult.mNorth += tLatMargin;
    tResult.mWest -= tLatMargin / tCos;
    tResult.mEast += tLatMargin / tCos;
    return tResult;
}

// nav.dat frekansi: VOR/DME/ILS MHz * 100 (11340 -> "113.40"), NDB kHz.
// TACAN'da SDK kanal numarasi verir: "CH85".
std::string formatFrequency(NavKind pKind, int pFrequency)
{
    if (pKind == NavKind::Ndb)
    {
        return std::to_string(pFrequency);
    }
    if (isTacanChannel(pFrequency))
    {
        return std::format("CH{}", pFrequency);
    }
    return std::format("{}.{:02d}", pFrequency / 100, pFrequency % 100);
}

}

NavaidLayer::NavaidLayer(NavaidType pType, const NavDatabase& pDatabase, const AirportGeometryStore* pRunways)
    : Layer(styleFor(pType).mId, styleFor(pType).mName, styleFor(pType).mDefaultVisible)
    , mType(pType)
    , mStyle(styleFor(pType))
    , mDatabase(pDatabase)
    , mRunways(pRunways)
{
}

int NavaidLayer::labelPriority() const
{
    return mStyle.mLabelPriority;
}

const NavaidLayer::Style& NavaidLayer::styleFor(NavaidType pType)
{
    return kStyles[static_cast<int>(pType)];
}

void NavaidLayer::collectVisible(const drawContext& pContext)
{
    mVisible.clear();
    const XMapView& tView = *pContext.mView;
    if (!mDatabase.isReady() || tView.zoom() < mStyle.mMinZoom)
    {
        return;
    }

    RectD tArea = pContext.mArea;
    tArea.mLeft -= kEdgeMargin;
    tArea.mRight += kEdgeMargin;
    tArea.mBottom -= kEdgeMargin;
    tArea.mTop += kEdgeMargin;

    GeoBounds tBounds = tView.boundsOf(tArea);
    if (mType == NavaidType::Ils)
    {
        tBounds = expandBounds(tBounds, kFeatherNm + 1.0);
    }

    mQuery.clear();
    for (int tKind = 0; tKind < mStyle.mKindCount; ++tKind)
    {
        mDatabase.query(mStyle.mKinds[tKind], tBounds, mQuery);
    }
    mSymbolPlacer.reset(tArea);

    for (int tIndex : mQuery)
    {
        const NavRecord& tRecord = mDatabase.at(tIndex);
        VisibleNavaid tNavaid;
        tNavaid.mIndex = tIndex;
        tNavaid.mPoint = tView.toScreen({tRecord.mLat, tRecord.mLon});
        const double tX = tNavaid.mPoint.mX;
        const double tY = tNavaid.mPoint.mY;

        if (mType == NavaidType::Ils)
        {
            // Tuy ekrana degiyorsa ciz (anten disarida olsa bile); seyreltme yok
            if (!computeFeather(pContext, tIndex, &tNavaid))
            {
                continue;
            }
        }
        else
        {
            if (!tArea.contains(tX, tY))
            {
                continue;
            }
            const double tRadius = (mType == NavaidType::Fix ? kFixRadius : kDmeBoxRadius) + 1.0;
            RectD tBox;
            tBox.mLeft = tX - tRadius;
            tBox.mRight = tX + tRadius;
            tBox.mBottom = tY - tRadius;
            tBox.mTop = tY + tRadius;
            if (!mSymbolPlacer.tryPlace(tBox))
            {
                continue;
            }
            if (pContext.mLabels)
            {
                pContext.mLabels->reserve(tBox);  // hicbir katmanin etiketi bu sembolu ortmesin
            }
        }
        mVisible.push_back(tNavaid);
        if (mVisible.size() >= kMaxSymbols)
        {
            return;
        }
    }
}

LatLon NavaidLayer::featherOrigin(int pIndex)
{
    const auto tCached = mOrigins.find(pIndex);
    if (tCached != mOrigins.end())
    {
        return tCached->second.mOrigin;
    }
    const NavRecord& tRecord = mDatabase.at(pIndex);
    const LatLon tAntenna = {tRecord.mLat, tRecord.mLon};
    if (!mRunways || !mRunways->isReady())
    {
        return tAntenna;  // pist verisi henuz yok: simdilik anten (onbellege yazma)
    }
    FeatherOriginCache tEntry;
    tEntry.mOnThreshold = findLocalizerThreshold(tAntenna, tRecord.mCourseTrue, *mRunways, &tEntry.mOrigin);
    if (!tEntry.mOnThreshold)
    {
        tEntry.mOrigin = tAntenna;  // LDA/SDF gibi kayik localizer: anteni kullan
    }
    mOrigins.emplace(pIndex, tEntry);
    return tEntry.mOrigin;
}

bool NavaidLayer::computeFeather(const drawContext& pContext, int pIndex, VisibleNavaid* pOut)
{
    // Ucak localizer'a course yonunde yaklasir; yani yaklasma hatti pist
    // esiginden geriye (course + 180) dogru uzanir.
    const NavRecord& tRecord = mDatabase.at(pIndex);
    const LatLon tOrigin = featherOrigin(pIndex);
    pOut->mPoint = pContext.mView->toScreen(tOrigin);
    const double tBackBearing = tRecord.mCourseTrue + 180.0;
    const double tSideNm = kFeatherNm / std::cos(kFeatherHalfAngle * 3.14159265358979 / 180.0);
    pOut->mFeather = {
        pContext.mView->toScreen(destinationPoint(tOrigin, tBackBearing - kFeatherHalfAngle, tSideNm)),
        pContext.mView->toScreen(destinationPoint(tOrigin, tBackBearing, kFeatherNm * 0.92)),  // ortada centik
        pContext.mView->toScreen(destinationPoint(tOrigin, tBackBearing + kFeatherHalfAngle, tSideNm)),
    };

    RectD tBounds;
    tBounds.mLeft = pOut->mPoint.mX;
    tBounds.mRight = pOut->mPoint.mX;
    tBounds.mBottom = pOut->mPoint.mY;
    tBounds.mTop = pOut->mPoint.mY;
    for (const PointF& tEnd : pOut->mFeather)
    {
        tBounds.mLeft = std::min<double>(tBounds.mLeft, tEnd.mX);
        tBounds.mRight = std::max<double>(tBounds.mRight, tEnd.mX);
        tBounds.mBottom = std::min<double>(tBounds.mBottom, tEnd.mY);
        tBounds.mTop = std::max<double>(tBounds.mTop, tEnd.mY);
    }
    RectD tUnused;
    return intersectRect(tBounds, pContext.mArea, &tUnused);
}

void NavaidLayer::draw(const drawContext& pContext)
{
    collectVisible(pContext);
    if (mVisible.empty())
    {
        return;
    }
    Gfx::Canvas& tCanvas = *pContext.mCanvas;
    mColor = pContext.mTheme->mNavaid[static_cast<int>(mType)];
    mHalo = pContext.mTheme->mHalo;

    // VFR / IFR: VOR'larin cevresinde manyetik kuzeye donuk pusula gulu
    if (mType == NavaidType::Vor && pContext.mTheme->mVorRose && pContext.mView->zoom() >= kRoseMinZoom)
    {
        Gfx::Mesh tRose;
        for (const VisibleNavaid& tNavaid : mVisible)
        {
            addCompassRose(tNavaid, pContext.mView->zoom(), tRose);
        }
        tCanvas.draw(tRose);
    }

    if (mType == NavaidType::Ils)
    {
        drawFeathers(tCanvas, pContext.mView->zoom());
        return;
    }

    Gfx::OutlinedLines tSymbols(XMapSymbols::kOutlineExtra);
    tSymbols.setOutlineColor(mHalo);
    Gfx::Mesh tCenters;
    for (const VisibleNavaid& tNavaid : mVisible)
    {
        addSymbol(tNavaid, tSymbols);
        if (mType != NavaidType::Fix)
        {
            // Ortadaki dolu nokta (fix haric)
            tCenters.addDisc(tNavaid.mPoint, mType == NavaidType::Ndb ? 2.5f : 1.6f, mColor);
        }
    }
    tCanvas.draw(tSymbols);
    tCanvas.draw(tCenters);
}

void NavaidLayer::addSymbol(const VisibleNavaid& pNavaid, Gfx::OutlinedLines& pSymbols) const
{
    const NavRecord& tRecord = mDatabase.at(pNavaid.mIndex);
    const float tWidth = XMapSymbols::kLineWidth;
    switch (tRecord.mKind)
    {
        case NavKind::Vor:
            pSymbols.addRegularPolygon(pNavaid.mPoint, kVorRadius, 6, 0.0f, tWidth, mColor);
            if (tRecord.mHasDme)
            {
                pSymbols.addRegularPolygon(pNavaid.mPoint, kDmeBoxRadius, 4, 45.0f, tWidth, mColor);
            }
            break;
        case NavKind::Dme:
            pSymbols.addRegularPolygon(pNavaid.mPoint, kVorRadius, 4, 45.0f, tWidth, mColor);
            break;
        case NavKind::Fix:
            pSymbols.addRegularPolygon(pNavaid.mPoint, kFixRadius, 3, 90.0f, tWidth, mColor);
            break;
        case NavKind::Ndb:
            pSymbols.addDottedCircle(pNavaid.mPoint, kNdbRadius, 12, 2.5f, mColor);  // noktali cember
            break;
        default:
            break;
    }
}

void NavaidLayer::addCompassRose(const VisibleNavaid& pNavaid, double pZoom, Gfx::Mesh& pRose) const
{
    // Cember + her 10 derecede centik (30'larda uzun), manyetik kuzeyde ok.
    // Manyetik kuzey gercek kuzeyden mVariation (+ = dogu) kadar saga doner.
    const NavRecord& tRecord = mDatabase.at(pNavaid.mIndex);
    if (tRecord.mKind != NavKind::Vor)
    {
        return;
    }
    const float tRadius =
        std::clamp(kRoseRadiusMin + static_cast<float>(pZoom - kRoseMinZoom) * 12.0f, kRoseRadiusMin, kRoseRadiusMax);
    const Rgba tColor = withAlpha(mColor, 0.75f);
    pRose.addRing(pNavaid.mPoint, tRadius, 1.0f, tColor);
    for (int tTick = 0; tTick < 36; ++tTick)
    {
        const double tBearing = (tTick * 10.0 + tRecord.mVariation) * 3.14159265358979 / 180.0;
        const float tLength = tTick % 3 == 0 ? 7.0f : 3.5f;
        const float tSin = static_cast<float>(std::sin(tBearing));
        const float tCos = static_cast<float>(std::cos(tBearing));
        const PointF tOuter = {pNavaid.mPoint.mX + tSin * tRadius, pNavaid.mPoint.mY + tCos * tRadius};
        const PointF tInner = {pNavaid.mPoint.mX + tSin * (tRadius - tLength),
                               pNavaid.mPoint.mY + tCos * (tRadius - tLength)};
        pRose.addLine(tInner, tOuter, tTick % 9 == 0 ? 1.6f : 1.0f, tColor);
        if (tTick == 0)
        {
            // Manyetik kuzey: cemberin disinda kucuk ok
            const PointF tTip = {pNavaid.mPoint.mX + tSin * (tRadius + 8.0f),
                                 pNavaid.mPoint.mY + tCos * (tRadius + 8.0f)};
            const PointF tLeft = {tOuter.mX - tCos * 3.5f, tOuter.mY + tSin * 3.5f};
            const PointF tRight = {tOuter.mX + tCos * 3.5f, tOuter.mY - tSin * 3.5f};
            pRose.addTriangle(tTip, tLeft, tRight, tColor);
        }
    }
}

void NavaidLayer::drawFeathers(Gfx::Canvas& pCanvas, double pZoom) const
{
    // Yari saydam dolgu: tuyun sag yarisi (haritalardaki klasik gorunum).
    // Saydamlik zoom'a gore azalir (kFeatherFadeStart .. kFeatherFadeEnd).
    const double tFade = std::clamp((pZoom - kFeatherFadeStart) / (kFeatherFadeEnd - kFeatherFadeStart), 0.0, 1.0);
    const float tFillAlpha =
        kFeatherFillAlphaFar + static_cast<float>(tFade) * (kFeatherFillAlphaNear - kFeatherFillAlphaFar);
    Gfx::Mesh tFill;
    const Rgba tFillColor = withAlpha(mColor, tFillAlpha);
    for (const VisibleNavaid& tNavaid : mVisible)
    {
        tFill.addTriangle(tNavaid.mPoint, tNavaid.mFeather[1], tNavaid.mFeather[2], tFillColor);
    }
    pCanvas.draw(tFill);

    // Kontur: esik -> sol kanat -> orta -> sag kanat -> esik, ve orta cizgi.
    // Uzaktan bakinca koyu kontur okunurluk saglar; yakinda gereksiz kalinlik yapar.
    Gfx::Mesh tOutline;
    Gfx::Mesh tLines;
    const bool tWithOutline = pZoom < kFeatherOutlineMaxZoom;
    for (const VisibleNavaid& tNavaid : mVisible)
    {
        const std::vector<PointF> tShape = {tNavaid.mPoint, tNavaid.mFeather[0], tNavaid.mFeather[1],
                                            tNavaid.mFeather[2]};
        if (tWithOutline)
        {
            tOutline.addPolyline(tShape, true, 3.0f, mHalo);
            tOutline.addLine(tNavaid.mPoint, tNavaid.mFeather[1], 3.0f, mHalo);
        }
        tLines.addPolyline(tShape, true, 1.2f, mColor);
        tLines.addLine(tNavaid.mPoint, tNavaid.mFeather[1], 1.2f, mColor);
    }
    pCanvas.draw(tOutline);
    pCanvas.draw(tLines);
}

void NavaidLayer::drawLabels(const drawContext& pContext)
{
    const double tZoom = pContext.mView->zoom();
    if (!pContext.mLabels || tZoom < mStyle.mLabelMinZoom)
    {
        return;
    }
    const bool tDetail = tZoom >= mStyle.mDetailMinZoom;
    mColor = pContext.mTheme->mNavaid[static_cast<int>(mType)];
    const Rgba tShadow = pContext.labelShadow();
    for (const VisibleNavaid& tNavaid : mVisible)
    {
        const std::string tText = formatLabel(mDatabase.at(tNavaid.mIndex), tDetail);
        if (mType == NavaidType::Ils)
        {
            // ILS etiketi tuyun dis ucunda
            XMapSymbols::drawLabelNear(*pContext.mLabels, tNavaid.mFeather[1].mX, tNavaid.mFeather[1].mY, 6.0f, tText,
                                      mColor, tShadow);
        }
        else
        {
            const float tGap = (mType == NavaidType::Fix ? kFixRadius : kDmeBoxRadius) + 3.0f;
            XMapSymbols::drawLabelNear(*pContext.mLabels, tNavaid.mPoint.mX, tNavaid.mPoint.mY, tGap, tText,
                                      mColor, tShadow);
        }
    }
}

std::string NavaidLayer::formatLabel(const NavRecord& pRecord, bool pDetail) const
{
    if (!pDetail || mType == NavaidType::Fix)
    {
        return pRecord.mId;
    }
    const std::string tFrequency = formatFrequency(pRecord.mKind, pRecord.mFrequency);
    if (mType == NavaidType::Ils)
    {
        // Pilotlar manyetik yonu kullanir. X-Plane 11 SDK'si sadece gercek yonu
        // verdigi icin cogu zaman "T" (true) ile gosterilir.
        if (pRecord.mCourseMagnetic >= 0)
        {
            return std::format("{} {}  {:03d}", pRecord.mId, tFrequency, pRecord.mCourseMagnetic);
        }
        return std::format("{} {}  {:03.0f}T", pRecord.mId, tFrequency, pRecord.mCourseTrue);
    }
    return std::format("{} {}", pRecord.mId, tFrequency);
}

std::string NavaidLayer::describe(const drawContext& /*pContext*/, double pX, double pY) const
{
    const float tRadius = (mType == NavaidType::Fix ? kFixRadius : kDmeBoxRadius) + 3.0f;
    for (const VisibleNavaid& tNavaid : mVisible)
    {
        if (std::hypot(tNavaid.mPoint.mX - pX, tNavaid.mPoint.mY - pY) > tRadius)
        {
            continue;
        }
        const NavRecord& tRecord = mDatabase.at(tNavaid.mIndex);
        const char* tKind = "Fix";
        switch (tRecord.mKind)
        {
            case NavKind::Vor:
                tKind = tRecord.mHasDme ? "VOR/DME" : "VOR";
                break;
            case NavKind::Dme:
                tKind = "DME";
                break;
            case NavKind::Ndb:
                tKind = "NDB";
                break;
            case NavKind::Ils:
                tKind = "ILS";
                break;
            case NavKind::Localizer:
                tKind = "LOC";
                break;
            default:
                break;
        }
        const std::string tName = (tRecord.mKind == NavKind::Fix || tRecord.mName.empty()) ? "" : "  " + tRecord.mName;
        return std::format("{}  {}{}", tKind, formatLabel(tRecord, true), tName);
    }
    return "";
}
