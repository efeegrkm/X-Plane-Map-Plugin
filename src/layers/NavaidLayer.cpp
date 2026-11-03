// NavaidLayer.cpp
#include "layers/NavaidLayer.h"

#include <cmath>
#include <cstdio>

#include "GLInclude.h"
#include "Geo.h"
#include "MapView.h"
#include "layers/MapSymbols.h"

namespace
{

// Katman turlerinin tablosu (sira NavaidType ile ayni olmali)
const NavaidLayer::Style kStyles[] = {
    {"vor", "VOR / DME", true, {NavKind::Vor, NavKind::Dme}, 2, 5.0, 6.5, 8.0, {0.30f, 0.85f, 0.75f, 1.0f}},
    {"ndb", "NDB", true, {NavKind::Ndb, NavKind::Ndb}, 1, 7.0, 8.0, 9.0, {0.80f, 0.60f, 1.00f, 1.0f}},
    {"ils", "ILS / LOC", true, {NavKind::Ils, NavKind::Localizer}, 2, 8.5, 10.0, 10.0, {1.00f, 0.62f, 0.30f, 1.0f}},
    {"fix", "Fix'ler", false, {NavKind::Fix, NavKind::Fix}, 1, 9.0, 10.0, 99.0, {0.72f, 0.80f, 0.88f, 1.0f}},
};

const size_t kMaxSymbols = 3000;   // FPS korumasi
const double kEdgeMargin = 20.0;   // boxel
const double kFeatherNm = 10.0;    // ILS tuyunun boyu
const double kFeatherHalfAngle = 2.5;  // derece
const float kFeatherFillAlpha = 0.30f;

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

// nav.dat frekansi: VOR/DME/ILS MHz * 100 (11340 -> "113.40"), NDB kHz
void FormatFrequency(NavKind pKind, int pFrequency, char* pOut, size_t pSize)
{
    if (pKind == NavKind::Ndb)
    {
        std::snprintf(pOut, pSize, "%d", pFrequency);
    }
    else
    {
        std::snprintf(pOut, pSize, "%d.%02d", pFrequency / 100, pFrequency % 100);
    }
}

}  // namespace

NavaidLayer::NavaidLayer(NavaidType pType, const NavDatabase& pDatabase)
    : Layer(StyleFor(pType).mId, StyleFor(pType).mName, StyleFor(pType).mDefaultVisible)
    , mType(pType)
    , mStyle(StyleFor(pType))
    , mDatabase(pDatabase)
{
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
        double tX = 0.0;
        double tY = 0.0;
        tView.LatLonToBoxel({tRecord.mLat, tRecord.mLon}, &tX, &tY);
        tNavaid.mX = static_cast<float>(tX);
        tNavaid.mY = static_cast<float>(tY);

        if (mType == NavaidType::Ils)
        {
            // Tuy ekrana degiyorsa ciz (anten disarida olsa bile); seyreltme yok
            if (!ComputeFeather(pContext, tRecord, &tNavaid))
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
        }
        mVisible.push_back(tNavaid);
        if (mVisible.size() >= kMaxSymbols)
        {
            return;
        }
    }
}

bool NavaidLayer::ComputeFeather(const DrawContext& pContext, const NavRecord& pRecord, VisibleNavaid* pOut) const
{
    // Ucak localizer'a course yonunde yaklasir; yani yaklasma hatti antenden
    // geriye (course + 180) dogru uzanir.
    const LatLon tAntenna = {pRecord.mLat, pRecord.mLon};
    const double tBackBearing = pRecord.mCourseTrue + 180.0;
    const double tSideNm = kFeatherNm / std::cos(kFeatherHalfAngle * 3.14159265358979 / 180.0);
    const LatLon tEnds[3] = {
        DestinationPoint(tAntenna, tBackBearing - kFeatherHalfAngle, tSideNm),
        DestinationPoint(tAntenna, tBackBearing, kFeatherNm * 0.92),  // ortada hafif centik
        DestinationPoint(tAntenna, tBackBearing + kFeatherHalfAngle, tSideNm),
    };

    RectD tBounds;
    tBounds.mLeft = pOut->mX;
    tBounds.mRight = pOut->mX;
    tBounds.mBottom = pOut->mY;
    tBounds.mTop = pOut->mY;
    for (int tEnd = 0; tEnd < 3; ++tEnd)
    {
        double tX = 0.0;
        double tY = 0.0;
        pContext.mView->LatLonToBoxel(tEnds[tEnd], &tX, &tY);
        pOut->mFeather[tEnd * 2] = static_cast<float>(tX);
        pOut->mFeather[tEnd * 2 + 1] = static_cast<float>(tY);
        tBounds.mLeft = std::fmin(tBounds.mLeft, tX);
        tBounds.mRight = std::fmax(tBounds.mRight, tX);
        tBounds.mBottom = std::fmin(tBounds.mBottom, tY);
        tBounds.mTop = std::fmax(tBounds.mTop, tY);
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
    Ui::UseFlatState();

    if (mType == NavaidType::Ils)
    {
        DrawFeathers();
        return;
    }

    if (mType == NavaidType::Ndb)
    {
        // Noktali cember: once koyu, sonra renkli noktalar
        for (int tPass = 0; tPass < 2; ++tPass)
        {
            const bool tOutline = (tPass == 0);
            glPointSize(tOutline ? 4.5f : 2.5f);
            MapSymbols::SetColor(tOutline ? MapSymbols::kOutline : mStyle.mColor);
            glBegin(GL_POINTS);
            for (const VisibleNavaid& tNavaid : mVisible)
            {
                MapSymbols::EmitDottedCircle(tNavaid.mX, tNavaid.mY, kNdbRadius, 12);
            }
            glEnd();
        }
        glPointSize(1.0f);
    }
    else
    {
        for (int tPass = 0; tPass < 2; ++tPass)
        {
            const bool tOutline = (tPass == 0);
            glLineWidth(tOutline ? MapSymbols::kOutlineWidth : MapSymbols::kLineWidth);
            MapSymbols::SetColor(tOutline ? MapSymbols::kOutline : mStyle.mColor);
            glBegin(GL_LINES);
            for (const VisibleNavaid& tNavaid : mVisible)
            {
                EmitSymbolEdges(tNavaid);
            }
            glEnd();
        }
        glLineWidth(1.0f);
    }

    // Ortadaki dolu nokta (fix haric)
    if (mType != NavaidType::Fix)
    {
        MapSymbols::SetColor(mStyle.mColor);
        glBegin(GL_TRIANGLES);
        for (const VisibleNavaid& tNavaid : mVisible)
        {
            MapSymbols::EmitDisc(tNavaid.mX, tNavaid.mY, mType == NavaidType::Ndb ? 2.5f : 1.6f, 8);
        }
        glEnd();
    }
}

void NavaidLayer::EmitSymbolEdges(const VisibleNavaid& pNavaid) const
{
    const NavRecord& tRecord = mDatabase.At(pNavaid.mIndex);
    switch (tRecord.mKind)
    {
        case NavKind::Vor:
            MapSymbols::EmitPolygonEdges(pNavaid.mX, pNavaid.mY, kVorRadius, 6, 0.0f);
            if (tRecord.mHasDme)
            {
                MapSymbols::EmitPolygonEdges(pNavaid.mX, pNavaid.mY, kDmeBoxRadius, 4, 45.0f);
            }
            break;
        case NavKind::Dme:
            MapSymbols::EmitPolygonEdges(pNavaid.mX, pNavaid.mY, kVorRadius, 4, 45.0f);
            break;
        case NavKind::Fix:
            MapSymbols::EmitPolygonEdges(pNavaid.mX, pNavaid.mY, kFixRadius, 3, 90.0f);
            break;
        default:
            break;
    }
}

void NavaidLayer::DrawFeathers() const
{
    // Yari saydam dolgu: tuyun sag yarisi (haritalardaki klasik gorunum)
    MapSymbols::SetColor({mStyle.mColor.mR, mStyle.mColor.mG, mStyle.mColor.mB, kFeatherFillAlpha});
    glBegin(GL_TRIANGLES);
    for (const VisibleNavaid& tNavaid : mVisible)
    {
        glVertex2f(tNavaid.mX, tNavaid.mY);
        glVertex2f(tNavaid.mFeather[2], tNavaid.mFeather[3]);
        glVertex2f(tNavaid.mFeather[4], tNavaid.mFeather[5]);
    }
    glEnd();

    // Kontur: anten -> sol kanat -> orta -> sag kanat -> anten, ve orta cizgi
    for (int tPass = 0; tPass < 2; ++tPass)
    {
        const bool tOutline = (tPass == 0);
        glLineWidth(tOutline ? 3.0f : 1.2f);
        MapSymbols::SetColor(tOutline ? MapSymbols::kOutline : mStyle.mColor);
        glBegin(GL_LINES);
        for (const VisibleNavaid& tNavaid : mVisible)
        {
            const float* tF = tNavaid.mFeather;
            const float tPoints[5][2] = {
                {tNavaid.mX, tNavaid.mY}, {tF[0], tF[1]}, {tF[2], tF[3]}, {tF[4], tF[5]}, {tNavaid.mX, tNavaid.mY},
            };
            for (int tSegment = 0; tSegment < 4; ++tSegment)
            {
                glVertex2f(tPoints[tSegment][0], tPoints[tSegment][1]);
                glVertex2f(tPoints[tSegment + 1][0], tPoints[tSegment + 1][1]);
            }
            glVertex2f(tNavaid.mX, tNavaid.mY);
            glVertex2f(tF[2], tF[3]);
        }
        glEnd();
    }
    glLineWidth(1.0f);
}

void NavaidLayer::DrawLabels(const DrawContext& pContext)
{
    const double tZoom = pContext.mView->Zoom();
    if (!pContext.mLabels || tZoom < mStyle.mLabelMinZoom)
    {
        return;
    }
    const bool tDetail = tZoom >= mStyle.mDetailMinZoom;
    char tText[96];
    for (const VisibleNavaid& tNavaid : mVisible)
    {
        const NavRecord& tRecord = mDatabase.At(tNavaid.mIndex);
        FormatLabel(tRecord, tDetail, tText, sizeof(tText));
        if (mType == NavaidType::Ils)
        {
            // ILS etiketi tuyun dis ucunda
            MapSymbols::DrawLabelNear(*pContext.mLabels, tNavaid.mFeather[2], tNavaid.mFeather[3], 6.0f, tText,
                                      mStyle.mColor);
        }
        else
        {
            const float tGap = (mType == NavaidType::Fix ? kFixRadius : kDmeBoxRadius) + 3.0f;
            MapSymbols::DrawLabelNear(*pContext.mLabels, tNavaid.mX, tNavaid.mY, tGap, tText, mStyle.mColor);
        }
    }
}

void NavaidLayer::FormatLabel(const NavRecord& pRecord, bool pDetail, char* pOut, size_t pSize) const
{
    if (!pDetail || mType == NavaidType::Fix)
    {
        std::snprintf(pOut, pSize, "%s", pRecord.mId.c_str());
        return;
    }
    char tFrequency[16];
    FormatFrequency(pRecord.mKind, pRecord.mFrequency, tFrequency, sizeof(tFrequency));
    if (mType == NavaidType::Ils)
    {
        // Pilotlar manyetik yonu kullanir; bilinmiyorsa gercek yon "T" ile
        if (pRecord.mCourseMagnetic >= 0)
        {
            std::snprintf(pOut, pSize, "%s %s  %03d", pRecord.mId.c_str(), tFrequency, pRecord.mCourseMagnetic);
        }
        else
        {
            std::snprintf(pOut, pSize, "%s %s  %03.0fT", pRecord.mId.c_str(), tFrequency, pRecord.mCourseTrue);
        }
        return;
    }
    std::snprintf(pOut, pSize, "%s %s", pRecord.mId.c_str(), tFrequency);
}
