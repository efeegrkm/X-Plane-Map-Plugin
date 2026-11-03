// Batch.h
// Toplu cizim yardimcilari: ayni turden parcalari (cizgi, ucgen, nokta)
// once bir listede toplar, sonra tek seferde cizer.
//
// Neden: katman kodu OpenGL ayrintisi (glBegin/glEnd, glVertex, durum
// ayarlari) bilmeden "su noktalari su renkle ciz" diyebilsin. OpenGL
// cagrilari sadece Batch.cpp icindedir. X-Plane 11 eklentilere uyumluluk
// profilli (eski) OpenGL verdigi icin simdilik glBegin/glEnd kullaniliyor;
// ileride vertex buffer'a gecmek gerekirse degisecek yer sadece burasi.
//
// Ornek:
//   Gfx::LineBatch tLines;
//   tLines.AddPolyline(tPoints, false, kCyan);
//   tLines.DrawOutlined(2.0f);   // once koyu kontur, sonra renkli cizgi
#pragma once

#include <vector>

#include "ui/UiDraw.h"

namespace Gfx
{

struct Point
{
    float mX;
    float mY;
};

const Rgba kOutline = {0.0f, 0.0f, 0.0f, 0.70f};  // her zeminde okunur kalsin diye alttaki koyu kontur

class LineBatch
{
public:
    void Clear();
    bool IsEmpty() const;

    void AddLine(const Point& pFrom, const Point& pTo, const Rgba& pColor);
    // pClosed: son nokta ilk noktaya baglanir (poligon kenari)
    void AddPolyline(const std::vector<Point>& pPoints, bool pClosed, const Rgba& pColor);
    // Duzgun cokgen kenarlari; pStartDeg ilk kosenin acisi (0 = sag, 90 = yukari)
    void AddRegularPolygon(const Point& pCenter, float pRadius, int pSides, float pStartDeg, const Rgba& pColor);
    void AddCircle(const Point& pCenter, float pRadius, const Rgba& pColor);

    void Draw(float pWidth) const;
    // Once ayni cizgiler kOutline renginde (pWidth + 2) kalinlikta, sonra kendi renkleriyle
    void DrawOutlined(float pWidth) const;

private:
    void Emit(float pWidth, const Rgba* pOverride) const;

    struct Vertex
    {
        Point mPoint;
        Rgba mColor;
    };
    std::vector<Vertex> mVertices;  // her iki kose bir cizgi
};

class TriangleBatch
{
public:
    void Clear();
    bool IsEmpty() const;

    void AddTriangle(const Point& pA, const Point& pB, const Point& pC, const Rgba& pColor);
    // Dortgen (koseler sirayla, cevre boyunca)
    void AddQuad(const Point& pA, const Point& pB, const Point& pC, const Point& pD, const Rgba& pColor);
    void AddDisc(const Point& pCenter, float pRadius, const Rgba& pColor);
    // pIndices: 3'erli ucgen indeksleri (TriangulatePolygon ciktisi gibi)
    void AddIndexed(const std::vector<Point>& pPoints, const std::vector<int>& pIndices, const Rgba& pColor);

    void Draw() const;

private:
    struct Vertex
    {
        Point mPoint;
        Rgba mColor;
    };
    std::vector<Vertex> mVertices;  // her uc kose bir ucgen
};

class PointBatch
{
public:
    void Clear();
    bool IsEmpty() const;

    void AddPoint(const Point& pPoint, const Rgba& pColor);
    // Cember uzerinde esit aralikli noktalar (NDB sembolu)
    void AddDottedCircle(const Point& pCenter, float pRadius, int pDots, const Rgba& pColor);

    void Draw(float pSize) const;
    void DrawOutlined(float pSize) const;

private:
    void Emit(float pSize, const Rgba* pOverride) const;

    struct Vertex
    {
        Point mPoint;
        Rgba mColor;
    };
    std::vector<Vertex> mVertices;
};

}  // namespace Gfx
