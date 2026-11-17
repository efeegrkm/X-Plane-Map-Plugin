// apt.dat dugum zincirlerinden cizilebilir sekiller. X-Plane'e ve openGL'e
// bagimli DEGIL (testli). Hesaplar metre cinsinden duz duzlemde (LocalFrame):
// birkac km'lik alanda dunyanin egriligi onemsiz, ucgenleme metrelerle saglikli.
#ifndef PRIME_MAP_SRC_DATA_AIRPORTSHAPES_H
#define PRIME_MAP_SRC_DATA_AIRPORTSHAPES_H

#include <string>
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

    LocalPoint toLocal(const LatLon& pPoint) const;
    LatLon toLatLon(const LocalPoint& pPoint) const;

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

// Cizime hazir noktalar havalimani referansina gore Mercator FARKI olarak
// saklanir: ekrana cevirme dogrusal kalir (Gfx::StaticMesh), float farklar
// ~0.3 mm hassas.

// Ucgenlenmis yuzey
struct SurfaceMesh
{
    int mSurfaceCode = 1;
    std::string mName;              // 110 satirindaki aciklama (fareyle gosterilir)
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

// Cizgi boyunca dizili tek bir isik (101-108), Mercator farki
struct LightPoint
{
    PointF mPoint;
    int mType = 0;
};

// Taksi agi kenari (1202): uclar Mercator farki
struct TaxiEdgeShape
{
    PointF mFrom;
    PointF mTo;
    std::string mName;  // "A", "16L/34R"; bos olabilir
    bool mRunway = false;
    bool mOneWay = false;
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
    std::vector<LightPoint> mLights;                  // isik dizilerinin tek tek lambalari
    std::vector<TaxiEdgeShape> mTaxiEdges;            // ucak taksi agi (1202; yer araci kenarlari haric)
    int mFailedPolygons = 0;                          // ucgenlenemeyen yuzey sayisi (sadece kenari cizilir)
    size_t mVertexCount = 0;                          // bellek / teshis icin
};

namespace Shapes
{

// Iki dugum arasindaki egrinin noktalari (baslangic dahil, bitis HARIC).
// Kural: P0 = A, P1 = A.kontrol (yoksa A), P2 = 2B - B.kontrol (yoksa B), P3 = B.
// Parca sayisi egri uzunluguna gore: yaklasik pMaxStepM metrede bir nokta.
void addSegment(const AptNode& pFrom, const AptNode& pTo, const LocalFrame& pFrame, double pMaxStepM,
                std::vector<LocalPoint>& pOut);

FlatChain flatten(const AptChain& pChain, const LocalFrame& pFrame, double pMaxStepM);

// Isaretli alan (saat yonu tersi pozitif), m^2
double signedArea(const std::vector<LocalPoint>& pRing);

// Delikli basit poligonu ucgenler. pOuter ve pHoles yonu fark etmez (duzeltilir).
// pOutVertices: kullanilan tum noktalar, pOutTriangles: 3'erli indeksler.
// basarisizsa(kendini kesen sinir vb.) false.
bool triangulate(const std::vector<LocalPoint>& pOuter, const std::vector<std::vector<LocalPoint>>& pHoles,
                 std::vector<LocalPoint>& pOutVertices, std::vector<int>& pOutTriangles);

// Ayrintidan cizim sekilleri. pReference: duzlemin merkezi (havalimani referansi).
AirportShapes build(const AirportDetail& pDetail, const LatLon& pReference);

}  // namespace Shapes
#endif  // PRIME_MAP_SRC_DATA_AIRPORTSHAPES_H
