// Mesh.h
// Cizilecek seklin CPU'daki hali: renkli ucgen listesi. X-Plane'e ve
// OpenGL'e bagimli DEGIL (testlerde denenir).
//
// Neden ucgen: modern OpenGL'de cizgi kalinligi (glLineWidth) guvenilir
// degil. Kalin cizgi, daire, cerceve... hepsi burada ucgenlere cevrilir;
// Gfx::Canvas da sadece ucgen cizer. Boylece her sey tek yoldan, tek
// cagriyla (bir vertex buffer) cizilir.
//
// Birimler: koordinat ve kalinliklar boxel (pencere birimi). UI olcek %150
// ise cizgiler de yazi ve semboller gibi %150 kalinlasir.
//
// Ornek:
//   Gfx::Mesh tMesh;
//   tMesh.AddPolyline(tPoints, false, 2.0f, kCyan);
//   tMesh.AddDisc(tCenter, 4.0f, kYellow);
//   pCanvas.Draw(tMesh);
#pragma once

#include <vector>

#include "Color.h"
#include "ScreenMath.h"

namespace Gfx
{

using Point = PointF;

// Vertex buffer'a oldugu gibi kopyalanan kose (Canvas'taki shader ile ayni duzen)
struct ColorVertex
{
    float mX = 0.0f;
    float mY = 0.0f;
    Rgba mColor;
};

// Dokulu cizim icin kose (u, v: doku koordinati 0..1)
struct TexturedVertex
{
    float mX = 0.0f;
    float mY = 0.0f;
    float mU = 0.0f;
    float mV = 0.0f;
};

const Rgba kOutline = {0.0f, 0.0f, 0.0f, 0.70f};  // sembollerin altindaki koyu kontur

class Mesh
{
public:
    void Clear();
    bool IsEmpty() const;
    const std::vector<ColorVertex>& Vertices() const;

    // --- Dolu sekiller ---
    void AddTriangle(const Point& pA, const Point& pB, const Point& pC, const Rgba& pColor);
    // Dortgen: koseler cevre boyunca sirayla
    void AddQuad(const Point& pA, const Point& pB, const Point& pC, const Point& pD, const Rgba& pColor);
    void AddRect(const RectD& pRect, const Rgba& pColor);
    void AddDisc(const Point& pCenter, float pRadius, const Rgba& pColor);
    // pIndices: 3'erli ucgen indeksleri (TriangulatePolygon ciktisi gibi)
    void AddIndexed(const std::vector<Point>& pPoints, const std::vector<int>& pIndices, const Rgba& pColor);

    // --- Cizgiler (pWidth kalinliginda seritler) ---
    void AddLine(const Point& pFrom, const Point& pTo, float pWidth, const Rgba& pColor);
    // Koseler kesik (bevel) birlestirilir: sivri acida bile disari tasmaz.
    // pClosed: son nokta ilk noktaya baglanir.
    void AddPolyline(const std::vector<Point>& pPoints, bool pClosed, float pWidth, const Rgba& pColor);
    void AddDashedLine(const Point& pFrom, const Point& pTo, float pWidth, float pDash, float pGap,
                       const Rgba& pColor);
    // Dikdortgenin ic kenarina pWidth kalinliginda cerceve
    void AddFrame(const RectD& pRect, float pWidth, const Rgba& pColor);
    // Cember (cizgi)
    void AddRing(const Point& pCenter, float pRadius, float pWidth, const Rgba& pColor);
    // Duzgun cokgenin kenarlari; pStartDeg ilk kosenin acisi (0 = sag, 90 = yukari)
    void AddRegularPolygon(const Point& pCenter, float pRadius, int pSides, float pStartDeg, float pWidth,
                           const Rgba& pColor);
    // Cember uzerinde pCount tane kucuk kare nokta (NDB sembolu)
    void AddDottedCircle(const Point& pCenter, float pRadius, int pCount, float pDotSize, const Rgba& pColor);

    // pRadius yaricapli daire icin parca sayisi (buyuk dairede daha cok)
    static int SegmentsFor(float pRadius);

private:
    void Push(const Point& pPoint, const Rgba& pColor);

    std::vector<ColorVertex> mVertices;  // her 3 kose bir ucgen
};

// Iki katmanli cizgi: once koyu kontur (daha kalin), ustune renkli cizgi.
// Her zemin renginde okunur kalir. Once TUM konturlar, sonra TUM renkli
// cizgiler cizilir; boylece bir cizginin konturu digerinin ustune binmez.
class OutlinedLines
{
public:
    explicit OutlinedLines(float pOutlineExtra = 2.0f);

    void Clear();
    bool IsEmpty() const;

    void AddLine(const Point& pFrom, const Point& pTo, float pWidth, const Rgba& pColor);
    void AddPolyline(const std::vector<Point>& pPoints, bool pClosed, float pWidth, const Rgba& pColor);
    void AddRegularPolygon(const Point& pCenter, float pRadius, int pSides, float pStartDeg, float pWidth,
                           const Rgba& pColor);
    void AddRing(const Point& pCenter, float pRadius, float pWidth, const Rgba& pColor);
    void AddDottedCircle(const Point& pCenter, float pRadius, int pCount, float pDotSize, const Rgba& pColor);

    const Mesh& Outline() const;
    const Mesh& Fill() const;

private:
    float mOutlineExtra;
    Mesh mOutline;
    Mesh mFill;
};

}  // namespace Gfx
