// Mesh.cpp
#include "gfx/Mesh.h"

#include <algorithm>
#include <cmath>

namespace
{

const float kPi = 3.14159265f;
const float kThinLine = 1.5f;  // bu kalinliga kadar kose birlestirme yapilmaz

float Length(float pX, float pY)
{
    return std::sqrt(pX * pX + pY * pY);
}

// pFrom -> pTo yonune dik, pHalfWidth uzunlugunda vektor (sola dogru)
Gfx::Point Normal(const Gfx::Point& pFrom, const Gfx::Point& pTo, float pHalfWidth)
{
    const float tDeltaX = pTo.mX - pFrom.mX;
    const float tDeltaY = pTo.mY - pFrom.mY;
    const float tLength = Length(tDeltaX, tDeltaY);
    if (tLength <= 0.0f)
    {
        return {0.0f, 0.0f};
    }
    return {-tDeltaY / tLength * pHalfWidth, tDeltaX / tLength * pHalfWidth};
}

Gfx::Point Offset(const Gfx::Point& pPoint, const Gfx::Point& pDelta, float pSign)
{
    return {pPoint.mX + pDelta.mX * pSign, pPoint.mY + pDelta.mY * pSign};
}

// Merkezden pRadius uzaklikta, pAngle (radyan) acisindaki nokta
Gfx::Point OnCircle(const Gfx::Point& pCenter, float pRadius, float pAngle)
{
    return {pCenter.mX + pRadius * std::cos(pAngle), pCenter.mY + pRadius * std::sin(pAngle)};
}

// RectD'nin alan sirasi (sol, ust, sag, alt) karisik; bu yardimci sol-alt-sag-ust alir
RectD MakeRect(double pLeft, double pBottom, double pRight, double pTop)
{
    RectD tRect;
    tRect.mLeft = pLeft;
    tRect.mBottom = pBottom;
    tRect.mRight = pRight;
    tRect.mTop = pTop;
    return tRect;
}

std::vector<Gfx::Point> PolygonCorners(const Gfx::Point& pCenter, float pRadius, int pSides, float pStartDeg)
{
    std::vector<Gfx::Point> tCorners;
    tCorners.reserve(static_cast<size_t>(pSides));
    const float tStart = pStartDeg * kPi / 180.0f;
    for (int tSide = 0; tSide < pSides; ++tSide)
    {
        tCorners.push_back(OnCircle(pCenter, pRadius, tStart + 2.0f * kPi * tSide / pSides));
    }
    return tCorners;
}

}  // namespace

namespace Gfx
{

// ============================================================================
// Mesh
// ============================================================================

void Mesh::Clear()
{
    mVertices.clear();
}

bool Mesh::IsEmpty() const
{
    return mVertices.empty();
}

const std::vector<ColorVertex>& Mesh::Vertices() const
{
    return mVertices;
}

int Mesh::SegmentsFor(float pRadius)
{
    return std::clamp(static_cast<int>(pRadius * 1.5f) + 12, 12, 64);
}

void Mesh::Push(const Point& pPoint, const Rgba& pColor)
{
    mVertices.push_back({pPoint.mX, pPoint.mY, pColor});
}

void Mesh::AddTriangle(const Point& pA, const Point& pB, const Point& pC, const Rgba& pColor)
{
    Push(pA, pColor);
    Push(pB, pColor);
    Push(pC, pColor);
}

void Mesh::AddQuad(const Point& pA, const Point& pB, const Point& pC, const Point& pD, const Rgba& pColor)
{
    AddTriangle(pA, pB, pC, pColor);
    AddTriangle(pA, pC, pD, pColor);
}

void Mesh::AddRect(const RectD& pRect, const Rgba& pColor)
{
    const float tLeft = static_cast<float>(pRect.mLeft);
    const float tRight = static_cast<float>(pRect.mRight);
    const float tBottom = static_cast<float>(pRect.mBottom);
    const float tTop = static_cast<float>(pRect.mTop);
    AddQuad({tLeft, tBottom}, {tRight, tBottom}, {tRight, tTop}, {tLeft, tTop}, pColor);
}

void Mesh::AddDisc(const Point& pCenter, float pRadius, const Rgba& pColor)
{
    const int tSegments = SegmentsFor(pRadius);
    for (int tSegment = 0; tSegment < tSegments; ++tSegment)
    {
        const float tAngle0 = 2.0f * kPi * tSegment / tSegments;
        const float tAngle1 = 2.0f * kPi * (tSegment + 1) / tSegments;
        AddTriangle(pCenter, OnCircle(pCenter, pRadius, tAngle0), OnCircle(pCenter, pRadius, tAngle1), pColor);
    }
}

void Mesh::AddIndexed(const std::vector<Point>& pPoints, const std::vector<int>& pIndices, const Rgba& pColor)
{
    for (size_t tIndex = 0; tIndex + 2 < pIndices.size(); tIndex += 3)
    {
        AddTriangle(pPoints[pIndices[tIndex]], pPoints[pIndices[tIndex + 1]], pPoints[pIndices[tIndex + 2]], pColor);
    }
}

void Mesh::AddLine(const Point& pFrom, const Point& pTo, float pWidth, const Rgba& pColor)
{
    const Point tNormal = Normal(pFrom, pTo, pWidth * 0.5f);
    if (tNormal.mX == 0.0f && tNormal.mY == 0.0f)
    {
        return;  // sifir uzunluklu cizgi
    }
    AddQuad(Offset(pFrom, tNormal, 1.0f), Offset(pFrom, tNormal, -1.0f), Offset(pTo, tNormal, -1.0f),
            Offset(pTo, tNormal, 1.0f), pColor);
}

void Mesh::AddPolyline(const std::vector<Point>& pPoints, bool pClosed, float pWidth, const Rgba& pColor)
{
    const size_t tCount = pPoints.size();
    if (tCount < 2)
    {
        return;
    }
    const size_t tSegmentCount = (pClosed && tCount >= 3) ? tCount : tCount - 1;
    for (size_t tSegment = 0; tSegment < tSegmentCount; ++tSegment)
    {
        AddLine(pPoints[tSegment], pPoints[(tSegment + 1) % tCount], pWidth, pColor);
    }

    // Kose birlestirme: iki seritin arasinda kalan kamayi iki yandan da doldur.
    // Ince cizgide (<= kThinLine) kama gozle gorulmez; binlerce noktali
    // apron kenarlarinda kose sayisini ucte birine indirir.
    if (pWidth <= kThinLine)
    {
        return;
    }
    const float tHalf = pWidth * 0.5f;
    const size_t tFirstJoint = pClosed ? 0 : 1;
    const size_t tLastJoint = pClosed ? tCount : tCount - 1;
    for (size_t tJoint = tFirstJoint; tJoint < tLastJoint; ++tJoint)
    {
        const Point& tPrevious = pPoints[(tJoint + tCount - 1) % tCount];
        const Point& tCorner = pPoints[tJoint];
        const Point& tNext = pPoints[(tJoint + 1) % tCount];
        const Point tNormalIn = Normal(tPrevious, tCorner, tHalf);
        const Point tNormalOut = Normal(tCorner, tNext, tHalf);
        AddTriangle(tCorner, Offset(tCorner, tNormalIn, 1.0f), Offset(tCorner, tNormalOut, 1.0f), pColor);
        AddTriangle(tCorner, Offset(tCorner, tNormalIn, -1.0f), Offset(tCorner, tNormalOut, -1.0f), pColor);
    }
}

void Mesh::AddDashedLine(const Point& pFrom, const Point& pTo, float pWidth, float pDash, float pGap,
                         const Rgba& pColor)
{
    const float tDeltaX = pTo.mX - pFrom.mX;
    const float tDeltaY = pTo.mY - pFrom.mY;
    const float tLength = Length(tDeltaX, tDeltaY);
    if (tLength <= 0.0f || pDash <= 0.0f)
    {
        return;
    }
    const float tDirX = tDeltaX / tLength;
    const float tDirY = tDeltaY / tLength;
    for (float tAt = 0.0f; tAt < tLength; tAt += pDash + pGap)
    {
        const float tEnd = std::min(tAt + pDash, tLength);
        AddLine({pFrom.mX + tDirX * tAt, pFrom.mY + tDirY * tAt}, {pFrom.mX + tDirX * tEnd, pFrom.mY + tDirY * tEnd},
                pWidth, pColor);
    }
}

void Mesh::AddFrame(const RectD& pRect, float pWidth, const Rgba& pColor)
{
    const double tWidth = pWidth;
    AddRect(MakeRect(pRect.mLeft, pRect.mBottom, pRect.mRight, pRect.mBottom + tWidth), pColor);  // alt
    AddRect(MakeRect(pRect.mLeft, pRect.mTop - tWidth, pRect.mRight, pRect.mTop), pColor);        // ust
    AddRect(MakeRect(pRect.mLeft, pRect.mBottom + tWidth, pRect.mLeft + tWidth, pRect.mTop - tWidth), pColor);  // sol
    AddRect(MakeRect(pRect.mRight - tWidth, pRect.mBottom + tWidth, pRect.mRight, pRect.mTop - tWidth),
            pColor);  // sag
}

void Mesh::AddRing(const Point& pCenter, float pRadius, float pWidth, const Rgba& pColor)
{
    AddPolyline(PolygonCorners(pCenter, pRadius, SegmentsFor(pRadius), 0.0f), true, pWidth, pColor);
}

void Mesh::AddRegularPolygon(const Point& pCenter, float pRadius, int pSides, float pStartDeg, float pWidth,
                             const Rgba& pColor)
{
    AddPolyline(PolygonCorners(pCenter, pRadius, pSides, pStartDeg), true, pWidth, pColor);
}

void Mesh::AddDottedCircle(const Point& pCenter, float pRadius, int pCount, float pDotSize, const Rgba& pColor)
{
    const double tHalf = pDotSize * 0.5;
    for (const Point& tDot : PolygonCorners(pCenter, pRadius, pCount, 0.0f))
    {
        AddRect(MakeRect(tDot.mX - tHalf, tDot.mY - tHalf, tDot.mX + tHalf, tDot.mY + tHalf), pColor);
    }
}

// ============================================================================
// OutlinedLines
// ============================================================================

OutlinedLines::OutlinedLines(float pOutlineExtra)
    : mOutlineExtra(pOutlineExtra)
{
}

void OutlinedLines::SetOutlineColor(const Rgba& pColor)
{
    mOutlineColor = pColor;
}

void OutlinedLines::Clear()
{
    mOutline.Clear();
    mFill.Clear();
}

bool OutlinedLines::IsEmpty() const
{
    return mFill.IsEmpty();
}

void OutlinedLines::AddLine(const Point& pFrom, const Point& pTo, float pWidth, const Rgba& pColor)
{
    mOutline.AddLine(pFrom, pTo, pWidth + mOutlineExtra, mOutlineColor);
    mFill.AddLine(pFrom, pTo, pWidth, pColor);
}

void OutlinedLines::AddPolyline(const std::vector<Point>& pPoints, bool pClosed, float pWidth, const Rgba& pColor)
{
    mOutline.AddPolyline(pPoints, pClosed, pWidth + mOutlineExtra, mOutlineColor);
    mFill.AddPolyline(pPoints, pClosed, pWidth, pColor);
}

void OutlinedLines::AddRegularPolygon(const Point& pCenter, float pRadius, int pSides, float pStartDeg, float pWidth,
                                      const Rgba& pColor)
{
    mOutline.AddRegularPolygon(pCenter, pRadius, pSides, pStartDeg, pWidth + mOutlineExtra, mOutlineColor);
    mFill.AddRegularPolygon(pCenter, pRadius, pSides, pStartDeg, pWidth, pColor);
}

void OutlinedLines::AddRing(const Point& pCenter, float pRadius, float pWidth, const Rgba& pColor)
{
    mOutline.AddRing(pCenter, pRadius, pWidth + mOutlineExtra, mOutlineColor);
    mFill.AddRing(pCenter, pRadius, pWidth, pColor);
}

void OutlinedLines::AddDottedCircle(const Point& pCenter, float pRadius, int pCount, float pDotSize,
                                    const Rgba& pColor)
{
    mOutline.AddDottedCircle(pCenter, pRadius, pCount, pDotSize + mOutlineExtra, mOutlineColor);
    mFill.AddDottedCircle(pCenter, pRadius, pCount, pDotSize, pColor);
}

const Mesh& OutlinedLines::Outline() const
{
    return mOutline;
}

const Mesh& OutlinedLines::Fill() const
{
    return mFill;
}

}  // namespace Gfx
