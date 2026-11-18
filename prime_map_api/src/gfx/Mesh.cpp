#include "gfx/Mesh.h"

#include <algorithm>
#include <cmath>

namespace
{
const float kPi = 3.14159265f;
const float kThinLine = 1.5f;

float length(float pX, float pY)
{
    return std::sqrt(pX * pX + pY * pY);
}

Gfx::Point normal(const Gfx::Point& pFrom, const Gfx::Point& pTo, float pHalfWidth)
{
    const float tDeltaX = pTo.mX - pFrom.mX;
    const float tDeltaY = pTo.mY - pFrom.mY;
    const float tLength = length(tDeltaX, tDeltaY);
    if (tLength <= 0.0f)
    {
        return {0.0f, 0.0f};
    }
    return {-tDeltaY / tLength * pHalfWidth, tDeltaX / tLength * pHalfWidth};
}

Gfx::Point offset(const Gfx::Point& pPoint, const Gfx::Point& pDelta, float pSign)
{
    return {pPoint.mX + pDelta.mX * pSign, pPoint.mY + pDelta.mY * pSign};
}

Gfx::Point onCircle(const Gfx::Point& pCenter, float pRadius, float pAngle)
{
    return {pCenter.mX + pRadius * std::cos(pAngle), pCenter.mY + pRadius * std::sin(pAngle)};
}

RectD makeRect(double pLeft, double pBottom, double pRight, double pTop)
{
    RectD tRect;
    tRect.mLeft = pLeft;
    tRect.mBottom = pBottom;
    tRect.mRight = pRight;
    tRect.mTop = pTop;
    return tRect;
}

std::vector<Gfx::Point> polygonCorners(const Gfx::Point& pCenter, float pRadius, int pSides, float pStartDeg)
{
    std::vector<Gfx::Point> tCorners;
    tCorners.reserve(static_cast<size_t>(pSides));
    const float tStart = pStartDeg * kPi / 180.0f;
    for (int tSide = 0; tSide < pSides; ++tSide)
    {
        tCorners.push_back(onCircle(pCenter, pRadius, tStart + 2.0f * kPi * tSide / pSides));
    }
    return tCorners;
}
}

namespace Gfx
{
void Mesh::clear()
{
    mVertices.clear();
}

bool Mesh::isEmpty() const
{
    return mVertices.empty();
}

const std::vector<ColorVertex>& Mesh::vertices() const
{
    return mVertices;
}

int Mesh::segmentsFor(float pRadius)
{
    return std::clamp(static_cast<int>(pRadius * 1.5f) + 12, 12, 64);
}

void Mesh::push(const Point& pPoint, const Rgba& pColor)
{
    mVertices.push_back({pPoint.mX, pPoint.mY, pColor});
}

void Mesh::addTriangle(const Point& pA, const Point& pB, const Point& pC, const Rgba& pColor)
{
    push(pA, pColor);
    push(pB, pColor);
    push(pC, pColor);
}

void Mesh::addQuad(const Point& pA, const Point& pB, const Point& pC, const Point& pD, const Rgba& pColor)
{
    addTriangle(pA, pB, pC, pColor);
    addTriangle(pA, pC, pD, pColor);
}

void Mesh::addRect(const RectD& pRect, const Rgba& pColor)
{
    const float tLeft = static_cast<float>(pRect.mLeft);
    const float tRight = static_cast<float>(pRect.mRight);
    const float tBottom = static_cast<float>(pRect.mBottom);
    const float tTop = static_cast<float>(pRect.mTop);
    addQuad({tLeft, tBottom}, {tRight, tBottom}, {tRight, tTop}, {tLeft, tTop}, pColor);
}

void Mesh::addDisc(const Point& pCenter, float pRadius, const Rgba& pColor)
{
    const int tSegments = segmentsFor(pRadius);
    for (int tSegment = 0; tSegment < tSegments; ++tSegment)
    {
        const float tAngle0 = 2.0f * kPi * tSegment / tSegments;
        const float tAngle1 = 2.0f * kPi * (tSegment + 1) / tSegments;
        addTriangle(pCenter, onCircle(pCenter, pRadius, tAngle0), onCircle(pCenter, pRadius, tAngle1), pColor);
    }
}

void Mesh::addIndexed(const std::vector<Point>& pPoints, const std::vector<int>& pIndices, const Rgba& pColor)
{
    for (size_t tIndex = 0; tIndex + 2 < pIndices.size(); tIndex += 3)
    {
        addTriangle(pPoints[pIndices[tIndex]], pPoints[pIndices[tIndex + 1]], pPoints[pIndices[tIndex + 2]], pColor);
    }
}

void Mesh::addLine(const Point& pFrom, const Point& pTo, float pWidth, const Rgba& pColor)
{
    const Point tNormal = normal(pFrom, pTo, pWidth * 0.5f);
    if (tNormal.mX == 0.0f && tNormal.mY == 0.0f)
    {
        return;
    }
    addQuad(offset(pFrom, tNormal, 1.0f), offset(pFrom, tNormal, -1.0f), offset(pTo, tNormal, -1.0f),
            offset(pTo, tNormal, 1.0f), pColor);
}

void Mesh::addPolyline(const std::vector<Point>& pPoints, bool pClosed, float pWidth, const Rgba& pColor)
{
    const size_t tCount = pPoints.size();
    if (tCount < 2)
    {
        return;
    }
    const size_t tSegmentCount = (pClosed && tCount >= 3) ? tCount : tCount - 1;
    for (size_t tSegment = 0; tSegment < tSegmentCount; ++tSegment)
    {
        addLine(pPoints[tSegment], pPoints[(tSegment + 1) % tCount], pWidth, pColor);
    }

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
        const Point tNormalIn = normal(tPrevious, tCorner, tHalf);
        const Point tNormalOut = normal(tCorner, tNext, tHalf);
        addTriangle(tCorner, offset(tCorner, tNormalIn, 1.0f), offset(tCorner, tNormalOut, 1.0f), pColor);
        addTriangle(tCorner, offset(tCorner, tNormalIn, -1.0f), offset(tCorner, tNormalOut, -1.0f), pColor);
    }
}

void Mesh::addDashedLine(const Point& pFrom, const Point& pTo, float pWidth, float pDash, float pGap,
                         const Rgba& pColor)
{
    const float tDeltaX = pTo.mX - pFrom.mX;
    const float tDeltaY = pTo.mY - pFrom.mY;
    const float tLength = length(tDeltaX, tDeltaY);
    if (tLength <= 0.0f || pDash <= 0.0f)
    {
        return;
    }
    const float tDirX = tDeltaX / tLength;
    const float tDirY = tDeltaY / tLength;
    for (float tAt = 0.0f; tAt < tLength; tAt += pDash + pGap)
    {
        const float tEnd = std::min(tAt + pDash, tLength);
        addLine({pFrom.mX + tDirX * tAt, pFrom.mY + tDirY * tAt}, {pFrom.mX + tDirX * tEnd, pFrom.mY + tDirY * tEnd},
                pWidth, pColor);
    }
}

void Mesh::addFrame(const RectD& pRect, float pWidth, const Rgba& pColor)
{
    const double tWidth = pWidth;
    addRect(makeRect(pRect.mLeft, pRect.mBottom, pRect.mRight, pRect.mBottom + tWidth), pColor);
    addRect(makeRect(pRect.mLeft, pRect.mTop - tWidth, pRect.mRight, pRect.mTop), pColor);
    addRect(makeRect(pRect.mLeft, pRect.mBottom + tWidth, pRect.mLeft + tWidth, pRect.mTop - tWidth), pColor);
    addRect(makeRect(pRect.mRight - tWidth, pRect.mBottom + tWidth, pRect.mRight, pRect.mTop - tWidth),
            pColor);
}

void Mesh::addRing(const Point& pCenter, float pRadius, float pWidth, const Rgba& pColor)
{
    addPolyline(polygonCorners(pCenter, pRadius, segmentsFor(pRadius), 0.0f), true, pWidth, pColor);
}

void Mesh::addRegularPolygon(const Point& pCenter, float pRadius, int pSides, float pStartDeg, float pWidth,
                             const Rgba& pColor)
{
    addPolyline(polygonCorners(pCenter, pRadius, pSides, pStartDeg), true, pWidth, pColor);
}

void Mesh::addDottedCircle(const Point& pCenter, float pRadius, int pCount, float pDotSize, const Rgba& pColor)
{
    const double tHalf = pDotSize * 0.5;
    for (const Point& tDot : polygonCorners(pCenter, pRadius, pCount, 0.0f))
    {
        addRect(makeRect(tDot.mX - tHalf, tDot.mY - tHalf, tDot.mX + tHalf, tDot.mY + tHalf), pColor);
    }
}

OutlinedLines::OutlinedLines(float pOutlineExtra)
    : mOutlineExtra(pOutlineExtra)
{
}

void OutlinedLines::setOutlineColor(const Rgba& pColor)
{
    mOutlineColor = pColor;
}

void OutlinedLines::clear()
{
    mOutline.clear();
    mFill.clear();
}

bool OutlinedLines::isEmpty() const
{
    return mFill.isEmpty();
}

void OutlinedLines::addLine(const Point& pFrom, const Point& pTo, float pWidth, const Rgba& pColor)
{
    mOutline.addLine(pFrom, pTo, pWidth + mOutlineExtra, mOutlineColor);
    mFill.addLine(pFrom, pTo, pWidth, pColor);
}

void OutlinedLines::addPolyline(const std::vector<Point>& pPoints, bool pClosed, float pWidth, const Rgba& pColor)
{
    mOutline.addPolyline(pPoints, pClosed, pWidth + mOutlineExtra, mOutlineColor);
    mFill.addPolyline(pPoints, pClosed, pWidth, pColor);
}

void OutlinedLines::addRegularPolygon(const Point& pCenter, float pRadius, int pSides, float pStartDeg, float pWidth,
                                      const Rgba& pColor)
{
    mOutline.addRegularPolygon(pCenter, pRadius, pSides, pStartDeg, pWidth + mOutlineExtra, mOutlineColor);
    mFill.addRegularPolygon(pCenter, pRadius, pSides, pStartDeg, pWidth, pColor);
}

void OutlinedLines::addRing(const Point& pCenter, float pRadius, float pWidth, const Rgba& pColor)
{
    mOutline.addRing(pCenter, pRadius, pWidth + mOutlineExtra, mOutlineColor);
    mFill.addRing(pCenter, pRadius, pWidth, pColor);
}

void OutlinedLines::addDottedCircle(const Point& pCenter, float pRadius, int pCount, float pDotSize,
                                    const Rgba& pColor)
{
    mOutline.addDottedCircle(pCenter, pRadius, pCount, pDotSize + mOutlineExtra, mOutlineColor);
    mFill.addDottedCircle(pCenter, pRadius, pCount, pDotSize, pColor);
}

const Mesh& OutlinedLines::outline() const
{
    return mOutline;
}

const Mesh& OutlinedLines::fill() const
{
    return mFill;
}
}
