// DrawingLayer.cpp
#include "layers/DrawingLayer.h"

#include <cmath>
#include <format>

#include "Drawings.h"
#include "Geo.h"
#include "LabelPlacer.h"
#include "MapView.h"
#include "gfx/Canvas.h"
#include "ui/UiDraw.h"

namespace
{

const Rgba kPointColor = {0.0f, 0.72f, 0.95f, 1.0f};
const Rgba kRouteColor = {0.0f, 0.72f, 0.95f, 1.0f};
const Rgba kRegionColor = {0.10f, 0.78f, 0.38f, 1.0f};
const Rgba kRegionFill = {0.10f, 0.78f, 0.38f, 0.25f};

// Alt katmanlar (yapicidaki AddSubLayer sirasi). Yarim kalan cizim her zaman gorunur.
enum DrawingPart
{
    kPartPoints,
    kPartRoutes,
    kPartRegions,
    kPartLabels,
};

int PartOf(Drawings::ShapeKind pKind)
{
    switch (pKind)
    {
        case Drawings::ShapeKind::Point:
            return kPartPoints;
        case Drawings::ShapeKind::Route:
            return kPartRoutes;
        case Drawings::ShapeKind::Region:
            return kPartRegions;
    }
    return kPartPoints;
}
const Rgba kCurrentColor = {1.0f, 0.55f, 0.08f, 1.0f};
const Rgba kCurrentFill = {1.0f, 0.55f, 0.08f, 0.16f};
const Rgba kWhite = {1.0f, 1.0f, 1.0f, 1.0f};
const Rgba kMarkerCenter = {0.05f, 0.12f, 0.20f, 1.0f};
const Rgba kLabelColor = {0.95f, 0.97f, 1.0f, 1.0f};

const double kDensifyNm = 5.0;     // uzun parcalar bu araliklarla bolunur (egri dogru cizilsin)
const float kDashLength = 8.0f;    // lastik cizginin kesik boyu (boxel)
const float kLineWidth = 2.5f;     // rota / bolge kenari (boxel)

}  // namespace

DrawingLayer::DrawingLayer(const Drawings& pDrawings)
    : Layer("drawings", "User drawings", true)
    , mDrawings(pDrawings)
{
    AddSubLayer("points", "Points");
    AddSubLayer("routes", "Routes");
    AddSubLayer("regions", "Regions");
    AddSubLayer("labels", "Names and distances");
}

Gfx::Point DrawingLayer::Project(const MapView& pView, const LatLon& pPoint)
{
    return pView.ToScreen(pPoint);
}

std::vector<Gfx::Point> DrawingLayer::ProjectPath(const MapView& pView, const std::vector<LatLon>& pPoints,
                                                  bool pClosed)
{
    std::vector<LatLon> tDense;
    for (size_t tIndex = 0; tIndex + 1 < pPoints.size(); ++tIndex)
    {
        DensifySegment(pPoints[tIndex], pPoints[tIndex + 1], kDensifyNm, tDense);
    }
    if (pClosed && pPoints.size() >= 3)
    {
        DensifySegment(pPoints.back(), pPoints.front(), kDensifyNm, tDense);
    }
    else if (!pPoints.empty())
    {
        tDense.push_back(pPoints.back());
    }
    std::vector<Gfx::Point> tResult;
    tResult.reserve(tDense.size());
    for (const LatLon& tPoint : tDense)
    {
        tResult.push_back(Project(pView, tPoint));
    }
    return tResult;
}

void DrawingLayer::AddLabel(const DrawContext& pContext, const std::string& pText, const Gfx::Point& pAnchor)
{
    if (!IsSubLayerVisible(kPartLabels))
    {
        return;
    }
    // Yazi isaretin sag ustune; yeri hemen ayrilir ki baska katmanlarin
    // etiketleri bunun ustune binmesin
    PendingLabel tLabel;
    tLabel.mText = pText;
    tLabel.mX = static_cast<int>(pAnchor.mX) + 10;
    tLabel.mY = static_cast<int>(pAnchor.mY) + 6;
    if (pContext.mLabels)
    {
        RectD tBox;
        tBox.mLeft = tLabel.mX - 1;
        tBox.mRight = tLabel.mX + Ui::TextWidth(pText) + 2;
        tBox.mBottom = tLabel.mY - 3;
        tBox.mTop = tLabel.mY + Ui::FontHeight() + 1;
        pContext.mLabels->Reserve(tBox);
    }
    mLabels.push_back(tLabel);
}

void DrawingLayer::AddDashedLine(const Gfx::Point& pFrom, const Gfx::Point& pTo, Gfx::OutlinedLines& pLines)
{
    const float tDeltaX = pTo.mX - pFrom.mX;
    const float tDeltaY = pTo.mY - pFrom.mY;
    const float tLength = std::sqrt(tDeltaX * tDeltaX + tDeltaY * tDeltaY);
    if (tLength < 1.0f)
    {
        return;
    }
    const float tStepX = tDeltaX / tLength;
    const float tStepY = tDeltaY / tLength;
    for (float tAt = 0.0f; tAt < tLength; tAt += 2.0f * kDashLength)
    {
        const float tEnd = std::fmin(tAt + kDashLength, tLength);
        pLines.AddLine({pFrom.mX + tStepX * tAt, pFrom.mY + tStepY * tAt},
                       {pFrom.mX + tStepX * tEnd, pFrom.mY + tStepY * tEnd}, kLineWidth, kCurrentColor);
    }
}

void DrawingLayer::Draw(const DrawContext& pContext)
{
    mLabels.clear();
    const MapView& tView = *pContext.mView;

    Gfx::Mesh tFills;
    Gfx::OutlinedLines tLines;
    Gfx::Mesh tMarkers;

    for (const Drawings::Shape& tShape : mDrawings.Shapes())
    {
        if (!IsSubLayerVisible(PartOf(tShape.mKind)))
        {
            continue;
        }
        switch (tShape.mKind)
        {
            case Drawings::ShapeKind::Point:
            {
                const Gfx::Point tCenter = Project(tView, tShape.mPoints[0]);
                tMarkers.AddDisc(tCenter, 8.5f, Gfx::kOutline);
                tMarkers.AddDisc(tCenter, 7.0f, kWhite);
                tMarkers.AddDisc(tCenter, 5.0f, kPointColor);
                tMarkers.AddDisc(tCenter, 1.8f, kMarkerCenter);
                AddLabel(pContext, Drawings::LabelOf(tShape), tCenter);
                break;
            }
            case Drawings::ShapeKind::Route:
            {
                tLines.AddPolyline(ProjectPath(tView, tShape.mPoints, false), false, kLineWidth, kRouteColor);
                double tLengthNm = 0.0;
                for (size_t tIndex = 0; tIndex + 1 < tShape.mPoints.size(); ++tIndex)
                {
                    tLengthNm += DistanceNm(tShape.mPoints[tIndex], tShape.mPoints[tIndex + 1]);
                }
                for (const LatLon& tVertex : tShape.mPoints)
                {
                    tMarkers.AddDisc(Project(tView, tVertex), 3.5f, kRouteColor);
                }
                AddLabel(pContext, std::format("{}  {:.1f} NM", Drawings::LabelOf(tShape), tLengthNm),
                         Project(tView, tShape.mPoints.back()));
                break;
            }
            case Drawings::ShapeKind::Region:
            {
                const std::vector<Gfx::Point> tCorners = ProjectPath(tView, tShape.mPoints, true);
                if (!tShape.mTriangles.empty())
                {
                    // Ucgenler kose indeksleriyle verilir: koseleri yogunlastirmadan cevir
                    std::vector<Gfx::Point> tVertices;
                    for (const LatLon& tVertex : tShape.mPoints)
                    {
                        tVertices.push_back(Project(tView, tVertex));
                    }
                    tFills.AddIndexed(tVertices, tShape.mTriangles, kRegionFill);
                }
                tLines.AddPolyline(tCorners, true, kLineWidth, kRegionColor);
                Gfx::Point tCentroid = {0.0f, 0.0f};
                const float tCount = static_cast<float>(tShape.mPoints.size());
                for (const LatLon& tVertex : tShape.mPoints)
                {
                    const Gfx::Point tPoint = Project(tView, tVertex);
                    tMarkers.AddDisc(tPoint, 3.5f, kRegionColor);
                    tCentroid.mX += tPoint.mX / tCount;
                    tCentroid.mY += tPoint.mY / tCount;
                }
                AddLabel(pContext,
                         std::format("{}  {:.1f} NM2", Drawings::LabelOf(tShape), PolygonAreaNm2(tShape.mPoints)),
                         tCentroid);
                break;
            }
        }
    }

    // Yarim kalan cizim
    if (const Drawings::Shape* tCurrent = mDrawings.Current())
    {
        const bool tRegion = tCurrent->mKind == Drawings::ShapeKind::Region;
        if (tRegion && tCurrent->mPoints.size() >= 3)
        {
            std::vector<int> tTriangles;
            if (TriangulatePolygon(tCurrent->mPoints, tTriangles))
            {
                std::vector<Gfx::Point> tVertices;
                for (const LatLon& tVertex : tCurrent->mPoints)
                {
                    tVertices.push_back(Project(tView, tVertex));
                }
                tFills.AddIndexed(tVertices, tTriangles, kCurrentFill);
            }
        }
        tLines.AddPolyline(ProjectPath(tView, tCurrent->mPoints, false), false, kLineWidth, kCurrentColor);
        for (const LatLon& tVertex : tCurrent->mPoints)
        {
            tMarkers.AddDisc(Project(tView, tVertex), 4.0f, kCurrentColor);
        }
        if (pContext.mHasCursor && !tCurrent->mPoints.empty())
        {
            // Son koseden imlece kesikli cizgi: nereye tiklarsan oraya baglanacak
            const Gfx::Point tCursor = {static_cast<float>(pContext.mCursorX), static_cast<float>(pContext.mCursorY)};
            AddDashedLine(Project(tView, tCurrent->mPoints.back()), tCursor, tLines);
            if (tRegion && tCurrent->mPoints.size() >= 2)
            {
                AddDashedLine(tCursor, Project(tView, tCurrent->mPoints.front()), tLines);  // bolge kapanacak
            }
        }
        if (tCurrent->mKind == Drawings::ShapeKind::Route && tCurrent->mPoints.size() >= 2)
        {
            double tLengthNm = 0.0;
            for (size_t tIndex = 0; tIndex + 1 < tCurrent->mPoints.size(); ++tIndex)
            {
                tLengthNm += DistanceNm(tCurrent->mPoints[tIndex], tCurrent->mPoints[tIndex + 1]);
            }
            AddLabel(pContext, std::format("{:.1f} NM", tLengthNm), Project(tView, tCurrent->mPoints.back()));
        }
    }

    Gfx::Canvas& tCanvas = *pContext.mCanvas;
    tCanvas.Draw(tFills);
    tCanvas.Draw(tLines);
    tCanvas.Draw(tMarkers);
}

void DrawingLayer::DrawLabels(const DrawContext& /*pContext*/)
{
    for (const PendingLabel& tLabel : mLabels)
    {
        Ui::TextShadowed(tLabel.mX, tLabel.mY, tLabel.mText, kLabelColor);
    }
}
