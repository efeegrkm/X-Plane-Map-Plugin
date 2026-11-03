// Batch.cpp
// Projede glBegin/glEnd ile kose noktasi gonderen tek yer burasi
// (eski katmanlar henuz MapSymbols uzerinden dogrudan ciziyor).
#include "gfx/Batch.h"

#include <cmath>

#include "GLInclude.h"

namespace
{

const float kPi = 3.14159265f;

void SetColor(const Rgba& pColor)
{
    glColor4f(pColor.mR, pColor.mG, pColor.mB, pColor.mA);
}

// Daire icin parca sayisi: buyuk dairelerde daha cok parca (kenar puruzsuz kalsin)
int SegmentsFor(float pRadius)
{
    const int tSegments = static_cast<int>(pRadius * 1.5f) + 12;
    return tSegments > 64 ? 64 : tSegments;
}

}  // namespace

namespace Gfx
{

// ============================================================================
// LineBatch
// ============================================================================

void LineBatch::Clear()
{
    mVertices.clear();
}

bool LineBatch::IsEmpty() const
{
    return mVertices.empty();
}

void LineBatch::AddLine(const Point& pFrom, const Point& pTo, const Rgba& pColor)
{
    mVertices.push_back({pFrom, pColor});
    mVertices.push_back({pTo, pColor});
}

void LineBatch::AddPolyline(const std::vector<Point>& pPoints, bool pClosed, const Rgba& pColor)
{
    for (size_t tIndex = 0; tIndex + 1 < pPoints.size(); ++tIndex)
    {
        AddLine(pPoints[tIndex], pPoints[tIndex + 1], pColor);
    }
    if (pClosed && pPoints.size() >= 3)
    {
        AddLine(pPoints.back(), pPoints.front(), pColor);
    }
}

void LineBatch::AddRegularPolygon(const Point& pCenter, float pRadius, int pSides, float pStartDeg,
                                  const Rgba& pColor)
{
    const float tStart = pStartDeg * kPi / 180.0f;
    for (int tSide = 0; tSide < pSides; ++tSide)
    {
        const float tAngle0 = tStart + 2.0f * kPi * tSide / pSides;
        const float tAngle1 = tStart + 2.0f * kPi * (tSide + 1) / pSides;
        AddLine({pCenter.mX + pRadius * std::cos(tAngle0), pCenter.mY + pRadius * std::sin(tAngle0)},
                {pCenter.mX + pRadius * std::cos(tAngle1), pCenter.mY + pRadius * std::sin(tAngle1)}, pColor);
    }
}

void LineBatch::AddCircle(const Point& pCenter, float pRadius, const Rgba& pColor)
{
    AddRegularPolygon(pCenter, pRadius, SegmentsFor(pRadius), 0.0f, pColor);
}

void LineBatch::Draw(float pWidth) const
{
    Emit(pWidth, nullptr);
}

void LineBatch::DrawOutlined(float pWidth) const
{
    Emit(pWidth + 2.0f, &kOutline);
    Emit(pWidth, nullptr);
}

void LineBatch::Emit(float pWidth, const Rgba* pOverride) const
{
    if (mVertices.empty())
    {
        return;
    }
    Ui::UseFlatState();
    glLineWidth(pWidth);
    if (pOverride)
    {
        SetColor(*pOverride);
    }
    glBegin(GL_LINES);
    for (const Vertex& tVertex : mVertices)
    {
        if (!pOverride)
        {
            SetColor(tVertex.mColor);
        }
        glVertex2f(tVertex.mPoint.mX, tVertex.mPoint.mY);
    }
    glEnd();
    glLineWidth(1.0f);
}

// ============================================================================
// TriangleBatch
// ============================================================================

void TriangleBatch::Clear()
{
    mVertices.clear();
}

bool TriangleBatch::IsEmpty() const
{
    return mVertices.empty();
}

void TriangleBatch::AddTriangle(const Point& pA, const Point& pB, const Point& pC, const Rgba& pColor)
{
    mVertices.push_back({pA, pColor});
    mVertices.push_back({pB, pColor});
    mVertices.push_back({pC, pColor});
}

void TriangleBatch::AddQuad(const Point& pA, const Point& pB, const Point& pC, const Point& pD, const Rgba& pColor)
{
    AddTriangle(pA, pB, pC, pColor);
    AddTriangle(pA, pC, pD, pColor);
}

void TriangleBatch::AddDisc(const Point& pCenter, float pRadius, const Rgba& pColor)
{
    const int tSegments = SegmentsFor(pRadius);
    for (int tSegment = 0; tSegment < tSegments; ++tSegment)
    {
        const float tAngle0 = 2.0f * kPi * tSegment / tSegments;
        const float tAngle1 = 2.0f * kPi * (tSegment + 1) / tSegments;
        AddTriangle(pCenter, {pCenter.mX + pRadius * std::cos(tAngle0), pCenter.mY + pRadius * std::sin(tAngle0)},
                    {pCenter.mX + pRadius * std::cos(tAngle1), pCenter.mY + pRadius * std::sin(tAngle1)}, pColor);
    }
}

void TriangleBatch::AddIndexed(const std::vector<Point>& pPoints, const std::vector<int>& pIndices,
                               const Rgba& pColor)
{
    for (size_t tIndex = 0; tIndex + 2 < pIndices.size(); tIndex += 3)
    {
        AddTriangle(pPoints[pIndices[tIndex]], pPoints[pIndices[tIndex + 1]], pPoints[pIndices[tIndex + 2]], pColor);
    }
}

void TriangleBatch::Draw() const
{
    if (mVertices.empty())
    {
        return;
    }
    Ui::UseFlatState();
    glBegin(GL_TRIANGLES);
    for (const Vertex& tVertex : mVertices)
    {
        SetColor(tVertex.mColor);
        glVertex2f(tVertex.mPoint.mX, tVertex.mPoint.mY);
    }
    glEnd();
}

// ============================================================================
// PointBatch
// ============================================================================

void PointBatch::Clear()
{
    mVertices.clear();
}

bool PointBatch::IsEmpty() const
{
    return mVertices.empty();
}

void PointBatch::AddPoint(const Point& pPoint, const Rgba& pColor)
{
    mVertices.push_back({pPoint, pColor});
}

void PointBatch::AddDottedCircle(const Point& pCenter, float pRadius, int pDots, const Rgba& pColor)
{
    for (int tDot = 0; tDot < pDots; ++tDot)
    {
        const float tAngle = 2.0f * kPi * tDot / pDots;
        AddPoint({pCenter.mX + pRadius * std::cos(tAngle), pCenter.mY + pRadius * std::sin(tAngle)}, pColor);
    }
}

void PointBatch::Draw(float pSize) const
{
    Emit(pSize, nullptr);
}

void PointBatch::DrawOutlined(float pSize) const
{
    Emit(pSize + 2.0f, &kOutline);
    Emit(pSize, nullptr);
}

void PointBatch::Emit(float pSize, const Rgba* pOverride) const
{
    if (mVertices.empty())
    {
        return;
    }
    Ui::UseFlatState();
    glPointSize(pSize);
    if (pOverride)
    {
        SetColor(*pOverride);
    }
    glBegin(GL_POINTS);
    for (const Vertex& tVertex : mVertices)
    {
        if (!pOverride)
        {
            SetColor(tVertex.mColor);
        }
        glVertex2f(tVertex.mPoint.mX, tVertex.mPoint.mY);
    }
    glEnd();
    glPointSize(1.0f);
}

}  // namespace Gfx
