// NavaidLayer.cpp
#include "layers/NavaidLayer.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <format>

#include "Geo.h"
#include "MapView.h"
#include "data/AirportGeometryStore.h"
#include "data/LocalizerMatch.h"
#include "gfx/Canvas.h"
#include "layers/MapSymbols.h"

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

// Gorunen alani pMarginNm kadar genislet (ILS tuyu antenden uzaga tasar)
GeoBounds ExpandBounds(const GeoBounds& pBounds, double pMarginNm)
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
std::string FormatFrequency(NavKind pKind, int pFrequency)
{
    if (pKind == NavKind::Ndb)
    {
        return std::to_string(pFrequency);
    }
    if (IsTacanChannel(pFrequency))
    {
        return std::format("CH{}", pFrequency);
    }
    return std::format("{}.{:02d}", pFrequency / 100, pFrequency % 100);
}

}  // namespace

NavaidLayer::NavaidLayer(NavaidType pType, const NavDatabase& pDatabase, const AirportGeometryStore* pRunways)
    : Layer(StyleFor(pType).mId, StyleFor(pType).mName, StyleFor(pType).mDefaultVisible)
    , mType(pType)
    , mStyle(StyleFor(pType))
    , mDatabase(pDatabase)
    , mRunways(pRunways)
{
}

int NavaidLayer::LabelPriority() const
{
    return mStyle.mLabelPriority;
}

const NavaidLayer::Style& NavaidLayer::StyleFor(NavaidType pType)
{
    return kStyles[static_cast<int>(pType)];
}

void NavaidLayer::CollectVisible(const DrawContext& pContext)
{
    mVisible.clear();
    const MapView& tView = *pContext.mView;
    if (!mDatabase.IsReady() || tView.Zoom() < mStyle.mMinZoom)
    {
        return;
    }

    RectD tArea = pContext.mArea;
    tArea.mLeft -= kEdgeMargin;
    tArea.mRight += kEdgeMargin;
    tArea.mBottom -= kEdgeMargin;
    tArea.mTop += kEdgeMargin;

    GeoBounds tBounds = tView.BoundsOf(tArea);
    if (mType == NavaidType::Ils)
    {
        tBounds = ExpandBounds(tBounds, kFeatherNm + 1.0);
    }

    mQuery.clear();
    for (int tKind = 0; tKind < mStyle.mKindCount; ++tKind)
    {
        mDatabase.Query(mStyle.mKinds[tKind], tBounds, mQuery);
    }
    mSymbolPlacer.Reset(tArea);

    for (int tIndex : mQuery)
    {
        const NavRecord& tRecord = mDatabase.At(tIndex);
        VisibleNavaid tNavaid;
        tNavaid.mIndex = tIndex;
        tNavaid.mPoint = tView.ToScreen({tRecord.mLat, tRecord.mLon});
        const double tX = tNavaid.mPoint.mX;
        const double tY = tNavaid.mPoint.mY;

        if (mType == NavaidType::Ils)
        {
            // Tuy ekrana degiyorsa ciz (anten disarida olsa bile); seyreltme yok
            if (!ComputeFeather(pContext, tIndex, &tNavaid))
            {
                continue;
            }
        }
        else
        {
            if (!tArea.Contains(tX, tY))
            {
                continue;
            }
            const double tRadius = (mType == NavaidType::Fix ? kFixRadius : kDmeBoxRadius) + 1.0;
            RectD tBox;
            tBox.mLeft = tX - tRadius;
            tBox.mRight = tX + tRadius;
            tBox.mBottom = tY - tRadius;
            tBox.mTop = tY + tRadius;
            if (!mSymbolPlacer.TryPlace(tBox))
            {
                continue;
            }
            if (pContext.mLabels)
            {
                pContext.mLabels->Reserve(tBox);  // hicbir katmanin etiketi bu sembolu ortmesin
            }
        }
        mVisible.push_back(tNavaid);
        if (mVisible.size() >= kMaxSymbols)
        {
            return;
        }
    }
}

LatLon NavaidLayer::FeatherOrigin(int pIndex)
{
    const auto tCached = mOrigins.find(pIndex);
    if (tCached != mOrigins.end())
    {
        return tCached->second.mOrigin;
    }
    const NavRecord& tRecord = mDatabase.At(pIndex);
    const LatLon tAntenna = {tRecord.mLat, tRecord.mLon};
    if (!mRunways || !mRunways->IsReady())
    {
        return tAntenna;  // pist verisi henuz yok: simdilik anten (onbellege yazma)
    }
    FeatherOriginCache tEntry;
    tEntry.mOnThreshold = FindLocalizerThreshold(tAntenna, tRecord.mCourseTrue, *mRunways, &tEntry.mOrigin);
    if (!tEntry.mOnThreshold)
    {
        tEntry.mOrigin = tAntenna;  // LDA/SDF gibi kayik localizer: anteni kullan
    }
    mOrigins.emplace(pIndex, tEntry);
    return tEntry.mOrigin;
}

bool NavaidLayer::ComputeFeather(const DrawContext& pContext, int pIndex, VisibleNavaid* pOut)
{
    // Ucak localizer'a course yonunde yaklasir; yani yaklasma hatti pist
    // esiginden geriye (course + 180) dogru uzanir.
    const NavRecord& tRecord = mDatabase.At(pIndex);
    const LatLon tOrigin = FeatherOrigin(pIndex);
    pOut->mPoint = pContext.mView->ToScreen(tOrigin);
    const double tBackBearing = tRecord.mCourseTrue + 180.0;
    const double tSideNm = kFeatherNm / std::cos(kFeatherHalfAngle * 3.14159265358979 / 180.0);
    pOut->mFeather = {
        pContext.mView->ToScreen(DestinationPoint(tOrigin, tBackBearing - kFeatherHalfAngle, tSideNm)),
        pContext.mView->ToScreen(DestinationPoint(tOrigin, tBackBearing, kFeatherNm * 0.92)),  // ortada centik
        pContext.mView->ToScreen(DestinationPoint(tOrigin, tBackBearing + kFeatherHalfAngle, tSideNm)),
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
    return IntersectRect(tBounds, pContext.mArea, &tUnused);
}

void NavaidLayer::Draw(const DrawContext& pContext)
{
    CollectVisible(pContext);
    if (mVisible.empty())
    {
        return;
    }
    Gfx::Canvas& tCanvas = *pContext.mCanvas;

    if (mType == NavaidType::Ils)
    {
        DrawFeathers(tCanvas, pContext.mView->Zoom());
        return;
    }

    Gfx::OutlinedLines tSymbols(MapSymbols::kOutlineExtra);
    Gfx::Mesh tCenters;
    for (const VisibleNavaid& tNavaid : mVisible)
    {
        AddSymbol(tNavaid, tSymbols);
        if (mType != NavaidType::Fix)
        {
            // Ortadaki dolu nokta (fix haric)
            tCenters.AddDisc(tNavaid.mPoint, mType == NavaidType::Ndb ? 2.5f : 1.6f, mStyle.mColor);
        }
    }
    tCanvas.Draw(tSymbols);
    tCanvas.Draw(tCenters);
}

void NavaidLayer::AddSymbol(const VisibleNavaid& pNavaid, Gfx::OutlinedLines& pSymbols) const
{
    const NavRecord& tRecord = mDatabase.At(pNavaid.mIndex);
    const float tWidth = MapSymbols::kLineWidth;
    switch (tRecord.mKind)
    {
        case NavKind::Vor:
            pSymbols.AddRegularPolygon(pNavaid.mPoint, kVorRadius, 6, 0.0f, tWidth, mStyle.mColor);
            if (tRecord.mHasDme)
            {
                pSymbols.AddRegularPolygon(pNavaid.mPoint, kDmeBoxRadius, 4, 45.0f, tWidth, mStyle.mColor);
            }
            break;
        case NavKind::Dme:
            pSymbols.AddRegularPolygon(pNavaid.mPoint, kVorRadius, 4, 45.0f, tWidth, mStyle.mColor);
            break;
        case NavKind::Fix:
            pSymbols.AddRegularPolygon(pNavaid.mPoint, kFixRadius, 3, 90.0f, tWidth, mStyle.mColor);
            break;
        case NavKind::Ndb:
            pSymbols.AddDottedCircle(pNavaid.mPoint, kNdbRadius, 12, 2.5f, mStyle.mColor);  // noktali cember
            break;
        default:
            break;
    }
}

void NavaidLayer::DrawFeathers(Gfx::Canvas& pCanvas, double pZoom) const
{
    // Yari saydam dolgu: tuyun sag yarisi (haritalardaki klasik gorunum).
    // Saydamlik zoom'a gore azalir (kFeatherFadeStart .. kFeatherFadeEnd).
    const double tFade = std::clamp((pZoom - kFeatherFadeStart) / (kFeatherFadeEnd - kFeatherFadeStart), 0.0, 1.0);
    const float tFillAlpha =
        kFeatherFillAlphaFar + static_cast<float>(tFade) * (kFeatherFillAlphaNear - kFeatherFillAlphaFar);
    Gfx::Mesh tFill;
    const Rgba tFillColor = WithAlpha(mStyle.mColor, tFillAlpha);
    for (const VisibleNavaid& tNavaid : mVisible)
    {
        tFill.AddTriangle(tNavaid.mPoint, tNavaid.mFeather[1], tNavaid.mFeather[2], tFillColor);
    }
    pCanvas.Draw(tFill);

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
            tOutline.AddPolyline(tShape, true, 3.0f, Gfx::kOutline);
            tOutline.AddLine(tNavaid.mPoint, tNavaid.mFeather[1], 3.0f, Gfx::kOutline);
        }
        tLines.AddPolyline(tShape, true, 1.2f, mStyle.mColor);
        tLines.AddLine(tNavaid.mPoint, tNavaid.mFeather[1], 1.2f, mStyle.mColor);
    }
    pCanvas.Draw(tOutline);
    pCanvas.Draw(tLines);
}

void NavaidLayer::DrawLabels(const DrawContext& pContext)
{
    const double tZoom = pContext.mView->Zoom();
    if (!pContext.mLabels || tZoom < mStyle.mLabelMinZoom)
    {
        return;
    }
    const bool tDetail = tZoom >= mStyle.mDetailMinZoom;
    for (const VisibleNavaid& tNavaid : mVisible)
    {
        const std::string tText = FormatLabel(mDatabase.At(tNavaid.mIndex), tDetail);
        if (mType == NavaidType::Ils)
        {
            // ILS etiketi tuyun dis ucunda
            MapSymbols::DrawLabelNear(*pContext.mLabels, tNavaid.mFeather[1].mX, tNavaid.mFeather[1].mY, 6.0f, tText,
                                      mStyle.mColor);
        }
        else
        {
            const float tGap = (mType == NavaidType::Fix ? kFixRadius : kDmeBoxRadius) + 3.0f;
            MapSymbols::DrawLabelNear(*pContext.mLabels, tNavaid.mPoint.mX, tNavaid.mPoint.mY, tGap, tText,
                                      mStyle.mColor);
        }
    }
}

std::string NavaidLayer::FormatLabel(const NavRecord& pRecord, bool pDetail) const
{
    if (!pDetail || mType == NavaidType::Fix)
    {
        return pRecord.mId;
    }
    const std::string tFrequency = FormatFrequency(pRecord.mKind, pRecord.mFrequency);
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

std::string NavaidLayer::Describe(const DrawContext& /*pContext*/, double pX, double pY) const
{
    const float tRadius = (mType == NavaidType::Fix ? kFixRadius : kDmeBoxRadius) + 3.0f;
    for (const VisibleNavaid& tNavaid : mVisible)
    {
        if (std::hypot(tNavaid.mPoint.mX - pX, tNavaid.mPoint.mY - pY) > tRadius)
        {
            continue;
        }
        const NavRecord& tRecord = mDatabase.At(tNavaid.mIndex);
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
        return std::format("{}  {}{}", tKind, FormatLabel(tRecord, true), tName);
    }
    return "";
}
