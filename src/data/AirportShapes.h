// AirportShapes.h
// apt.dat dugum zincirlerinden cizilebilir sekiller: Bezier egrilerini
// duzlestirme, delikli yuzey poligonlarini ucgenleme. X-Plane'e ve
// OpenGL'e bagimli DEGIL (testli). Arka plan thread'inde calisir.
//
// Hesaplar havalimani cevresinde metre cinsinden duz bir duzlemde yapilir
// (LocalFrame): birkac km'lik alanda dunyanin egriligi onemsiz, ucgenleme
// ise derecelerle degil metrelerle saglikli calisir.
#pragma once

#include <vector>

#include "data/AirportDetail.h"

// Havalimani merkezli duz duzlem (dogu = +x, kuzey = +y, metre)
struct LocalPoint
{
    double mX = 0.0;
    double mY = 0.0;
};

class LocalFrame
{
public:
    explicit LocalFrame(const LatLon& pOrigin);

    LocalPoint ToLocal(const LatLon& pPoint) const;
    LatLon ToLatLon(const LocalPoint& pPoint) const;

private:
    LatLon mOrigin;
    double mMetersPerDegreeLon;
    double mMetersPerDegreeLat;
};

// Duzlestirilmis zincir: noktalar ve her parcanin (nokta i -> i+1) stili
struct FlatChain
{
    std::vector<LocalPoint> mPoints;  // kapali zincirde son nokta ilk noktayi TEKRARLAMAZ
    std::vector<int> mLineTypes;      // parca basina ilk boya cizgisi (0 = yok)
    std::vector<int> mLightTypes;     // parca basina isik (0 = yok)
    bool mClosed = false;
};

// Cizime hazir noktalar Web Mercator'da, havalimani referansina gore fark
// olarak saklanir (AirportShapes::mOriginX/Y + nokta). Boylece ekrana
// cevirme dogrusal: GPU'da bir kez durur, her karede sadece olcek ve
// kaydirma degisir (Gfx::StaticMesh). float farklar ~0.3 mm hassas.

// Ucgenlenmis yuzey
struct SurfaceMesh
{
    int mSurfaceCode = 1;
    std::vector<PointF> mVertices;  // Mercator farki
    std::vector<int> mTriangles;    // 3'erli indeksler
};

struct LineMesh
{
    std::vector<PointF> mPoints;  // Mercator farki
    std::vector<int> mLineTypes;   // parca basina
    std::vector<int> mLightTypes;  // parca basina
    bool mClosed = false;
};

// Bir havalimaninin cizilebilir sekilleri
struct AirportShapes
{
    double mOriginX = 0.0;  // referans noktasinin Mercator koordinati (0..1)
    double mOriginY = 0.0;
    std::vector<SurfaceMesh> mPavements;              // CIZIM sirasi: alttan uste (dosyanin tersi)
    std::vector<std::vector<PointF>> mPavementEdges;  // yuzeylerin kenar halkalari (ucgenleme basarisizsa da cizilir)
    std::vector<std::vector<PointF>> mBoundaries;     // havalimani siniri halkalari (130)
    std::vector<LineMesh> mLines;                     // boya cizgileri ve isik dizileri (120 + yuzey kenari stilleri)
    int mFailedPolygons = 0;                          // ucgenlenemeyen yuzey sayisi (sadece kenari cizilir)
    size_t mVertexCount = 0;                          // bellek / teshis icin
};

namespace Shapes
{

// Iki dugum arasindaki egrinin noktalari (baslangic dahil, bitis HARIC).
// Kural: P0 = A, P1 = A.kontrol (yoksa A), P2 = 2B - B.kontrol (yoksa B), P3 = B.
// Parca sayisi egri uzunluguna gore: yaklasik pMaxStepM metrede bir nokta.
void AddSegment(const AptNode& pFrom, const AptNode& pTo, const LocalFrame& pFrame, double pMaxStepM,
                std::vector<LocalPoint>& pOut);

FlatChain Flatten(const AptChain& pChain, const LocalFrame& pFrame, double pMaxStepM);

// Isaretli alan (saat yonu tersi pozitif), m^2
double SignedArea(const std::vector<LocalPoint>& pRing);

// Delikli basit poligonu ucgenler. pOuter ve pHoles yonu fark etmez (duzeltilir).
// pOutVertices: kullanilan tum noktalar, pOutTriangles: 3'erli indeksler.
// Basarisizsa (kendini kesen sinir vb.) false.
bool Triangulate(const std::vector<LocalPoint>& pOuter, const std::vector<std::vector<LocalPoint>>& pHoles,
                 std::vector<LocalPoint>& pOutVertices, std::vector<int>& pOutTriangles);

// Ayrintidan cizim sekilleri. pReference: duzlemin merkezi (havalimani referansi).
AirportShapes Build(const AirportDetail& pDetail, const LatLon& pReference);

}  // namespace Shapes
