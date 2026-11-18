// Cizilecek seklin islemcideki hali: renkli ucgenlerden cizgi, daire ve cerceve.
#ifndef PRIME_MAP_SRC_GFX_MESH_H
#define PRIME_MAP_SRC_GFX_MESH_H

#include <vector>

#include "Color.h"
#include "ScreenMath.h"

namespace Gfx
{
using Point = PointF;

struct ColorVertex
{
    float mX = 0.0f;
    float mY = 0.0f;
    Rgba mColor;
};

struct TexturedVertex
{
    float mX = 0.0f;
    float mY = 0.0f;
    float mU = 0.0f;
    float mV = 0.0f;
};

const Rgba kOutline = {0.0f, 0.0f, 0.0f, 0.70f};

class Mesh
{
public:
    void clear();
    bool isEmpty() const;
    const std::vector<ColorVertex>& vertices() const;

    void addTriangle(const Point& pA, const Point& pB, const Point& pC, const Rgba& pColor);
    void addQuad(const Point& pA, const Point& pB, const Point& pC, const Point& pD, const Rgba& pColor);
    void addRect(const RectD& pRect, const Rgba& pColor);
    void addDisc(const Point& pCenter, float pRadius, const Rgba& pColor);
    void addIndexed(const std::vector<Point>& pPoints, const std::vector<int>& pIndices, const Rgba& pColor);

    void addLine(const Point& pFrom, const Point& pTo, float pWidth, const Rgba& pColor);
    void addPolyline(const std::vector<Point>& pPoints, bool pClosed, float pWidth, const Rgba& pColor);
    void addDashedLine(const Point& pFrom, const Point& pTo, float pWidth, float pDash, float pGap,
                       const Rgba& pColor);
    void addFrame(const RectD& pRect, float pWidth, const Rgba& pColor);
    void addRing(const Point& pCenter, float pRadius, float pWidth, const Rgba& pColor);
    void addRegularPolygon(const Point& pCenter, float pRadius, int pSides, float pStartDeg, float pWidth,
                           const Rgba& pColor);
    void addDottedCircle(const Point& pCenter, float pRadius, int pCount, float pDotSize, const Rgba& pColor);

    static int segmentsFor(float pRadius);

private:
    void push(const Point& pPoint, const Rgba& pColor);

    std::vector<ColorVertex> mVertices;
};

class OutlinedLines
{
public:
    explicit OutlinedLines(float pOutlineExtra = 2.0f);

    void setOutlineColor(const Rgba& pColor);

    void clear();
    bool isEmpty() const;

    void addLine(const Point& pFrom, const Point& pTo, float pWidth, const Rgba& pColor);
    void addPolyline(const std::vector<Point>& pPoints, bool pClosed, float pWidth, const Rgba& pColor);
    void addRegularPolygon(const Point& pCenter, float pRadius, int pSides, float pStartDeg, float pWidth,
                           const Rgba& pColor);
    void addRing(const Point& pCenter, float pRadius, float pWidth, const Rgba& pColor);
    void addDottedCircle(const Point& pCenter, float pRadius, int pCount, float pDotSize, const Rgba& pColor);

    const Mesh& outline() const;
    const Mesh& fill() const;

private:
    float mOutlineExtra;
    Rgba mOutlineColor = kOutline;
    Mesh mOutline;
    Mesh mFill;
};
}
#endif
