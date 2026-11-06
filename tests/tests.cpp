// tests.cpp
// X-Plane OLMADAN calisan testler: projeksiyon, koordinat matematigi,
// cografi hesaplar, cizim modeli, ayar dosyasi, katman yoneticisi.
//
// Derleme: cmake -DMAP_TESTS=ON ... ; calistirma: map_tests(.exe)
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "Drawings.h"
#include "Geo.h"
#include "LabelPlacer.h"
#include "LayerManager.h"
#include "MapTheme.h"
#include "Log.h"
#include "MapFormat.h"
#include "MapView.h"
#include "ScreenMath.h"
#include "Settings.h"
#include "data/AirportGeometryStore.h"
#include "data/AirwayDatabase.h"
#include "data/AirportShapes.h"
#include "data/AptDatIndexer.h"
#include "data/AptDatParser.h"
#include "data/AptLine.h"
#include "data/LocalizerMatch.h"
#include "data/DsfReader.h"
#include "data/LandColorField.h"
#include "data/NavDatabase.h"
#include "data/SceneryIndex.h"
#include "data/SevenZip.h"
#include "data/TerrainClasses.h"
#include "data/TerrainNoise.h"
#include "data/TerrainRaster.h"
#include "data/TerrainStore.h"
#include "data/TerrainTriangles.h"
#include "gfx/Mesh.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

// Log.cpp (XPLMDebugString kullanir) yerine bos bir taklit
void LogLine(std::string_view /*pText*/)
{
}

namespace
{

int gFailCount = 0;
int gPassCount = 0;

#define CHECK(pCondition, ...)                                  \
    do                                                          \
    {                                                           \
        if (pCondition)                                         \
        {                                                       \
            ++gPassCount;                                       \
        }                                                       \
        else                                                    \
        {                                                       \
            ++gFailCount;                                       \
            std::printf("FAIL %s:%d: ", __FILE__, __LINE__);    \
            std::printf(__VA_ARGS__);                           \
            std::printf("\n");                                  \
        }                                                       \
    } while (0)

bool Near(double pA, double pB, double pEpsilon)
{
    return std::fabs(pA - pB) <= pEpsilon;
}

// ---- OpenGL matris yardimcilari (sutun oncelikli) ----

struct Mat4
{
    float mM[16];
};

Mat4 Identity()
{
    Mat4 tResult{};
    tResult.mM[0] = 1;
    tResult.mM[5] = 1;
    tResult.mM[10] = 1;
    tResult.mM[15] = 1;
    return tResult;
}

Mat4 Ortho(double pLeft, double pRight, double pBottom, double pTop)
{
    Mat4 tResult = Identity();
    tResult.mM[0] = float(2 / (pRight - pLeft));
    tResult.mM[5] = float(2 / (pTop - pBottom));
    tResult.mM[10] = -1;
    tResult.mM[12] = float(-(pRight + pLeft) / (pRight - pLeft));
    tResult.mM[13] = float(-(pTop + pBottom) / (pTop - pBottom));
    return tResult;
}

// ---------------------------------------------------------------------------

// X-Plane pencere cizerken: projeksiyon boxel'den NDC'ye, viewport piksel.
// UI olcek s ise pencerenin W piksel genisligi W/s boxel eder.
void TestWindowTransform(const char* pName, double pUiScale, int pWidth, int pHeight, double pOriginX)
{
    const int tViewport[4] = {0, 0, pWidth, pHeight};
    const Mat4 tModelview = Identity();
    const Mat4 tProjection = Ortho(pOriginX, pOriginX + pWidth / pUiScale, 0, pHeight / pUiScale);

    Affine2D tBoxelToPixel;
    CHECK(AffineFromGL(tModelview.mM, tProjection.mM, tViewport, &tBoxelToPixel), "%s: matris okunmali", pName);

    double tPx = 0.0;
    double tPy = 0.0;
    tBoxelToPixel.Apply(pOriginX, 0, &tPx, &tPy);
    CHECK(Near(tPx, 0, 0.01) && Near(tPy, 0, 0.01), "%s: sol alt kose (%.2f,%.2f)", pName, tPx, tPy);
    tBoxelToPixel.Apply(pOriginX + pWidth / pUiScale, pHeight / pUiScale, &tPx, &tPy);
    CHECK(Near(tPx, pWidth, 0.05) && Near(tPy, pHeight, 0.05), "%s: sag ust kose (%.2f,%.2f)", pName, tPx, tPy);

    // Pencere alanini piksel dikdortgenine cevirme (scissor icin)
    RectD tBoxels;
    tBoxels.mLeft = pOriginX + 100;
    tBoxels.mRight = pOriginX + 500;
    tBoxels.mBottom = 50;
    tBoxels.mTop = 350;
    const RectD tPixels = TransformRectBBox(tBoxelToPixel, tBoxels);
    CHECK(Near(tPixels.mLeft, 100 * pUiScale, 0.05) && Near(tPixels.mTop, 350 * pUiScale, 0.05),
          "%s: scissor dikdortgeni (%.2f,%.2f)", pName, tPixels.mLeft, tPixels.mTop);

    // Olcek: boxel basina piksel
    const double tScale =
        std::sqrt(std::fabs(tBoxelToPixel.mA * tBoxelToPixel.mD - tBoxelToPixel.mB * tBoxelToPixel.mC));
    CHECK(Near(tScale, pUiScale, 1e-4), "%s: UI olcek %.4f", pName, tScale);
}

void TestScreenMath()
{
    TestWindowTransform("UI %100", 1.0, 1920, 1080, 0);
    TestWindowTransform("UI %150", 1.5, 2560, 1440, 0);
    TestWindowTransform("UI %200, sol monitor (eksi x)", 2.0, 3840, 2160, -1920);

    // Perspektif matris reddedilmeli
    {
        Mat4 tProjection = Identity();
        tProjection.mM[11] = -1;
        tProjection.mM[15] = 0;
        const Mat4 tModelview = Identity();
        const int tViewport[4] = {0, 0, 100, 100};
        Affine2D tResult;
        CHECK(!AffineFromGL(tModelview.mM, tProjection.mM, tViewport, &tResult), "perspektif reddedilmeli");
    }

    // Compose / Invert
    {
        Affine2D tMatrix;
        tMatrix.mA = 2;
        tMatrix.mB = 0.5;
        tMatrix.mC = -0.3;
        tMatrix.mD = 1.5;
        tMatrix.mTx = 10;
        tMatrix.mTy = -4;
        Affine2D tInverse;
        CHECK(Invert(tMatrix, &tInverse), "ters alinmali");
        const Affine2D tIdentity = Compose(tInverse, tMatrix);
        CHECK(Near(tIdentity.mA, 1, 1e-12) && Near(tIdentity.mD, 1, 1e-12) && Near(tIdentity.mB, 0, 1e-12) &&
                  Near(tIdentity.mC, 0, 1e-12) && Near(tIdentity.mTx, 0, 1e-9) && Near(tIdentity.mTy, 0, 1e-9),
              "A^-1 * A = birim");
    }

    // Kesisim
    {
        RectD tFirst{0, 100, 100, 0};
        RectD tSecond{50, 150, 150, 50};
        RectD tOut;
        CHECK(IntersectRect(tFirst, tSecond, &tOut) && tOut.mLeft == 50 && tOut.mTop == 100 &&
                  tOut.mRight == 100 && tOut.mBottom == 50,
              "kesisim");
        RectD tFar{200, 300, 300, 200};
        CHECK(!IntersectRect(tFirst, tFar, &tOut), "kesismeyen");
    }
}

// ---------------------------------------------------------------------------

void TestMercator()
{
    CHECK(Near(Mercator::LonToX(-180), 0.0, 1e-12) && Near(Mercator::LonToX(180), 1.0, 1e-12), "boylam uclari");
    CHECK(Near(Mercator::LatToY(0), 0.5, 1e-12), "ekvator ortada");
    CHECK(Near(Mercator::LatToY(Mercator::kMaxLat), 1.0, 1e-6), "en kuzey enlem = 1");
    CHECK(Near(Mercator::LatToY(-Mercator::kMaxLat), 0.0, 1e-6), "en guney enlem = 0");
    CHECK(Mercator::LatToY(89.9) <= 1.0, "kutup kirpilmali");
    const double tLats[] = {-60.0, -12.5, 0.0, 41.0, 41.2620, 70.0};
    for (double tLat : tLats)
    {
        CHECK(Near(Mercator::YToLat(Mercator::LatToY(tLat)), tLat, 1e-9), "enlem gidis-donus %.4f", tLat);
    }
    CHECK(Near(Mercator::XToLon(Mercator::LonToX(28.742)), 28.742, 1e-12), "boylam gidis-donus");
}

MapView MakeView()
{
    MapView tView;
    RectD tViewport;
    tViewport.mLeft = 100;
    tViewport.mRight = 1100;
    tViewport.mBottom = 300;
    tViewport.mTop = 900;
    tView.SetViewport(tViewport);
    tView.SetCenter({41.0, 29.0});
    tView.SetZoom(8.25);
    return tView;
}

void TestMapView()
{
    // Merkez, alanin ortasina duser
    {
        const MapView tView = MakeView();
        double tX = 0.0;
        double tY = 0.0;
        tView.LatLonToBoxel({41.0, 29.0}, &tX, &tY);
        CHECK(Near(tX, 600, 1e-6) && Near(tY, 600, 1e-6), "merkez ortada (%.3f,%.3f)", tX, tY);
        const LatLon tCenter = tView.Center();
        CHECK(Near(tCenter.mLat, 41.0, 1e-9) && Near(tCenter.mLon, 29.0, 1e-9), "Center() geri verir");
    }

    // Gidis-donus: lat/lon -> boxel -> lat/lon
    {
        const MapView tView = MakeView();
        const LatLon tPoints[] = {{41.2620, 28.7420}, {40.1281, 32.9951}, {38.0, 26.0}, {42.9, 29.5}};
        for (const LatLon& tPoint : tPoints)
        {
            double tX = 0.0;
            double tY = 0.0;
            tView.LatLonToBoxel(tPoint, &tX, &tY);
            const LatLon tBack = tView.BoxelToLatLon(tX, tY);
            CHECK(Near(tBack.mLat, tPoint.mLat, 1e-9) && Near(tBack.mLon, tPoint.mLon, 1e-9),
                  "lat/lon gidis-donus %.4f,%.4f -> %.9f,%.9f", tPoint.mLat, tPoint.mLon, tBack.mLat, tBack.mLon);
        }
    }

    // Gidis-donus: boxel -> lat/lon -> boxel (tiklama -> nokta -> ekran)
    {
        const MapView tView = MakeView();
        for (int tIndex = 0; tIndex < 25; ++tIndex)
        {
            const double tX = 100 + 1000.0 * ((tIndex % 5) + 0.5) / 5;
            const double tY = 300 + 600.0 * ((tIndex / 5) + 0.5) / 5;
            double tBackX = 0.0;
            double tBackY = 0.0;
            tView.LatLonToBoxel(tView.BoxelToLatLon(tX, tY), &tBackX, &tBackY);
            CHECK(Near(tBackX, tX, 1e-6) && Near(tBackY, tY, 1e-6), "boxel gidis-donus (%.1f,%.1f)", tX, tY);
        }
    }

    // Yukari = kuzey, saga = dogu
    {
        const MapView tView = MakeView();
        const LatLon tUpperRight = tView.BoxelToLatLon(1000, 800);
        CHECK(tUpperRight.mLat > 41.0 && tUpperRight.mLon > 29.0, "sag ust kuzeydogu");
    }

    // Zoom imlecin etrafinda: imlecin altindaki yer sabit kalmali
    {
        const double tDeltas[] = {1.5, -2.25, 0.5, 30.0, -30.0};  // son ikisi sinira takilir
        MapView tView = MakeView();
        for (double tDelta : tDeltas)
        {
            const double tCursorX = 250;
            const double tCursorY = 740;
            const LatLon tBefore = tView.BoxelToLatLon(tCursorX, tCursorY);
            tView.ZoomAt(tCursorX, tCursorY, tDelta);
            const LatLon tAfter = tView.BoxelToLatLon(tCursorX, tCursorY);
            CHECK(Near(tBefore.mLat, tAfter.mLat, 1e-9) && Near(tBefore.mLon, tAfter.mLon, 1e-9),
                  "zoom %+.2f sonrasi imlec kaydi: %.9f,%.9f -> %.9f,%.9f", tDelta, tBefore.mLat, tBefore.mLon,
                  tAfter.mLat, tAfter.mLon);
        }
        CHECK(Near(tView.Zoom(), MapView::kMinZoom, 1e-12), "zoom alt sinira kirpildi: %.2f", tView.Zoom());
    }

    // Surukleme: icerik fareyle birlikte gider
    {
        MapView tView = MakeView();
        const LatLon tPoint = tView.BoxelToLatLon(500, 500);
        tView.PanByBoxels(30, -20);
        double tX = 0.0;
        double tY = 0.0;
        tView.LatLonToBoxel(tPoint, &tX, &tY);
        CHECK(Near(tX, 530, 1e-6) && Near(tY, 480, 1e-6), "surukleme (%.3f,%.3f)", tX, tY);
    }

    // Olcek: ekvatorda zoom 3, bir boxel = 2*pi*R / 2048 metre
    {
        MapView tView;
        tView.SetCenter({0.0, 0.0});
        tView.SetZoom(3.0);
        CHECK(Near(tView.MetersPerBoxel(), 2 * 3.14159265358979 * 6378137.0 / 2048.0, 0.01), "metre/boxel");
        tView.SetCenter({60.0, 0.0});
        CHECK(Near(tView.MetersPerBoxel(), 2 * 3.14159265358979 * 6378137.0 / 2048.0 * 0.5, 0.01),
              "60 derecede olcek yariya iner");
    }

    // 180. boylam: dunyanin en yakin kopyasi secilir
    {
        MapView tView = MakeView();
        tView.SetCenter({0.0, 179.5});
        double tX = 0.0;
        double tY = 0.0;
        tView.LatLonToBoxel({0.0, -179.5}, &tX, &tY);
        CHECK(tX > 600, "-179.5 boylami merkezin saginda olmali (x=%.1f)", tX);
        const GeoBounds tBounds = tView.VisibleBounds();
        CHECK(tBounds.mWest < 179.5 && tBounds.mEast > 179.5, "gorunen alan merkezi kapsar");
    }
}

// ---------------------------------------------------------------------------

double TriangleArea(const std::vector<LatLon>& pPoints, const std::vector<int>& pTriangles)
{
    double tSum = 0;
    for (size_t tIndex = 0; tIndex + 2 < pTriangles.size(); tIndex += 3)
    {
        const LatLon& tA = pPoints[pTriangles[tIndex]];
        const LatLon& tB = pPoints[pTriangles[tIndex + 1]];
        const LatLon& tC = pPoints[pTriangles[tIndex + 2]];
        tSum += std::fabs((tB.mLon - tA.mLon) * (tC.mLat - tA.mLat) - (tC.mLon - tA.mLon) * (tB.mLat - tA.mLat)) / 2;
    }
    return tSum;
}

double PolygonArea(const std::vector<LatLon>& pPoints)
{
    double tSum = 0;
    for (size_t tIndex = 0; tIndex < pPoints.size(); ++tIndex)
    {
        const LatLon& tA = pPoints[tIndex];
        const LatLon& tB = pPoints[(tIndex + 1) % pPoints.size()];
        tSum += tA.mLon * tB.mLat - tB.mLon * tA.mLat;
    }
    return std::fabs(tSum) / 2;
}

void TestGeo()
{
    const LatLon tLtfm{41.2620, 28.7420};
    const LatLon tLtac{40.1281, 32.9951};
    const double tDistance = DistanceNm(tLtfm, tLtac);
    CHECK(tDistance > 200 && tDistance < 210, "LTFM-LTAC mesafe %.1f NM (gercek ~380 km = ~205 NM)", tDistance);
    CHECK(Near(DistanceNm({0, 0}, {0, 1}), 60.04, 0.1), "ekvatorda 1 derece ~60 NM");

    const LatLon tMid = GreatCircleInterpolate(tLtfm, tLtac, 0.5);
    CHECK(Near(DistanceNm(tLtfm, tMid), tDistance / 2, 0.01), "orta nokta");

    std::vector<LatLon> tDense;
    DensifySegment(tLtfm, tLtac, 5.0, tDense);
    CHECK(tDense.size() >= 41 && tDense.size() <= 42, "5 NM parcalara bolme: %d", static_cast<int>(tDense.size()));
    CHECK(Near(tDense[0].mLat, tLtfm.mLat, 1e-12), "ilk nokta baslangic");
    std::vector<LatLon> tShort;
    DensifySegment(tLtfm, {41.27, 28.75}, 5.0, tShort);
    CHECK(tShort.size() == 1, "kisa segment bolunmez");

    // Kare
    const std::vector<LatLon> tSquare = {{41, 29}, {41, 30}, {42, 30}, {42, 29}};
    std::vector<int> tTriangles;
    CHECK(TriangulatePolygon(tSquare, tTriangles) && tTriangles.size() == 6, "kare 2 ucgen");
    CHECK(Near(TriangleArea(tSquare, tTriangles), PolygonArea(tSquare), 1e-9), "kare alan");

    // Saat yonunde L sekli (icbukey)
    const std::vector<LatLon> tShapeL = {{0, 0}, {2, 0}, {2, 1}, {1, 1}, {1, 2}, {0, 2}};
    CHECK(TriangulatePolygon(tShapeL, tTriangles) && tTriangles.size() == 12, "L 4 ucgen");
    CHECK(Near(TriangleArea(tShapeL, tTriangles), PolygonArea(tShapeL), 1e-9), "L alan %.3f vs %.3f",
          TriangleArea(tShapeL, tTriangles), PolygonArea(tShapeL));
    const std::vector<LatLon> tShapeLReversed(tShapeL.rbegin(), tShapeL.rend());
    CHECK(TriangulatePolygon(tShapeLReversed, tTriangles) &&
              Near(TriangleArea(tShapeLReversed, tTriangles), PolygonArea(tShapeLReversed), 1e-9),
          "ters yon L");

    // Kendini kesen (papyon)
    const std::vector<LatLon> tBowTie = {{0, 0}, {1, 1}, {1, 0}, {0, 1}};
    CHECK(!TriangulatePolygon(tBowTie, tTriangles), "papyon reddedilmeli");

    // Ayni dogru uzerinde kose
    const std::vector<LatLon> tCollinear = {{0, 0}, {0, 1}, {0, 2}, {1, 2}, {1, 0}};
    CHECK(TriangulatePolygon(tCollinear, tTriangles) &&
              Near(TriangleArea(tCollinear, tTriangles), PolygonArea(tCollinear), 1e-9),
          "dogrusal kose");

    // Dogru (alansiz)
    const std::vector<LatLon> tLine = {{0, 0}, {0, 1}, {0, 2}};
    CHECK(!TriangulatePolygon(tLine, tTriangles), "alansiz reddedilmeli");

    // Hedef nokta: ekvatorda doguya 60 NM ~ 1 derece, kuzeye 60 NM ~ 1 derece
    const LatLon tEast = DestinationPoint({0, 0}, 90.0, 60.04);
    CHECK(Near(tEast.mLat, 0.0, 1e-9) && Near(tEast.mLon, 1.0, 1e-3), "doguya 60 NM: %.5f,%.5f", tEast.mLat,
          tEast.mLon);
    const LatLon tNorth = DestinationPoint({41.0, 29.0}, 0.0, 60.04);
    CHECK(Near(tNorth.mLat, 42.0, 1e-3) && Near(tNorth.mLon, 29.0, 1e-9), "kuzeye 60 NM");
    const LatLon tFar = DestinationPoint(tLtfm, 117.0, 123.4);
    CHECK(Near(DistanceNm(tLtfm, tFar), 123.4, 1e-6), "hedef noktaya mesafe geri tutar");
}

void TestDrawings()
{
    Drawings tModel;
    tModel.SetMode(Drawings::Mode::Pan);
    tModel.AddVertex({1, 1});
    CHECK(tModel.ShapeCount() == 0, "gezin modunda ekleme yok");

    tModel.SetMode(Drawings::Mode::Point);
    tModel.AddVertex({1, 1});
    tModel.AddVertex({2, 2});
    CHECK(tModel.ShapeCount() == 2 && tModel.Shapes()[1].mNumber == 2, "iki nokta P1 P2");

    tModel.SetMode(Drawings::Mode::Route);
    tModel.AddVertex({0, 0});
    CHECK(tModel.Current() && tModel.Current()->mPoints.size() == 1, "rota basladi");
    tModel.SetMode(Drawings::Mode::Region);  // 1 koseli rota atilmali
    CHECK(tModel.ShapeCount() == 2 && !tModel.Current(), "1 koseli rota atildi");

    tModel.AddVertex({0, 0});
    tModel.AddVertex({0, 1});
    tModel.AddVertex({1, 1});
    tModel.Finish();
    CHECK(tModel.ShapeCount() == 3 && tModel.Shapes()[2].mKind == Drawings::ShapeKind::Region &&
              tModel.Shapes()[2].mTriangles.size() == 3,
          "bolge B1 ucgenlendi");

    tModel.SetMode(Drawings::Mode::Route);
    tModel.AddVertex({0, 0});
    tModel.AddVertex({0, 1});
    tModel.AddVertex({0, 2});
    tModel.Undo();
    CHECK(tModel.Current() && tModel.Current()->mPoints.size() == 2, "geri al: kose");
    tModel.Finish();
    CHECK(tModel.ShapeCount() == 4 && tModel.Shapes()[3].mNumber == 1 && tModel.Shapes()[3].mPoints.size() == 2,
          "rota R1");
    tModel.Undo();
    CHECK(tModel.ShapeCount() == 3, "geri al: sekil");
    tModel.AddVertex({5, 5});
    tModel.AddVertex({5, 6});
    tModel.Finish();
    CHECK(tModel.Shapes()[3].mNumber == 1, "numara geri alinca tekrar kullanilir");

    tModel.Clear();
    CHECK(tModel.ShapeCount() == 0 && !tModel.Current(), "temizle");
}

// ---------------------------------------------------------------------------

// Mesh'in koselerini kapsayan dikdortgen
RectD MeshBounds(const Gfx::Mesh& pMesh)
{
    RectD tBounds;
    tBounds.mLeft = 1e9;
    tBounds.mBottom = 1e9;
    tBounds.mRight = -1e9;
    tBounds.mTop = -1e9;
    for (const Gfx::ColorVertex& tVertex : pMesh.Vertices())
    {
        tBounds.mLeft = std::min<double>(tBounds.mLeft, tVertex.mX);
        tBounds.mRight = std::max<double>(tBounds.mRight, tVertex.mX);
        tBounds.mBottom = std::min<double>(tBounds.mBottom, tVertex.mY);
        tBounds.mTop = std::max<double>(tBounds.mTop, tVertex.mY);
    }
    return tBounds;
}

// Ucgenlerin toplam alani (yon fark etmez)
double MeshArea(const Gfx::Mesh& pMesh)
{
    const std::vector<Gfx::ColorVertex>& tVertices = pMesh.Vertices();
    double tArea = 0.0;
    for (size_t tIndex = 0; tIndex + 2 < tVertices.size(); tIndex += 3)
    {
        const Gfx::ColorVertex& tA = tVertices[tIndex];
        const Gfx::ColorVertex& tB = tVertices[tIndex + 1];
        const Gfx::ColorVertex& tC = tVertices[tIndex + 2];
        tArea += std::fabs((tB.mX - tA.mX) * (tC.mY - tA.mY) - (tC.mX - tA.mX) * (tB.mY - tA.mY)) * 0.5;
    }
    return tArea;
}

void TestMesh()
{
    const Rgba tRed = {1.0f, 0.0f, 0.0f, 1.0f};

    Gfx::Mesh tLine;
    tLine.AddLine({0.0f, 0.0f}, {10.0f, 0.0f}, 2.0f, tRed);
    const RectD tLineBounds = MeshBounds(tLine);
    CHECK(tLine.Vertices().size() == 6, "cizgi = 2 ucgen");
    CHECK(Near(tLineBounds.mBottom, -1.0, 1e-6) && Near(tLineBounds.mTop, 1.0, 1e-6) &&
              Near(tLineBounds.mRight, 10.0, 1e-6),
          "cizgi kalinligi iki yana yari yariya");
    CHECK(Near(MeshArea(tLine), 20.0, 1e-4), "cizgi alani = boy x kalinlik");
    CHECK(tLine.Vertices()[0].mColor.mR == 1.0f, "renk koseye yazildi");

    Gfx::Mesh tEmpty;
    tEmpty.AddLine({3.0f, 3.0f}, {3.0f, 3.0f}, 2.0f, tRed);
    CHECK(tEmpty.IsEmpty(), "sifir uzunluklu cizgi atlanir");

    Gfx::Mesh tOpen;
    tOpen.AddPolyline({{0.0f, 0.0f}, {10.0f, 0.0f}, {10.0f, 10.0f}}, false, 2.0f, tRed);
    CHECK(tOpen.Vertices().size() == 2 * 6 + 1 * 6, "acik cizgi: 2 parca + 1 kose birlesimi");

    Gfx::Mesh tClosed;
    tClosed.AddPolyline({{0.0f, 0.0f}, {10.0f, 0.0f}, {10.0f, 10.0f}, {0.0f, 10.0f}}, true, 2.0f, tRed);
    const RectD tClosedBounds = MeshBounds(tClosed);
    CHECK(tClosed.Vertices().size() == 4 * 6 + 4 * 6, "kapali cizgi: 4 parca + 4 kose");
    CHECK(Near(tClosedBounds.mLeft, -1.0, 1e-6) && Near(tClosedBounds.mTop, 11.0, 1e-6),
          "kose birlesimi disari en fazla yari kalinlik tasar");

    Gfx::Mesh tFrame;
    RectD tBox;
    tBox.mLeft = 0.0;
    tBox.mBottom = 0.0;
    tBox.mRight = 20.0;
    tBox.mTop = 10.0;
    tFrame.AddFrame(tBox, 1.0f, tRed);
    const RectD tFrameBounds = MeshBounds(tFrame);
    CHECK(tFrame.Vertices().size() == 4 * 6, "cerceve = 4 dikdortgen");
    CHECK(Near(tFrameBounds.mLeft, 0.0, 1e-9) && Near(tFrameBounds.mRight, 20.0, 1e-9) &&
              Near(tFrameBounds.mTop, 10.0, 1e-9),
          "cerceve kutunun icinde kalir");
    CHECK(Near(MeshArea(tFrame), 20.0 * 10.0 - 18.0 * 8.0, 1e-4), "cerceve ust uste binmez");

    Gfx::Mesh tDisc;
    tDisc.AddDisc({0.0f, 0.0f}, 10.0f, tRed);
    CHECK(tDisc.Vertices().size() == static_cast<size_t>(Gfx::Mesh::SegmentsFor(10.0f)) * 3, "daire parca sayisi");
    CHECK(MeshArea(tDisc) > 3.0 * 100.0 && MeshArea(tDisc) < 3.1416 * 100.0, "daire alani ~ pi r^2");

    Gfx::Mesh tDots;
    tDots.AddDottedCircle({0.0f, 0.0f}, 7.0f, 12, 2.0f, tRed);
    CHECK(tDots.Vertices().size() == 12 * 6 && Near(MeshArea(tDots), 12 * 4.0, 1e-3), "12 kare nokta");

    Gfx::OutlinedLines tOutlined(2.0f);
    tOutlined.AddLine({0.0f, 0.0f}, {10.0f, 0.0f}, 1.5f, tRed);
    CHECK(Near(MeshBounds(tOutlined.Outline()).mTop, 1.75, 1e-6) && Near(MeshBounds(tOutlined.Fill()).mTop, 0.75, 1e-6),
          "kontur cizgiden 2 boxel kalin");
    CHECK(tOutlined.Outline().Vertices()[0].mColor.mA == Gfx::kOutline.mA, "kontur rengi");
}

// ---------------------------------------------------------------------------

void TestMapFormat()
{
    std::string tText = FormatLatLon({41.26201, -28.74205});
    CHECK(tText == "41.26201 N  28.74205 W", "lat/lon bicimi: '%s'", tText.c_str());

    tText = FormatGridLabel(41.0, true, 1.0);
    CHECK(tText == "41N", "izgara etiketi derece: '%s'", tText.c_str());
    tText = FormatGridLabel(41.5, true, 0.5);
    CHECK(tText == "41 30N", "izgara etiketi dakika: '%s'", tText.c_str());
    tText = FormatGridLabel(28.0 + 5.5 / 60.0, false, 0.5 / 60.0);
    CHECK(tText == "28 05.5E", "izgara etiketi yarim dakika: '%s'", tText.c_str());
    tText = FormatGridLabel(-(12.0 + 20.0 / 60.0), false, 10.0 / 60.0);
    CHECK(tText == "12 20W", "izgara etiketi bati: '%s'", tText.c_str());
    tText = FormatGridLabel(29.0 + 59.9999999 / 60.0, true, 1.0 / 60.0);
    CHECK(tText == "30 00N", "yuvarlama tasmasi: '%s'", tText.c_str());

    CHECK(Near(ChooseGridStepDeg(100.0, 90.0), 1.0, 1e-12), "izgara araligi 1 derece");
    CHECK(Near(ChooseGridStepDeg(1000.0, 90.0), 10.0 / 60.0, 1e-12), "izgara araligi 10 dakika");
    CHECK(Near(ChooseGridStepDeg(1.0, 90.0), 30.0, 1e-12), "izgara araligi ust sinir");

    CHECK(Near(ChooseScaleBarNm(0.1, 120.0), 10.0, 1e-9), "olcek 10 NM");
    CHECK(Near(ChooseScaleBarNm(1.0, 120.0), 100.0, 1e-9), "olcek 100 NM");
    CHECK(Near(ChooseScaleBarNm(0.013, 120.0), 1.0, 1e-9), "olcek 1 NM");
    CHECK(Near(ChooseScaleBarNm(0.003, 120.0), 0.2, 1e-9), "olcek 0.2 NM");
}

void TestSettings()
{
    const char* tPath = "map_test_settings.ini";
    {
        FILE* tFile = std::fopen(tPath, "w");
        CHECK(tFile != nullptr, "test dosyasi acilmali");
        if (tFile)
        {
            std::fputs("# comment\n[window]\n left = -1200 \ntop=900\n\n[view]\nzoom=7.25\nfollow_aircraft=0\n"
                       "broken line\n; another comment\n",
                       tFile);
            std::fclose(tFile);
        }
    }

    Settings tSettings;
    CHECK(tSettings.Load(tPath), "dosya okunmali");
    CHECK(tSettings.GetInt("window", "left", 0) == -1200, "bosluklu deger");
    CHECK(tSettings.GetInt("window", "top", 0) == 900, "tamsayi");
    CHECK(Near(tSettings.GetDouble("view", "zoom", 0.0), 7.25, 1e-12), "ondalik nokta ile");
    CHECK(!tSettings.GetBool("view", "follow_aircraft", true), "bool 0");
    CHECK(tSettings.GetInt("window", "missing", 42) == 42, "olmayan anahtar varsayilan");
    CHECK(Near(tSettings.GetDouble("missing", "missing", 1.5), 1.5, 1e-12), "olmayan bolum varsayilan");

    tSettings.SetDouble("view", "center_lat", 41.123456789);
    tSettings.SetBool("layers", "grid", true);
    CHECK(tSettings.Save(), "kaydedilmeli");

    Settings tReloaded;
    CHECK(tReloaded.Load(tPath), "yeniden okunmali");
    CHECK(Near(tReloaded.GetDouble("view", "center_lat", 0.0), 41.123456789, 1e-8), "ondalik geri okunur");
    CHECK(tReloaded.GetBool("layers", "grid", false), "bool geri okunur");
    CHECK(tReloaded.GetInt("window", "left", 0) == -1200, "eski deger korunur");
    std::remove(tPath);

    Settings tMissing;
    CHECK(!tMissing.Load("no_such_file.ini"), "olmayan dosya false");
    CHECK(tMissing.GetInt("a", "b", 3) == 3, "bos ayarlar varsayilan verir");
}

// Testler icin sayac tutan sahte katman
class CountingLayer : public Layer
{
public:
    CountingLayer(const char* pId, bool pVisible, std::string* pTrace)
        : Layer(pId, pId, pVisible)
        , mTrace(pTrace)
    {
    }

    void Draw(const DrawContext& /*pContext*/) override
    {
        *mTrace += "D" + Id() + " ";
    }

    void DrawLabels(const DrawContext& /*pContext*/) override
    {
        *mTrace += "L" + Id() + " ";
    }

private:
    std::string* mTrace;
};

// Alt katmanli sahte katman
class PartedLayer : public Layer
{
public:
    PartedLayer()
        : Layer("parted", "Parted", true)
    {
        AddSubLayer("one", "One");
        AddSubLayer("two", "Two", false);
    }

    void Draw(const DrawContext& /*pContext*/) override
    {
    }
};

void TestSubLayers()
{
    LayerManager tLayers;
    tLayers.Add(std::make_unique<PartedLayer>());
    Layer& tLayer = *tLayers.At(0);
    CHECK(tLayer.SubLayerCount() == 2 && tLayer.IsSubLayerVisible(0) && !tLayer.IsSubLayerVisible(1),
          "alt katman varsayilanlari");
    CHECK(tLayer.IsSubLayerVisible(7) && tLayer.VisibleSubLayerCount() == 1, "gecersiz indeks gorunur sayilir");
    CHECK(LayerManager::SubLayerKey(tLayer, 1) == "parted.two", "alt katman anahtari");

    const MapTheme& tScenery = ThemeFor(MapMode::Scenery);
    Settings tSettings;
    tLayer.SetSubLayerVisible(0, false);
    tLayer.SetSubLayerVisible(1, true);
    tLayers.SaveVisibility(tSettings, tScenery);
    CHECK(!tSettings.GetBool("layers.scenery", "parted.one", true) &&
              tSettings.GetBool("layers.scenery", "parted.two", false),
          "alt katman kaydi");

    Settings tEmpty;
    tLayers.LoadVisibility(tEmpty, tScenery);
    CHECK(tLayer.IsSubLayerVisible(0) && !tLayer.IsSubLayerVisible(1), "ayar yoksa alt katman varsayilani");
    tLayers.LoadVisibility(tSettings, tScenery);
    CHECK(!tLayer.IsSubLayerVisible(0) && tLayer.IsSubLayerVisible(1), "alt katman geri okunur");
}

void TestLayerManager()
{
    std::string tTrace;
    LayerManager tLayers;
    tLayers.Add(std::make_unique<CountingLayer>("a", true, &tTrace));
    tLayers.Add(std::make_unique<CountingLayer>("b", false, &tTrace));
    tLayers.Add(std::make_unique<CountingLayer>("c", true, &tTrace));
    CHECK(tLayers.Count() == 3 && tLayers.At(3) == nullptr && tLayers.At(-1) == nullptr, "katman sayisi");

    DrawContext tContext;
    tLayers.DrawAll(tContext);
    CHECK(tTrace == "Da Dc La Lc ", "cizim sirasi: once geometri sonra yazi, kapali atlanir: '%s'", tTrace.c_str());

    const MapTheme& tScenery = ThemeFor(MapMode::Scenery);
    Settings tSettings;
    tLayers.At(0)->SetVisible(false);
    tLayers.SaveVisibility(tSettings, tScenery);
    CHECK(!tSettings.GetBool("layers.scenery", "a", true) && !tSettings.GetBool("layers.scenery", "b", true) &&
              tSettings.GetBool("layers.scenery", "c", false),
          "gorunurluk kaydi (mod bolumu)");

    tSettings.SetBool("layers.scenery", "b", true);
    tLayers.At(0)->SetVisible(true);
    tLayers.LoadVisibility(tSettings, tScenery);
    CHECK(!tLayers.At(0)->IsVisible() && tLayers.At(1)->IsVisible() && tLayers.At(2)->IsVisible(),
          "gorunurluk okuma");

    Settings tEmpty;
    tLayers.LoadVisibility(tEmpty, tScenery);
    CHECK(tLayers.At(0)->IsVisible() && !tLayers.At(1)->IsVisible(), "ayar yoksa katmanin varsayilani");

    Settings tLegacy;  // 0.12 ve oncesi: tek [layers] bolumu -> Scenery'ye tasinir
    tLegacy.SetBool("layers", "a", false);
    tLayers.LoadVisibility(tLegacy, tScenery);
    CHECK(!tLayers.At(0)->IsVisible(), "eski [layers] bolumu Scenery'de okunur");
    tLayers.LoadVisibility(tLegacy, ThemeFor(MapMode::Vfr));
    CHECK(tLayers.At(0)->IsVisible(), "eski bolum diger modlari etkilemez");
    tLegacy.SetBool("layers", "b", true);  // varsayilan kapali katman (Taxi routes gibi)
    tLayers.LoadVisibility(tLegacy, tScenery);
    CHECK(!tLayers.At(1)->IsVisible(), "varsayilan kapali katman eski bolumden acilmaz");
    CHECK(!LayerManager::ThemeDefault(*tLayers.At(1), tScenery) && LayerManager::ThemeDefault(*tLayers.At(0), tScenery),
          "katmanin mod varsayilani");

    // Mod varsayilani: VFR'de ILS kapali
    LayerManager tModeLayers;
    tModeLayers.Add(std::make_unique<CountingLayer>("ils", true, &tTrace));
    tModeLayers.LoadVisibility(tEmpty, ThemeFor(MapMode::Vfr));
    CHECK(!tModeLayers.At(0)->IsVisible(), "VFR temasi ILS'i varsayilan kapatir");
    tModeLayers.LoadVisibility(tEmpty, ThemeFor(MapMode::IfrLow));
    CHECK(tModeLayers.At(0)->IsVisible(), "IFR Low'da ILS acik");
}

void TestMapTheme()
{
    CHECK(ModeFromId("vfr", MapMode::Scenery) == MapMode::Vfr && ModeFromId("ifr_high", MapMode::Scenery) ==
              MapMode::IfrHigh && ModeFromId("xyz", MapMode::IfrLow) == MapMode::IfrLow,
          "mod kimligi okuma");
    CHECK(NextMode(MapMode::IfrHigh) == MapMode::Scenery && NextMode(MapMode::Scenery) == MapMode::Vfr,
          "siradaki mod donguseldir");
    CHECK(LayerSectionFor(MapMode::IfrLow) == "layers.ifr_low", "mod bolumu adi");
    for (int tIndex = 0; tIndex < kMapModeCount; ++tIndex)
    {
        const MapTheme& tTheme = ThemeFor(static_cast<MapMode>(tIndex));
        CHECK(static_cast<int>(tTheme.mMode) == tIndex, "tema sirasi: %s", tTheme.mId);
    }
    CHECK(AllTerrainStyles().size() == static_cast<size_t>(kMapModeCount) &&
              AllTerrainStyles()[static_cast<int>(MapMode::Vfr)].mStyle == TerrainStyle::Elevation,
          "zemin bicimleri mod sirasiyla");

    // Rakim rampasi: alcakta yesil baskin, yuksekte kahverengi (kirmizi > mavi, koyulasir)
    const TerrainColor tLow = ElevationColor(0.0);
    const TerrainColor tHigh = ElevationColor(4000.0);
    CHECK(tLow.mG > tLow.mR && tHigh.mR > tHigh.mB && tHigh.mG < tLow.mG, "rakim rampasi");
    CHECK(ElevationColor(-50.0).mG == tLow.mG && ElevationColor(20000.0).mR == ElevationColor(9000.0).mR,
          "rampa uclarda sabit");
}

// ---------------------------------------------------------------------------

NavRecord MakeRecord(NavKind pKind, const char* pId, float pLat, float pLon)
{
    NavRecord tRecord;
    tRecord.mKind = pKind;
    tRecord.mId = pId;
    tRecord.mLat = pLat;
    tRecord.mLon = pLon;
    return tRecord;
}

bool Contains(const std::vector<int>& pIndices, int pValue)
{
    for (int tValue : pIndices)
    {
        if (tValue == pValue)
        {
            return true;
        }
    }
    return false;
}

void TestNavDatabase()
{
    // ILS yonu: "manyetik * 360 + gercek"
    float tTrue = 0.0f;
    short tMagnetic = 0;
    DecodeLocalizerCourse(84112.655, &tTrue, &tMagnetic);
    CHECK(Near(tTrue, 232.655, 1e-3) && tMagnetic == 233, "ILS kodlu yon: %.3f / %d", tTrue, tMagnetic);
    DecodeLocalizerCourse(11556.087, &tTrue, &tMagnetic);
    CHECK(Near(tTrue, 36.087, 1e-3) && tMagnetic == 32, "LTAC 03L yonu: %.3f / %d", tTrue, tMagnetic);
    DecodeLocalizerCourse(36.5, &tTrue, &tMagnetic);
    CHECK(Near(tTrue, 36.5, 1e-6) && tMagnetic == -1, "kodsuz yon");

    CHECK(IsPlaceholderId("----") && IsPlaceholderId("") && !IsPlaceholderId("IANC") && !IsPlaceholderId("A-1"),
          "yer tutucu kimlik");
    CHECK(IsTacanChannel(85) && !IsTacanChannel(11380) && !IsTacanChannel(0), "TACAN kanali");

    CHECK(IsMajorAirportId("LTFM") && IsMajorAirportId("KSEA"), "ICAO kodu buyuk");
    CHECK(!IsMajorAirportId("LT51") && !IsMajorAirportId("K1A2") && !IsMajorAirportId("XPA0001") &&
              !IsMajorAirportId("ABC"),
          "rakamli / farkli uzunluk kucuk");

    NavDatabase tDatabase;
    CHECK(!tDatabase.IsReady(), "Build'den once hazir degil");
    tDatabase.Add(MakeRecord(NavKind::Airport, "LTFM", 41.262f, 28.742f));   // 0
    tDatabase.Add(MakeRecord(NavKind::Airport, "LTAC", 40.128f, 32.995f));   // 1
    tDatabase.Add(MakeRecord(NavKind::Vor, "IST", 40.9615f, 28.8107f));      // 2
    tDatabase.Add(MakeRecord(NavKind::Dme, "IST", 40.9615f, 28.8107f));      // 3: VOR ile eslesmeli
    tDatabase.Add(MakeRecord(NavKind::Dme, "IST", 43.5327f, 16.2891f));      // 4: cok uzak, tek basina
    tDatabase.Add(MakeRecord(NavKind::Ils, "IAKR", 40.1442f, 33.0088f));     // 5
    tDatabase.Add(MakeRecord(NavKind::Dme, "IAKR", 40.1200f, 32.9900f));     // 6: ILS DME (~1.6 NM)
    tDatabase.Add(MakeRecord(NavKind::Fix, "ABISI", 40.3627f, 31.9524f));    // 7
    tDatabase.Add(MakeRecord(NavKind::Fix, "WEST1", 10.5f, -179.5f));        // 8: 180. boylamin batisi
    tDatabase.Add(MakeRecord(NavKind::Fix, "EAST1", 10.5f, 179.5f));         // 9: 180. boylamin dogusu
    tDatabase.Build();
    CHECK(tDatabase.IsReady() && tDatabase.Count() == 10, "hazir, 10 kayit");

    CHECK(tDatabase.At(2).mHasDme && tDatabase.At(3).mHidden, "VOR/DME eslesti");
    CHECK(!tDatabase.At(4).mHidden, "uzak DME eslesmez");
    CHECK(tDatabase.At(5).mHasDme && tDatabase.At(6).mHidden, "ILS DME eslesti");
    CHECK(tDatabase.CountOf(NavKind::Dme) == 1, "izgarada tek basina 1 DME: %d", tDatabase.CountOf(NavKind::Dme));
    CHECK(tDatabase.CountOf(NavKind::Airport) == 2 && tDatabase.CountOf(NavKind::Fix) == 3, "tur sayilari");

    // Istanbul cevresi: sadece LTFM gelmeli (LTAC baska hucrede)
    GeoBounds tIstanbul;
    tIstanbul.mSouth = 40.5;
    tIstanbul.mNorth = 41.5;
    tIstanbul.mWest = 28.0;
    tIstanbul.mEast = 29.5;
    std::vector<int> tResult;
    tDatabase.Query(NavKind::Airport, tIstanbul, tResult);
    CHECK(tResult.size() == 1 && tResult[0] == 0, "Istanbul sorgusu: %d sonuc", static_cast<int>(tResult.size()));
    tResult.clear();
    tDatabase.Query(NavKind::Vor, tIstanbul, tResult);
    CHECK(tResult.size() == 1 && tResult[0] == 2, "Istanbul VOR");
    tResult.clear();
    tDatabase.Query(NavKind::Dme, tIstanbul, tResult);
    CHECK(tResult.empty(), "eslesen DME sorguda gelmez");

    // Turkiye: iki havalimani
    GeoBounds tTurkey;
    tTurkey.mSouth = 35.0;
    tTurkey.mNorth = 43.0;
    tTurkey.mWest = 25.0;
    tTurkey.mEast = 45.0;
    tResult.clear();
    tDatabase.Query(NavKind::Airport, tTurkey, tResult);
    CHECK(tResult.size() == 2 && Contains(tResult, 0) && Contains(tResult, 1), "Turkiye sorgusu");

    // 180. boylami gecen alan (bati 179, dogu 181): iki fix de gelmeli
    GeoBounds tDateLine;
    tDateLine.mSouth = 10.0;
    tDateLine.mNorth = 11.0;
    tDateLine.mWest = 179.0;
    tDateLine.mEast = 181.0;
    tResult.clear();
    tDatabase.Query(NavKind::Fix, tDateLine, tResult);
    CHECK(tResult.size() == 2 && Contains(tResult, 8) && Contains(tResult, 9), "180. boylam sorgusu: %d",
          static_cast<int>(tResult.size()));

    // Tum dunya: her kayit bir kez
    GeoBounds tWorld;
    tWorld.mSouth = -90.0;
    tWorld.mNorth = 90.0;
    tWorld.mWest = -200.0;
    tWorld.mEast = 300.0;
    tResult.clear();
    tDatabase.Query(NavKind::Fix, tWorld, tResult);
    CHECK(tResult.size() == 3, "dunya sorgusu tekrarsiz: %d", static_cast<int>(tResult.size()));

    tDatabase.Clear();
    tResult.clear();
    tDatabase.Query(NavKind::Fix, tWorld, tResult);
    CHECK(!tDatabase.IsReady() && tResult.empty(), "temizlendi");
}

RectD Box(double pLeft, double pBottom, double pRight, double pTop)
{
    RectD tBox;
    tBox.mLeft = pLeft;
    tBox.mBottom = pBottom;
    tBox.mRight = pRight;
    tBox.mTop = pTop;
    return tBox;
}

void TestLabelPlacer()
{
    LabelPlacer tPlacer;
    tPlacer.Reset(Box(0, 0, 500, 300));
    CHECK(tPlacer.TryPlace(Box(10, 10, 60, 22)), "ilk etiket");
    CHECK(!tPlacer.TryPlace(Box(50, 15, 100, 27)), "cakisan reddedilir");
    CHECK(tPlacer.TryPlace(Box(60, 10, 110, 22)), "kenar kenara degen kabul");
    CHECK(!tPlacer.TryPlace(Box(480, 10, 520, 22)), "alan disina tasan reddedilir");
    CHECK(tPlacer.TryPlace(Box(200, 100, 400, 200)), "buyuk kutu (cok hucre)");
    CHECK(!tPlacer.TryPlace(Box(250, 150, 260, 160)), "buyuk kutunun icindeki reddedilir");
    CHECK(tPlacer.PlacedCount() == 3, "3 kutu yerlesti");

    tPlacer.Reset(Box(-1000, -500, -200, 100));  // eksi koordinatli alan (sol monitor)
    CHECK(tPlacer.PlacedCount() == 0, "reset bosaltir");
    CHECK(tPlacer.TryPlace(Box(-990, -490, -950, -470)) && tPlacer.TryPlace(Box(-240, 80, -201, 99)),
          "eksi koordinatlar");

    // Cok sayida kucuk etiket: izgara gibi dizilenler hep kabul
    tPlacer.Reset(Box(0, 0, 1000, 1000));
    int tPlaced = 0;
    for (int tIndex = 0; tIndex < 400; ++tIndex)
    {
        const double tX = (tIndex % 20) * 50.0;
        const double tY = (tIndex / 20) * 50.0;
        if (tPlacer.TryPlace(Box(tX, tY, tX + 40, tY + 12)))
        {
            ++tPlaced;
        }
    }
    CHECK(tPlaced == 400, "400 ayrik etiket: %d", tPlaced);

    // Reserve: sembol yeri kontrolsuz ayrilir, etiketler ustune binemez
    tPlacer.Reset(Box(0, 0, 500, 300));
    tPlacer.Reserve(Box(100, 100, 120, 120));
    tPlacer.Reserve(Box(110, 110, 130, 130));  // cakissa da ayrilir
    CHECK(tPlacer.PlacedCount() == 2, "iki sembol ayrildi");
    CHECK(!tPlacer.TryPlace(Box(115, 90, 160, 102)), "etiket sembolun ustune binmez");
    CHECK(tPlacer.TryPlace(Box(131, 110, 170, 122)), "sembolun yanina etiket konur");
    tPlacer.Reserve(Box(-50, -50, 10, 10));  // alan disina tasan kismi kirpilir
    CHECK(!tPlacer.TryPlace(Box(0, 0, 8, 8)), "kirpilmis sembol de korunur");
    tPlacer.Reserve(Box(600, 600, 700, 700));  // tamamen disarida: yok sayilir
    CHECK(tPlacer.PlacedCount() == 4, "disaridaki sembol eklenmez: %d", tPlacer.PlacedCount());
}

// ---------------------------------------------------------------------------

// Metni satir satir tarayiciya verir (her satirin bayt konumuyla)
void ScanText(AptDatScanner& pScanner, std::string_view pText)
{
    size_t tStart = 0;
    while (tStart < pText.size())
    {
        size_t tStop = pText.find('\n', tStart);
        if (tStop == std::string_view::npos)
        {
            tStop = pText.size();
        }
        pScanner.AddLine(pText.substr(tStart, tStop - tStart), tStart);
        tStart = tStop + 1;
    }
    pScanner.Finish(pText.size());
}

void TestAptLine()
{
    AptLine tLine("  100\t29.87 -122.30000000 03R  Boeing Field  King \r");
    int tCode = 0;
    double tWidth = 0.0;
    double tLon = 0.0;
    std::string tName;
    CHECK(tLine.Int(tCode) && tCode == 100, "satir kodu (bas bosluk, tab)");
    CHECK(!tLine.Int(tCode), "29.87 tamsayi olarak okunmaz");
    CHECK(tLine.Double(tWidth) && Near(tWidth, 29.87, 1e-12), "ondalik nokta (yerel ayardan bagimsiz)");
    CHECK(tLine.Double(tLon) && Near(tLon, -122.3, 1e-12), "eksi sayi");
    CHECK(tLine.Token(tName) && tName == "03R", "kelime");
    CHECK(tLine.Rest() == "Boeing Field  King" && tLine.AtEnd(), "satirin geri kalani (ic bosluklar korunur)");
    CHECK(AptRowCode("# yorum") == -1 && AptRowCode("") == -1 && AptRowCode("1302 city X") == 1302, "satir kodu");

    // Gercek apt.dat satiri (Global Airports, LTAC)
    Runway tRunway;
    CHECK(AptDatParser::ParseRunway("100 59.00 1 0 0.25 1 2 0 03R 40.1140623 32.9831635 0 65 3 2 1 0 "
                                    "21L 40.1413103 33.0091255 0 60 3 2 1 0",
                                    &tRunway),
          "pist satiri okunmali");
    CHECK(Near(tRunway.mWidthM, 59.0, 1e-9) && tRunway.mSurface == RunwaySurface::Paved && tRunway.mCenterLights &&
              tRunway.mEdgeLights == 2,
          "genislik, yuzey, isiklar");
    CHECK(tRunway.mEnds[0].mName == "03R" && tRunway.mEnds[1].mName == "21L" && tRunway.mEnds[0].mMarkings == 3 &&
              tRunway.mEnds[0].mApproachLights == 2 && tRunway.mEnds[0].mTouchdownLights,
          "pist uclari: numara, isaret, yaklasma isigi, TDZ");
    CHECK(Near(tRunway.mEnds[0].mOverrunM, 65.0, 1e-9) && Near(tRunway.mEnds[1].mPosition.mLon, 33.0091255, 1e-9),
          "overrun, konum");
    const double tLength = DistanceNm(tRunway.mEnds[0].mPosition, tRunway.mEnds[1].mPosition) * 1852.0;
    CHECK(tLength > 3700 && tLength < 3800, "LTAC 03R/21L boyu %.0f m (gercek 3750 m)", tLength);
    CHECK(AptDatParser::ParseRunway("100 30 3 0 0.25 0 0 0 09 41.0 29.0 150.5 0 1 0 0 0 27 41.0 29.02 0 0 1 0 0 0\r",
                                    &tRunway) &&
              tRunway.mSurface == RunwaySurface::Unpaved && Near(tRunway.mEnds[0].mDisplacedThresholdM, 150.5, 1e-9),
          "cim pist, kayik esik, CRLF");
    CHECK(!AptDatParser::ParseRunway("100 30 3 0 0.25 0 0 0 09 41.0", &tRunway), "eksik satir reddedilir");
    CHECK(AptDatParser::ParseWaterRunway("101 182.88 0 09 47.900116 -92.577834 27 47.899159 -92.536945", &tRunway) &&
              tRunway.mWater && tRunway.mSurface == RunwaySurface::Water && tRunway.mEnds[1].mName == "27",
          "su pisti (gercek satir)");

    AirportType tType = AirportType::Land;
    int tElevation = 0;
    std::string tId;
    std::string tAirportName;
    CHECK(AptDatParser::ParseHeader("1   3125 0 0 LTAC Esenboga Intl \r", &tType, &tElevation, &tId, &tAirportName) &&
              tId == "LTAC" && tAirportName == "Esenboga Intl" && tElevation == 3125,
          "baslik: '%s' '%s'", tId.c_str(), tAirportName.c_str());
    CHECK(AptDatParser::ParseHeader("17 -12 0 0 XHEL Heliport", &tType, &tElevation, &tId, &tAirportName) &&
              tType == AirportType::Heliport && tElevation == -12,
          "heliport, deniz seviyesi alti");
}

void TestAptDatScanner()
{
    // Iki "dosya": once gelen kazanir (ozel sahne Global'i ezer)
    const std::string tFirst = "I\n1130 Version\n\n"
                               "1 3125 0 0 LTAC Esenboga\n"
                               "1302 datum_lat 40.128\n"
                               "1302 datum_lon 32.995\n"
                               "1302 icao_code LTAC\n"
                               "100 59.00 1 0 0.25 1 2 0 03R 40.11 32.98 0 65 3 2 1 0 21L 40.14 33.00 0 60 3 2 1 0\n"
                               "110 1 0.25 0.00 Taxiway\n"
                               "111 40.12 32.99\n"
                               "113 40.12 33.05 1\n"
                               "1054 118100 TWR\n"
                               "17 2000 0 0 XHEL Heliport\n"
                               "102 H1 40.0 33.0 0 10 10 1 0 0 0.25 0\n"
                               "99\n";
    const std::string tSecond = "1 3125 0 0 LTAC Eski Kayit\n"
                                "100 45 1 0 0.25 0 0 0 03 40.0 32.0 0 0 0 0 0 0 21 40.1 32.1 0 0 0 0 0 0\n"
                                "1 100 0 0 LTFM Istanbul\n"
                                "100 60 2 0 0.25 0 0 0 16L 41.30 28.72 0 0 0 0 0 0 34R 41.25 28.73 0 0 0 0 0 0\n"
                                "100 60 2 0 0.25 0 0 0 16R 41.30 28.74 0 0 0 0 0 0 34L 41.25 28.75 0 0 0 0 0 0";
    std::unordered_set<std::string> tSeen;
    std::vector<AirportGeometry> tAirports;
    AptDatScanner tScanFirst(0, tSeen, tAirports);
    ScanText(tScanFirst, tFirst);
    AptDatScanner tScanSecond(1, tSeen, tAirports);
    ScanText(tScanSecond, tSecond);
    CHECK(tScanFirst.AirportCount() == 2 && tScanSecond.AirportCount() == 1 && tAirports.size() == 3,
          "LTAC + heliport, sonra sadece LTFM (eski LTAC atlandi): %d", static_cast<int>(tAirports.size()));
    if (tAirports.size() != 3)
    {
        return;
    }
    const AirportGeometry& tLtac = tAirports[0];
    CHECK(tLtac.mId == "LTAC" && tLtac.mName == "Esenboga" && tLtac.mIcaoCode == "LTAC" && tLtac.mHasTower &&
              tLtac.mRunways.size() == 1 && tLtac.mPavementCount == 1 && tLtac.mNodeCount == 2,
          "LTAC ozeti");
    CHECK(Near(tLtac.mReference.mLat, 40.128, 1e-9) && Near(tLtac.mReference.mLon, 32.995, 1e-9),
          "referans = datum");
    CHECK(Near(tLtac.mBounds.mWest, 32.98, 1e-9) && Near(tLtac.mBounds.mEast, 33.05, 1e-9) &&
              Near(tLtac.mBounds.mNorth, 40.14, 1e-9),
          "kapsayan kutu pist ve dugumlerden");
    const AirportGeometry& tHeliport = tAirports[1];
    CHECK(tHeliport.mType == AirportType::Heliport && tHeliport.mRunways.empty() &&
              Near(tHeliport.mReference.mLat, 40.0, 1e-9),
          "pistsiz heliport da alinir (helipad konumu)");
    CHECK(tAirports[2].mId == "LTFM" && tAirports[2].mLocation.mFile == 1 && tAirports[2].mRunways.size() == 2,
          "son satirda 99 olmasa da havalimani kapanir");

    // Bayt araligi: dosyada tam o havalimaninin metnini gosterir
    const AptFileLocation& tLocation = tLtac.mLocation;
    const std::string tSlice = tFirst.substr(tLocation.mOffset, tLocation.mLength);
    CHECK(tSlice.rfind("1 3125 0 0 LTAC", 0) == 0 && tSlice.find("XHEL") == std::string::npos, "LTAC bayt araligi");
    const AirportDetail tDetail = AptDatParser::ParseAirport(tSlice);
    CHECK(tDetail.mId == "LTAC" && tDetail.mPavements.size() == 1 && tDetail.mRunways.size() == 1 &&
              tDetail.mSkippedLineCount == 0,
          "araliktan ayrinti okunur");
}

void TestAptIndexCache()
{
    AirportIndex tIndex;
    tIndex.mFiles = {"C:/X-Plane 11/Custom Scenery/Global Airports/Earth nav data/apt.dat"};
    AirportGeometry tAirport;
    tAirport.mId = "LTAC";
    tAirport.mIcaoCode = "LTAC";
    tAirport.mName = "Esenboga Intl";
    tAirport.mType = AirportType::Land;
    tAirport.mElevationFt = 3125;
    tAirport.mHasTower = true;
    tAirport.mReference = {40.128, 32.995};
    tAirport.mBounds = {40.10, 32.97, 40.15, 33.02};
    tAirport.mPavementCount = 12;
    tAirport.mNodeCount = 345;
    tAirport.mLocation = {0, 123456789012ull, 4567};
    Runway tRunway;
    tRunway.mWidthM = 59.0;
    tRunway.mSurfaceCode = 1;
    tRunway.mEnds[0].mName = "03R";
    tRunway.mEnds[0].mPosition = {40.114, 32.983};
    tRunway.mEnds[1].mName = "21L";
    tRunway.mEnds[1].mOverrunM = 60.0;
    tRunway.mEnds[1].mTouchdownLights = true;
    tAirport.mRunways.push_back(tRunway);
    tIndex.mAirports.push_back(tAirport);

    const std::string tPath = "map_test_airport_index.bin";
    CHECK(AptDatIndexer::SaveCache(tPath, "anahtar-1", tIndex), "onbellek yazilir");
    AirportIndex tLoaded;
    CHECK(AptDatIndexer::LoadCache(tPath, "anahtar-1", tLoaded), "ayni anahtarla okunur");
    CHECK(tLoaded.mFiles == tIndex.mFiles && tLoaded.mAirports.size() == 1, "dosya ve havalimani sayisi");
    if (tLoaded.mAirports.size() == 1)
    {
        const AirportGeometry& tBack = tLoaded.mAirports[0];
        CHECK(tBack.mId == "LTAC" && tBack.mName == "Esenboga Intl" && tBack.mElevationFt == 3125 && tBack.mHasTower &&
                  Near(tBack.mBounds.mEast, 33.02, 1e-12) && tBack.mLocation.mOffset == 123456789012ull &&
                  tBack.mLocation.mLength == 4567 && tBack.mNodeCount == 345,
              "ozet alanlari birebir");
        CHECK(tBack.mRunways.size() == 1 && tBack.mRunways[0].mEnds[1].mName == "21L" &&
                  Near(tBack.mRunways[0].mEnds[1].mOverrunM, 60.0, 1e-12) &&
                  tBack.mRunways[0].mEnds[1].mTouchdownLights,
              "pistler birebir");
    }
    AirportIndex tStale;
    CHECK(!AptDatIndexer::LoadCache(tPath, "anahtar-2", tStale), "dosyalar degisti (anahtar farkli): reddedilir");
    CHECK(!AptDatIndexer::LoadCache("yok_boyle_dosya.bin", "anahtar-1", tStale), "dosya yok: reddedilir");
    std::remove(tPath.c_str());
}

void TestAptDatDetail()
{
    // Spesifikasyondaki KBFI ornegi + gercek veride gorulen bicimler
    const std::string tText =
        "1 21 1 0 KBFI Boeing Field King Co Intl\n"
        "100 29.87 1 0 0.15 0 2 1 13L 47.53801700 -122.30746100 73.15 0.00 2 0 0 1 "
        "31R 47.52919200 -122.30000000 110.95 0.00 2 0 0 1\n"
        "101 49 1 08 35.04420900 -106.59855700 26 35.04420911 -106.59855711\n"
        "102 H1 47.53918248 -122.30722302 2.00 10.06 10.06 1 0 0 0.25 0\n"
        "21 47.53666659 -122.30585255 2 150.28 3.30\n"
        "110 1 0.25 150.29 A2 Exit\n"
        "111 47.53770968 -122.30849802\n"
        "111 47.53742819 -122.30825844 3\n"
        "112 47.53752190 -122.30826710 47.53757385 -122.30824831 3 102\n"
        "114 47.53768630 -122.30834929 47.53768690 -122.30838150 3 102\n"
        "111 47.53760 -122.30835\n"
        "111 47.53758 -122.30833 51 59 105\n"
        "113 47.53760 -122.30831\n"
        "120 Line B1\n"
        "111 47.53969864 -122.31276189 51\n"
        "111 47.53977825 -122.31255145 1\n"
        "115 47.54002296 -122.31189878\n"
        "130 Airport Boundary\n"
        "111 47.53 -122.31\n"
        "111 47.53 -122.29\n"
        "113 47.55 -122.30\n"
        "14 47.52917900 -122.30434900 242.78 0 ATC Tower\n"
        "15 47.52926674 -122.29919589 304.16 A8 Run Up\n"
        "18 47.52920400 -122.30412800 1 BCN\n"
        "19 47.53900921 -122.30868700 1 WS\n"
        "20 47.54099177 -122.31031317 235.71 0 2 {@L}A1{@R}31R-13L\n"
        "50 12775 ATIS\n"
        "54 11800 TWR\n"
        "1050 127750 ATIS\n"
        "1054 118300 KBFI TWR\n"
        "1000 Calm and South flow\n"
        "1001 KSEA 000 359 5\n"
        "1100 16C 11920 arrivals jets|turboprops|props 160340 161161 Arrival 16C\n"
        "1200\n"
        "1201 47.46360812 -122.30613338 both 5416 A_stop\n"
        "1201 47.46370812 -122.30613338 junc 5417\n"
        "1202 5416 5417 twoway taxiway_A B\n"
        "1204 ils 34R\n"
        "1206 5417 5416 oneway\n"
        "1300 47.43931757 -122.29806851 88.78 tie_down jets|turboprops A10\n"
        "1301 d airline dal\n"
        "1302 icao_code KBFI\n"
        "1400 47.44374472 -122.30463464 88.1 baggage_train 3 Svc Baggage\n"
        "1401 47.44103438 -122.30382493 0.0 baggage_train Luggage Train Destination South 2\n"
        "99\n";
    const AirportDetail tDetail = AptDatParser::ParseAirport(tText);
    CHECK(tDetail.mId == "KBFI" && tDetail.mElevationFt == 21 && tDetail.mSkippedLineCount == 0,
          "baslik, bozuk satir yok (%d)", tDetail.mSkippedLineCount);
    CHECK(tDetail.mRunways.size() == 2 && tDetail.mRunways[1].mWater && tDetail.mHelipads.size() == 1 &&
              Near(tDetail.mHelipads[0].mWidthM, 10.06, 1e-9),
          "kara + su pisti, helipad");
    CHECK(tDetail.mLightObjects.size() == 1 && tDetail.mLightObjects[0].mType == 2 &&
              Near(tDetail.mLightObjects[0].mGlideslopeDeg, 3.3, 1e-9),
          "PAPI (6 alanli satir)");

    CHECK(tDetail.mPavements.size() == 1 && tDetail.mPavements[0].mName == "A2 Exit" &&
              tDetail.mPavements[0].mRings.size() == 2,
          "yuzey: dis sinir + delik");
    if (tDetail.mPavements.size() == 1 && tDetail.mPavements[0].mRings.size() == 2)
    {
        const AptChain& tOuter = tDetail.mPavements[0].mRings[0];
        CHECK(tOuter.mClosed && tOuter.mNodes.size() == 4 && tOuter.mNodes[2].mHasControl &&
                  Near(tOuter.mNodes[2].mControl.mLat, 47.53757385, 1e-12),
              "Bezier dugumu ve kontrol noktasi");
        CHECK(tOuter.mNodes[1].mLineTypes[0] == 3 && tOuter.mNodes[2].mLineTypes[0] == 3 &&
                  tOuter.mNodes[2].mLightType == 102,
              "stil: boya + isik");
        const AptNode& tMulti = tDetail.mPavements[0].mRings[1].mNodes[1];
        CHECK(tMulti.mLineTypes[0] == 51 && tMulti.mLineTypes[1] == 59 && tMulti.mLightType == 105,
              "GERCEK VERI: iki boya + isik ('51 59 105')");
    }
    CHECK(tDetail.mLinearFeatures.size() == 1 && tDetail.mLinearFeatures[0].mChains.size() == 1 &&
              !tDetail.mLinearFeatures[0].mChains[0].mClosed &&
              tDetail.mLinearFeatures[0].mChains[0].mNodes.size() == 3,
          "cizgi: 115 ile biten acik zincir");
    CHECK(tDetail.mBoundaries.size() == 1 && tDetail.mBoundaries[0].mRings[0].mClosed, "havalimani siniri");
    CHECK(tDetail.mViewpoints.size() == 1 && tDetail.mViewpoints[0].mName == "ATC Tower" &&
              Near(tDetail.mViewpoints[0].mHeightFt, 242.78, 1e-9) &&
              tDetail.mBeacons.size() == 1 && tDetail.mBeacons[0].mType == 1 && tDetail.mWindsocks.size() == 1 &&
              tDetail.mWindsocks[0].mLit,
          "bakis noktasi, beacon, ruzgar tulumu");
    CHECK(tDetail.mSigns.size() == 1 && tDetail.mSigns[0].mText == "{@L}A1{@R}31R-13L" && tDetail.mSigns[0].mSize == 2,
          "tabela");
    CHECK(tDetail.mFrequencies.size() == 2 && tDetail.mFrequencies[0].mKhz == 127750 &&
              tDetail.mFrequencies[1].mKind == AptFrequencyKind::Tower && tDetail.mFrequencies[1].mName == "KBFI TWR",
          "1050'li satirlar varsa 50'li satirlar yok sayilir");
    CHECK(tDetail.mTrafficFlowCount == 1, "trafik akisi sayilir");
    CHECK(tDetail.mTaxiNodes.size() == 2 && tDetail.mTaxiNodes[0].mId == 5416 && tDetail.mTaxiNodes[1].mName.empty(),
          "taksi dugumleri");
    CHECK(tDetail.mTaxiEdges.size() == 2 && tDetail.mTaxiEdges[0].mWidthClass == 'A' &&
              tDetail.mTaxiEdges[0].mName == "B" && tDetail.mTaxiEdges[0].mActiveZones.size() == 1 &&
              tDetail.mTaxiEdges[1].mGroundVehicles && tDetail.mTaxiEdges[1].mOneWay,
          "taksi kenarlari: genislik sinifi, aktif bolge, yer araci");
    CHECK(tDetail.mStartLocations.size() == 2 && tDetail.mStartLocations[1].mType == "tie_down" &&
              tDetail.mStartLocations[1].mName == "A10" && tDetail.mStartLocations[1].mAirlines == "dal",
          "kalkis noktalari (15 + 1300/1301)");
    CHECK(tDetail.mMetadata.size() == 1 && tDetail.mTruckParkings.size() == 1 &&
              tDetail.mTruckParkings[0].mCount == 3 &&
              tDetail.mTruckDestinations.size() == 1 &&
              tDetail.mTruckDestinations[0].mName == "Luggage Train Destination South 2",
          "ust bilgi, servis araci park / hedef");
}

void TestAirportShapes()
{
    const LocalFrame tFrame({47.0, 8.0});
    const LocalPoint tEast = tFrame.ToLocal({47.0, 8.001});
    CHECK(Near(tEast.mX, 111320.0 * std::cos(47.0 * 3.14159265358979 / 180.0) * 0.001, 1e-6) &&
              Near(tEast.mY, 0.0, 1e-9),
          "yerel duzlem: dogu +x");
    const LatLon tBack = tFrame.ToLatLon(tEast);
    CHECK(Near(tBack.mLon, 8.001, 1e-12) && Near(tBack.mLat, 47.0, 1e-12), "yerel duzlemden geri");

    // Duz parca: sadece baslangic noktasi
    AptNode tA;
    tA.mPosition = {47.0, 8.0};
    AptNode tB;
    tB.mPosition = {47.0, 8.001};
    std::vector<LocalPoint> tPoints;
    Shapes::AddSegment(tA, tB, tFrame, 2.0, tPoints);
    CHECK(tPoints.size() == 1, "kontrolsuz parca duz");

    // Egri: kural "kontrol noktasi cikan egriye ait, gelende aynasi"
    tA.mHasControl = true;
    tA.mControl = {47.0003, 8.0};  // A'dan kuzeye cik
    tB.mHasControl = true;
    tB.mControl = {46.9997, 8.001};  // B'ye kuzeyden gel: aynasi (47.0003) kullanilir
    tPoints.clear();
    Shapes::AddSegment(tA, tB, tFrame, 2.0, tPoints);
    CHECK(tPoints.size() > 10, "egri parcalara bolundu: %d", static_cast<int>(tPoints.size()));
    const LocalPoint tMiddle = tPoints[tPoints.size() / 2];
    CHECK(tMiddle.mY > 20.0, "egri kuzeye siskin (ayna kurali): y = %.1f m", tMiddle.mY);

    // Kare (100 x 100 m) icinde kare delik (40 x 40 m)
    const std::vector<LocalPoint> tOuter = {{0, 0}, {100, 0}, {100, 100}, {0, 100}};
    const std::vector<LocalPoint> tHole = {{30, 30}, {30, 70}, {70, 70}, {70, 30}};
    std::vector<LocalPoint> tVertices;
    std::vector<int> tTriangles;
    auto tTriangleArea = [&]()
    {
        double tArea = 0.0;
        for (size_t tIndex = 0; tIndex + 2 < tTriangles.size(); tIndex += 3)
        {
            tArea += std::fabs(Shapes::SignedArea(
                {tVertices[tTriangles[tIndex]], tVertices[tTriangles[tIndex + 1]], tVertices[tTriangles[tIndex + 2]]}));
        }
        return tArea;
    };
    CHECK(Shapes::Triangulate(tOuter, {tHole}, tVertices, tTriangles), "delikli kare ucgenlenir");
    CHECK(Near(tTriangleArea(), 10000.0 - 1600.0, 1e-6), "alan = dis - delik: %.1f", tTriangleArea());

    // Ters yonlu girdi ve iki delik
    std::vector<LocalPoint> tReversed(tOuter.rbegin(), tOuter.rend());
    const std::vector<LocalPoint> tHole2 = {{5, 5}, {5, 15}, {15, 15}, {15, 5}};
    CHECK(Shapes::Triangulate(tReversed, {tHole, tHole2}, tVertices, tTriangles) &&
              Near(tTriangleArea(), 10000.0 - 1600.0 - 100.0, 1e-6),
          "ters yon + iki delik: %.1f", tTriangleArea());

    // Icbukey (L) sekil, tekrar eden nokta ve dogrusal kose
    const std::vector<LocalPoint> tL = {{0, 0}, {50, 0}, {100, 0}, {100, 40}, {40, 40}, {40, 100}, {40, 100}, {0, 100}};
    CHECK(Shapes::Triangulate(tL, {}, tVertices, tTriangles) && Near(tTriangleArea(), 100.0 * 40.0 + 40.0 * 60.0, 1e-6),
          "L sekli: %.1f", tTriangleArea());

    CHECK(!Shapes::Triangulate({{0, 0}, {10, 0}}, {}, tVertices, tTriangles), "iki nokta ucgenlenmez");

    // GERCEK VERI: kendini kesen sinir (kelebek) cokmeden ucgenlenir
    const std::vector<LocalPoint> tBowtie = {{0, 0}, {100, 100}, {100, 0}, {0, 100}};
    CHECK(Shapes::Triangulate(tBowtie, {}, tVertices, tTriangles) && tTriangles.size() % 3 == 0,
          "kendini kesen sinir: %d ucgen", static_cast<int>(tTriangles.size() / 3));

    // Buyuk poligon (Z-duzeni yolu, > 80 nokta): 400 kenarli daire icinde kare delik
    std::vector<LocalPoint> tCircle;
    for (int tStep = 0; tStep < 400; ++tStep)
    {
        const double tAngle = 2.0 * 3.14159265358979 * tStep / 400.0;
        tCircle.push_back({500.0 * std::cos(tAngle), 500.0 * std::sin(tAngle)});
    }
    const std::vector<LocalPoint> tSquareHole = {{-50, -50}, {-50, 50}, {50, 50}, {50, -50}};
    CHECK(Shapes::Triangulate(tCircle, {tSquareHole}, tVertices, tTriangles) &&
              Near(tTriangleArea(), std::fabs(Shapes::SignedArea(tCircle)) - 10000.0, 1e-3),
          "buyuk poligon + delik: %.1f", tTriangleArea());

    // Delik dis sinira degiyor (ortak kose)
    const std::vector<LocalPoint> tTouching = {{0, 0}, {0, 40}, {40, 40}, {40, 0}};
    CHECK(Shapes::Triangulate(tOuter, {tTouching}, tVertices, tTriangles) &&
              Near(tTriangleArea(), 10000.0 - 1600.0, 1e-6),
          "koseden degen delik: %.1f", tTriangleArea());

    // Mercator: sekillerin kaydirma + olcek ile cizilmesi lat/lon yoluyla ayni yere duser
    MapView tView;
    RectD tViewport;
    tViewport.mLeft = 0;
    tViewport.mBottom = 0;
    tViewport.mRight = 1000;
    tViewport.mTop = 800;
    tView.SetViewport(tViewport);
    tView.SetCenter({47.45, -122.31});
    tView.SetZoom(15.0);
    const LatLon tReference = {47.449888889, -122.311777778};
    const LatLon tCorner = {47.4600, -122.3185};
    const double tMercX = Mercator::LonToX(tCorner.mLon) - Mercator::LonToX(tReference.mLon);
    const double tMercY = Mercator::LatToY(tCorner.mLat) - Mercator::LatToY(tReference.mLat);
    const PointF tOffset = tView.MercatorToScreen(Mercator::LonToX(tReference.mLon), Mercator::LatToY(tReference.mLat));
    const PointF tDirect = tView.ToScreen(tCorner);
    const double tGpuX = tOffset.mX + static_cast<float>(tMercX) * tView.WorldSize();
    const double tGpuY = tOffset.mY + static_cast<float>(tMercY) * tView.WorldSize();
    CHECK(Near(tGpuX, tDirect.mX, 0.05) && Near(tGpuY, tDirect.mY, 0.05),
          "GPU donusumu = ToScreen (float fark): %.3f %.3f / %.3f %.3f", tGpuX, tGpuY, tDirect.mX, tDirect.mY);

    // Ayrintidan sekiller: yuzey + kenar + cizgi
    const AirportDetail tDetail = AptDatParser::ParseAirport("1 0 0 0 TEST Test\n"
                                                             "110 1 0.25 0 Apron\n"
                                                             "111 47.000 8.000 1\n"
                                                             "111 47.000 8.002\n"
                                                             "111 47.001 8.002\n"
                                                             "113 47.001 8.000\n"
                                                             "110 3 0.25 0 Grass\n"
                                                             "111 47.002 8.000\n"
                                                             "111 47.002 8.001\n"
                                                             "113 47.003 8.001\n");
    const AirportShapes tShapes = Shapes::Build(tDetail, {47.0, 8.0});
    CHECK(tShapes.mPavements.size() == 2 && tShapes.mFailedPolygons == 0 && tShapes.mPavements[0].mSurfaceCode == 3,
          "iki yuzey; cizim sirasi dosyanin tersi (once cim)");
    CHECK(tShapes.mPavementEdges.size() == 2 && tShapes.mLines.size() == 1 && tShapes.mLines[0].mLineTypes[0] == 1 &&
              tShapes.mLines[0].mLineTypes[1] == 0,
          "kenar cizgisi stili ilk parcada");
}

class FakeAirportSource : public IAirportGeometrySource
{
public:
    const char* Name() const override
    {
        return "sahte kaynak";
    }

    bool LoadAll(const std::string& pXPlaneDirectory, const std::atomic<bool>& /*pCancel*/,
                 AirportIndex& pOut, std::string& pOutReport) override
    {
        AirportGeometry tAirport;
        tAirport.mId = "LTAC";
        Runway tRunway;
        tRunway.mWidthM = 60.0;
        tRunway.mEnds[0].mPosition = {40.11, 32.98};
        tRunway.mEnds[1].mPosition = {40.14, 33.01};
        tAirport.mRunways.push_back(tRunway);
        pOut.mAirports.push_back(tAirport);
        tAirport.mId = "KSEA";
        tAirport.mRunways[0].mEnds[0].mPosition = {47.46, -122.31};
        tAirport.mRunways[0].mEnds[1].mPosition = {47.43, -122.31};
        pOut.mAirports.push_back(tAirport);
        pOutReport = "klasor: " + pXPlaneDirectory;
        return true;
    }
};

void TestAirportGeometryStore()
{
    AirportGeometryStore tStore(std::make_unique<FakeAirportSource>());
    CHECK(!tStore.IsReady() && !tStore.IsLoading(), "baslangicta bos");
    tStore.StartLoading("C:\\X-Plane 11\\");
    CHECK(tStore.IsLoading(), "yukleniyor");

    bool tDone = false;
    for (int tTry = 0; tTry < 500 && !tDone; ++tTry)
    {
        tDone = tStore.Poll();
        if (!tDone)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
    CHECK(tDone && tStore.IsReady() && !tStore.IsLoading() && tStore.LastLoadSucceeded(), "yukleme bitti");
    CHECK(!tStore.Poll(), "Poll bitisi bir kez bildirir");
    CHECK(tStore.Count() == 2 && tStore.RunwayCount() == 2, "2 havalimani, 2 pist");
    CHECK(tStore.LastReport() == "klasor: C:\\X-Plane 11\\", "rapor ana thread'e gelir");
    CHECK(tStore.Find("KSEA") && tStore.Find("KSEA")->mId == "KSEA" && !tStore.Find("YOK1"), "kimlikle bulma");

    GeoBounds tAnkara;
    tAnkara.mSouth = 39.5;
    tAnkara.mNorth = 40.5;
    tAnkara.mWest = 32.5;
    tAnkara.mEast = 33.5;
    std::vector<int> tResult;
    tStore.Query(tAnkara, tResult);
    CHECK(tResult.size() == 1 && tStore.At(tResult[0]).mId == "LTAC", "Ankara sorgusu");
}

void TestDrawingsFile()
{
    Drawings tModel;
    tModel.SetMode(Drawings::Mode::Point);
    tModel.AddVertex({41.123456789, 29.987654321});
    tModel.SetMode(Drawings::Mode::Route);
    tModel.AddVertex({41.0, 29.0});
    tModel.AddVertex({40.5, 30.5});
    tModel.AddVertex({40.0, 32.0});
    tModel.Finish();
    tModel.SetMode(Drawings::Mode::Region);
    tModel.AddVertex({0.0, 0.0});
    tModel.AddVertex({0.0, 1.0});
    tModel.AddVertex({1.0, 1.0});
    tModel.AddVertex({1.0, 0.0});
    tModel.Finish();
    tModel.AddVertex({5.0, 5.0});  // yarim kalan bolge: kaydedilmez
    CHECK(Drawings::LabelOf(tModel.Shapes()[0]) == "P1" && Drawings::LabelOf(tModel.Shapes()[1]) == "R1" &&
              Drawings::LabelOf(tModel.Shapes()[2]) == "A1",
          "etiketler P1 R1 A1");

    const std::string tText = tModel.Serialize();
    Drawings tLoaded;
    CHECK(tLoaded.Deserialize(tText) == 3, "3 sekil geri okunur");
    CHECK(!tLoaded.Current(), "yarim cizim kaydedilmez");
    CHECK(tLoaded.Shapes()[1].mPoints.size() == 3 && Near(tLoaded.Shapes()[1].mPoints[2].mLon, 32.0, 1e-9),
          "rota koseleri");
    CHECK(Near(tLoaded.Shapes()[0].mPoints[0].mLat, 41.123456789, 1e-8), "hassasiyet korunur");
    CHECK(tLoaded.Shapes()[2].mTriangles.size() == 6, "bolge yeniden ucgenlenir");

    // Numaralar devam eder: yeni nokta P2 olmali
    tLoaded.SetMode(Drawings::Mode::Point);
    tLoaded.AddVertex({1.0, 1.0});
    CHECK(tLoaded.Shapes().back().mNumber == 2, "numara kaldigi yerden devam eder");

    CHECK(tLoaded.Deserialize("# yorum\nX 1 2 3\nP 0 1 1\nR 4 1 1\nP 7 41 29\n") == 1 &&
              tLoaded.Shapes()[0].mNumber == 7,
          "bozuk satirlar atlanir");

    // Alan: ekvatorda 1x1 derece ~ 60x60 NM; 60 derecede yarisi
    CHECK(Near(PolygonAreaNm2({{0, 0}, {0, 1}, {1, 1}, {1, 0}}), 3600.0, 1.0), "ekvator alani %.1f",
          PolygonAreaNm2({{0, 0}, {0, 1}, {1, 1}, {1, 0}}));
    CHECK(Near(PolygonAreaNm2({{59.5, 0}, {59.5, 1}, {60.5, 1}, {60.5, 0}}), 1800.0, 5.0), "60 derece alani");
    CHECK(PolygonAreaNm2({{0, 0}, {0, 1}}) == 0.0, "iki nokta alansiz");
}

void TestAirways()
{
    // Gercek earth_awy.dat satiri
    AirwayLine tLine;
    CHECK(AirwayDatabase::ParseLine("ABKAL LT 11 ULSAB LT 11 N 1 195 285 G8-L614", &tLine), "satir okunur");
    CHECK(tLine.mFromId == "ABKAL" && tLine.mFromType == 11 && tLine.mToId == "ULSAB" && tLine.mDirection == 'N' &&
              tLine.mLevel == 1 && tLine.mBaseFl == 195 && tLine.mTopFl == 285 && tLine.mNames == "G8-L614",
          "alanlar");
    CHECK(AirwayDatabase::ParseLine(" ZZV K5  3 SABGE K5 11 F 2  30 179 V214", &tLine) && tLine.mFromType == 3 &&
              tLine.mDirection == 'F' && tLine.mLevel == 2,
          "bosluklu satir, VOR ucu, tek yon");
    CHECK(!AirwayDatabase::ParseLine("99", &tLine) && !AirwayDatabase::ParseLine("I", &tLine) &&
              !AirwayDatabase::ParseLine("1100 Version - data cycle 1802", &tLine),
          "baslik ve son satirlar atlanir");

    // Uc nokta cozme: ayni kimlikte iki fix var; diger uca yakin olan secilmeli
    NavDatabase tNav;
    tNav.Add(MakeRecord(NavKind::Fix, "ABKAL", 38.0f, 41.5f));
    tNav.Add(MakeRecord(NavKind::Fix, "ULSAB", 38.5f, 42.0f));
    tNav.Add(MakeRecord(NavKind::Fix, "ULSAB", -30.0f, 150.0f));  // ayni ad, dunyanin obur ucu
    tNav.Add(MakeRecord(NavKind::Vor, "ZZV", 40.0f, -82.0f));
    tNav.Add(MakeRecord(NavKind::Fix, "SABGE", 40.5f, -81.0f));
    tNav.Build();

    const char* tPath = "map_test_airways.dat";
    FILE* tFile = std::fopen(tPath, "w");
    CHECK(tFile != nullptr, "test dosyasi");
    if (tFile)
    {
        std::fputs("I\n1100 Version - test\n\n"
                   "ABKAL LT 11 ULSAB LT 11 N 1 195 285 G8-L614\n"
                   " ZZV K5  3 SABGE K5 11 F 2  30 179 V214\n"
                   "NOSUCH LT 11 ABKAL LT 11 N 1 0 0 X1\n"
                   "99\n",
                   tFile);
        std::fclose(tFile);
    }
    AirwayDatabase tAirways;
    tAirways.StartLoading({"no_such_awy.dat", tPath}, tNav);
    for (int tTry = 0; tTry < 500 && !tAirways.Poll(); ++tTry)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    std::remove(tPath);
    CHECK(tAirways.IsReady() && tAirways.Count() == 2, "2 segment cozuldu: %d", tAirways.Count());
    CHECK(tAirways.LastReport().find("1 unresolved") != std::string::npos, "rapor: '%s'",
          tAirways.LastReport().c_str());
    if (tAirways.Count() == 2)
    {
        CHECK(Near(tAirways.At(0).mTo.mLat, 38.5, 1e-5) && !tAirways.At(0).mHigh, "yakin ULSAB secildi, alcak");
        CHECK(tAirways.At(1).mHigh && tAirways.At(1).mDirection == 'F', "yuksek, tek yon");
    }

    // Sorgu: segmentin ortasindan gecen bir alan (uclar disarida) yine bulur
    GeoBounds tMiddle;
    tMiddle.mSouth = 40.1;
    tMiddle.mNorth = 40.4;
    tMiddle.mWest = -81.7;
    tMiddle.mEast = -81.3;
    std::vector<int> tResult;
    tAirways.Query(tMiddle, tResult);
    CHECK(Contains(tResult, 1), "uclari ekran disinda olan segment de bulunur");
}

void TestLabelPriority()
{
    std::string tTrace;
    class PriorityLayer : public Layer
    {
    public:
        PriorityLayer(const char* pId, int pPriority, std::string* pTrace)
            : Layer(pId, pId, true)
            , mPriority(pPriority)
            , mTrace(pTrace)
        {
        }
        void Draw(const DrawContext& /*pContext*/) override
        {
            *mTrace += "D" + Id() + " ";
        }
        void DrawLabels(const DrawContext& /*pContext*/) override
        {
            *mTrace += "L" + Id() + " ";
        }
        int LabelPriority() const override
        {
            return mPriority;
        }

    private:
        int mPriority;
        std::string* mTrace;
    };
    LayerManager tLayers;
    tLayers.Add(std::make_unique<PriorityLayer>("awy", 70, &tTrace));
    tLayers.Add(std::make_unique<PriorityLayer>("apt", 10, &tTrace));
    tLayers.Add(std::make_unique<PriorityLayer>("fix", 60, &tTrace));
    DrawContext tContext;
    tLayers.DrawAll(tContext);
    CHECK(tTrace == "Dawy Dapt Dfix Lapt Lfix Lawy ", "geometri liste sirasi, yazi oncelik sirasi: '%s'",
          tTrace.c_str());
}

// ---- Sentetik DSF (kucuk-endian bayt dizisi) ----

void PutU8(std::vector<uint8_t>& pOut, int pValue)
{
    pOut.push_back(static_cast<uint8_t>(pValue));
}

void PutU16(std::vector<uint8_t>& pOut, int pValue)
{
    pOut.push_back(static_cast<uint8_t>(pValue & 0xFF));
    pOut.push_back(static_cast<uint8_t>((pValue >> 8) & 0xFF));
}

void PutU32(std::vector<uint8_t>& pOut, uint32_t pValue)
{
    for (int tShift = 0; tShift < 32; tShift += 8)
    {
        pOut.push_back(static_cast<uint8_t>((pValue >> tShift) & 0xFF));
    }
}

void PutF32(std::vector<uint8_t>& pOut, float pValue)
{
    uint32_t tBits = 0;
    std::memcpy(&tBits, &pValue, sizeof(tBits));
    PutU32(pOut, tBits);
}

// Atom: kimlik harfleri dosyada ters ("HEAD" -> "DAEH"), boyut basligi icerir
std::vector<uint8_t> Atom(const char* pId, const std::vector<uint8_t>& pPayload)
{
    std::vector<uint8_t> tAtom;
    PutU32(tAtom, (static_cast<uint32_t>(pId[0]) << 24) | (static_cast<uint32_t>(pId[1]) << 16) |
                      (static_cast<uint32_t>(pId[2]) << 8) | static_cast<uint32_t>(pId[3]));
    PutU32(tAtom, static_cast<uint32_t>(pPayload.size() + 8));
    tAtom.insert(tAtom.end(), pPayload.begin(), pPayload.end());
    return tAtom;
}

std::vector<uint8_t> Strings(const std::vector<std::string>& pStrings)
{
    std::vector<uint8_t> tOut;
    for (const std::string& tString : pStrings)
    {
        tOut.insert(tOut.end(), tString.begin(), tString.end());
        tOut.push_back(0);
    }
    return tOut;
}

void Append(std::vector<uint8_t>& pOut, const std::vector<uint8_t>& pMore)
{
    pOut.insert(pOut.end(), pMore.begin(), pMore.end());
}

// 1x1 derece karo (+41+029): sol yari su, sag yari orman (4 nokta, 2 ucgen)
std::vector<uint8_t> MakeTestDsf(bool pOverlay)
{
    std::vector<uint8_t> tProperties = Strings({"sim/west", "29", "sim/east", "30", "sim/south", "41", "sim/north",
                                                "42", "sim/overlay", pOverlay ? "1" : "0"});
    std::vector<uint8_t> tTerrains = Strings({"terrain_Water", "lib/g10/terrain10/coni_tmp_wet_hill.ter"});

    // Havuz: 6 nokta, 5 duzlem (lon, lat, yukseklik, normal x, normal z).
    // Noktalar: (29,41) (29.5,41) (29.5,42) (29,42) (30,41) (30,42)
    std::vector<uint8_t> tPool;
    PutU32(tPool, 6);
    PutU8(tPool, 5);
    // Duzlem 0 (boylam): RLE + delta (kodlama 3): farklar 0, 32767, 0, -32767, 65535, 0
    PutU8(tPool, 3);
    PutU8(tPool, 0x80 | 1);  // 1 kez tekrar: 0
    PutU16(tPool, 0);
    PutU8(tPool, 5);          // 5 deger oldugu gibi
    PutU16(tPool, 32767);     // 32767
    PutU16(tPool, 0);         // 32767
    PutU16(tPool, 32769);     // 32767 + 32769 = 65536 -> 0 (16 bit tasma)
    PutU16(tPool, 65535);     // 65535
    PutU16(tPool, 0);         // 65535
    // Duzlem 1 (enlem): ham (kodlama 0)
    PutU8(tPool, 0);
    for (int tValue : {0, 0, 65535, 65535, 0, 65535})
    {
        PutU16(tPool, tValue);
    }
    // Duzlem 2..4: RLE (kodlama 2): hepsi 0
    for (int tPlane = 2; tPlane < 5; ++tPlane)
    {
        PutU8(tPool, 2);
        PutU8(tPool, 0x80 | 6);
        PutU16(tPool, 0);
    }
    std::vector<uint8_t> tScale;
    PutF32(tScale, 1.0f);
    PutF32(tScale, 29.0f);  // boylam = 29 + ham / 65535
    PutF32(tScale, 1.0f);
    PutF32(tScale, 41.0f);
    for (int tPlane = 2; tPlane < 5; ++tPlane)
    {
        PutF32(tScale, 0.0f);
        PutF32(tScale, 0.0f);
    }

    std::vector<uint8_t> tCommands;
    PutU8(tCommands, 2);  // junction offset (atlanir)
    PutU32(tCommands, 0);
    PutU8(tCommands, 15);  // nested polygon range: param, sayac 2, 3 indeks (sayac + 1)
    PutU16(tCommands, 0);
    PutU8(tCommands, 2);
    PutU16(tCommands, 0);
    PutU16(tCommands, 2);
    PutU16(tCommands, 4);
    PutU8(tCommands, 32);  // yorum
    PutU8(tCommands, 3);
    PutU8(tCommands, 'a');
    PutU8(tCommands, 'b');
    PutU8(tCommands, 'c');
    PutU8(tCommands, 1);  // havuz 0
    PutU16(tCommands, 0);
    PutU8(tCommands, 3);  // tanim 0 (su)
    PutU8(tCommands, 0);
    PutU8(tCommands, 18);  // patch: bayrak 1 (fiziksel), LOD 0..-1
    PutU8(tCommands, 1);
    PutF32(tCommands, 0.0f);
    PutF32(tCommands, -1.0f);
    PutU8(tCommands, 23);  // ucgen: 0 1 2 ve 0 2 3 (sol yari)
    PutU8(tCommands, 6);
    for (int tIndex : {0, 1, 2, 0, 2, 3})
    {
        PutU16(tCommands, tIndex);
    }
    PutU8(tCommands, 3);  // tanim 1 (orman)
    PutU8(tCommands, 1);
    PutU8(tCommands, 16);  // yeni patch (ayni bayrak)
    PutU8(tCommands, 26);  // serit: 1 4 2 5 -> (1,4,2) (4,2,5): sag yari
    PutU8(tCommands, 4);
    for (int tIndex : {1, 4, 2, 5})
    {
        PutU16(tCommands, tIndex);
    }
    PutU8(tCommands, 17);  // overlay patch: ucgen sayilir ama overlay bayragi tasir
    PutU8(tCommands, 3);
    PutU8(tCommands, 25);  // ucgen araligi 0..3
    PutU16(tCommands, 0);
    PutU16(tCommands, 3);

    std::vector<uint8_t> tDefinitions = Atom("TERT", tTerrains);
    std::vector<uint8_t> tGeometry = Atom("POOL", tPool);
    Append(tGeometry, Atom("SCAL", tScale));

    std::vector<uint8_t> tFile = {'X', 'P', 'L', 'N', 'E', 'D', 'S', 'F'};
    PutU32(tFile, 1);
    Append(tFile, Atom("HEAD", Atom("PROP", tProperties)));
    Append(tFile, Atom("DEFN", tDefinitions));
    Append(tFile, Atom("GEOD", tGeometry));
    Append(tFile, Atom("CMDS", tCommands));
    tFile.resize(tFile.size() + 16, 0);  // MD5 yeri (okuyucu dogrulamaz)
    return tFile;
}

void TestDsfReader()
{
    const std::vector<uint8_t> tData = MakeTestDsf(false);
    CHECK(!SevenZip::IsArchive(tData.data(), tData.size()), "duz DSF 7z degil");
    const uint8_t tSevenZip[6] = {'7', 'z', 0xBC, 0xAF, 0x27, 0x1C};
    CHECK(SevenZip::IsArchive(tSevenZip, 6), "7z imzasi");

    DsfReader tReader;
    std::string tError;
    CHECK(tReader.Open(tData, &tError), "DSF acilir: %s", tError.c_str());
    const DsfInfo& tInfo = tReader.Info();
    CHECK(Near(tInfo.mWest, 29, 1e-9) && Near(tInfo.mNorth, 42, 1e-9) && !tInfo.mOverlay, "ozellikler");
    CHECK(tInfo.mTerrainNames.size() == 2 && tInfo.mTerrainNames[0] == "terrain_Water", "arazi adlari");
    CHECK(tReader.PoolCount() == 1, "bir havuz");

    std::vector<DsfTriangle> tTriangles;
    CHECK(tReader.ForEachTriangle(
              [&](const DsfTriangle& pTriangle)
              {
                  tTriangles.push_back(pTriangle);
              },
              &tError),
          "komut akisi okunur: %s", tError.c_str());
    CHECK(tTriangles.size() == 5, "5 ucgen (2 su + 2 serit + 1 overlay): %d", static_cast<int>(tTriangles.size()));
    if (tTriangles.size() == 5)
    {
        CHECK(tTriangles[0].mTerrain == 0 && Near(tTriangles[0].mLon[1], 29.0 + 32767.0 / 65535.0, 1e-9) &&
                  Near(tTriangles[0].mLat[2], 42.0, 1e-9),
              "RLE + delta cozuldu, olcek uygulandi");
        CHECK(tTriangles[2].mTerrain == 1 && Near(tTriangles[2].mLon[1], 30.0, 1e-9), "serit ucgeni sag yarida");
        CHECK((tTriangles[4].mPatchFlags & DsfReader::kPatchFlagOverlay) != 0 &&
                  (tTriangles[0].mPatchFlags & DsfReader::kPatchFlagOverlay) == 0,
              "overlay bayragi");
    }

    // Bozuk akis: bilinmeyen komut
    std::vector<uint8_t> tBroken = tData;
    DsfReader tBrokenReader;
    CHECK(tBrokenReader.Open(tBroken, &tError), "bozuk akisli dosya acilir");
    std::vector<uint8_t> tBadCommands = Atom("CMDS", {200});
    tBroken.resize(tBroken.size() - 16);
    Append(tBroken, tBadCommands);
    tBroken.resize(tBroken.size() + 16, 0);
    CHECK(tBrokenReader.Open(tBroken, &tError), "ikinci CMDS");
    CHECK(!tBrokenReader.ForEachTriangle(
              [](const DsfTriangle&)
              {
              },
              &tError) &&
              tError.find("unknown") != std::string::npos,
          "bilinmeyen komut hata verir: %s", tError.c_str());

    std::vector<uint8_t> tNotDsf = {'N', 'O', 'T'};
    CHECK(!tReader.Open(tNotDsf, &tError), "DSF olmayan reddedilir");
}

void TestTerrain()
{
    // Siniflandirma (gercek XP11 adlari)
    CHECK(ClassifyTerrain("terrain_Water") == TerrainClass::Water, "su");
    CHECK(ClassifyTerrain("lib/g10/terrain10/coni_tmp_wet_hill.ter") == TerrainClass::ForestConifer, "igne yaprak");
    CHECK(ClassifyTerrain("lib/g10/terrain10/urban_tmp_wet_sflat.ter") == TerrainClass::Urban, "sehir");
    CHECK(ClassifyTerrain("lib/g10/terrain10/sq_grass_tmp_wet_fl.ter") == TerrainClass::Grass, "sq_ on eki");
    CHECK(ClassifyTerrain("lib/g10/terrain10/sq_tmp_crptwnhill.ter") == TerrainClass::Crop, "crptwn");
    CHECK(ClassifyTerrain("lib/g10/terrain10/sp_tmp_wet_fl.ter") == TerrainClass::Grass &&
              ClassifyTerrain("lib/g10/terrain10/spr_tmp_wet_fl.ter") == TerrainClass::Grass,
          "sp / spr");
    CHECK(ClassifyTerrain("lib/g10/terrain10/snow_tmp.ter") == TerrainClass::SnowIce, "kar");
    CHECK(ClassifyTerrain("terrain/ortho_+47-123_16.ter") == TerrainClass::Land, "taninmayan: kara");

    // Renk dosyasi gidis-donus
    TerrainPalette tPalette;
    Settings tColors;
    tPalette.Save(tColors);
    tColors.SetString("colors", "water", "10 20 30");
    tColors.SetDouble("shading", "strength", 2.0);  // sinira kirpilmali
    TerrainPalette tLoaded;
    tLoaded.Load(tColors);
    CHECK(tLoaded.ColorOf(TerrainClass::Water).mR == 10 && tLoaded.ColorOf(TerrainClass::Water).mB == 30,
          "renk okunur");
    CHECK(Near(tLoaded.ShadingStrength(), 1.0, 1e-12), "golge gucu 0..1");

    // Sentetik DSF'ten raster: sol yari su, sag yari orman
    const std::vector<uint8_t> tData = MakeTestDsf(false);
    DsfReader tReader;
    std::string tError;
    tReader.Open(tData, &tError);
    TerrainRaster tRaster;
    tRaster.Reset(29, 41);
    tReader.ForEachTriangle(
        [&](const DsfTriangle& pTriangle)
        {
            if ((pTriangle.mPatchFlags & DsfReader::kPatchFlagOverlay) == 0)
            {
                tRaster.FillTriangle(pTriangle.mLon, pTriangle.mLat,
                                     ClassifyTerrain(tReader.Info().mTerrainNames[pTriangle.mTerrain]));
            }
        },
        &tError);
    const int tHalf = TerrainRaster::kSize / 2;
    CHECK(tRaster.ClassAt(100, 100) == TerrainClass::Water && tRaster.ClassAt(tHalf + 100, 100) ==
                                                                   TerrainClass::ForestConifer,
          "sol su, sag orman");
    const int tWater = tRaster.CountOf(TerrainClass::Water);
    const int tTotal = TerrainRaster::kSize * TerrainRaster::kSize;
    CHECK(tWater > tTotal * 0.49 && tWater < tTotal * 0.51, "su yari yariya: %d / %d", tWater, tTotal);
    CHECK(tRaster.CountOf(TerrainClass::None) == 0, "bosluk yok (ucgenler karoyu tam kapliyor)");

    // Goruntu: 64 px, sol ust su rengi, saydam degil
    std::vector<uint8_t> tImage;
    tRaster.BuildImage(0, 0, TerrainRaster::kSize, TerrainRaster::kSize, 64, tPalette, TerrainImageStyle(), &tImage);
    CHECK(tImage.size() == 64 * 64 * 4 && tImage[3] == 255 &&
              tImage[0] == tPalette.ColorOf(TerrainClass::Water).mR,
          "goruntu: su pikseli");

    // Seviye 4: DSF ucgenlerinden boyanan 1/32 derecelik parcalar
    TerrainTriangles tTriangles;
    tTriangles.Reset(29, 41);
    tReader.ForEachTriangle(
        [&](const DsfTriangle& pTriangle)
        {
            if ((pTriangle.mPatchFlags & DsfReader::kPatchFlagOverlay) == 0)
            {
                tTriangles.Add(pTriangle.mLon, pTriangle.mLat,
                               ClassifyTerrain(tReader.Info().mTerrainNames[pTriangle.mTerrain]));
            }
        },
        &tError);
    const int tDetailChunks = TerrainTriangles::kChunksPerSide;
    std::vector<uint8_t> tWaterChunk;
    std::vector<uint8_t> tForestChunk;
    const int tSize = 64;
    tRaster.BuildDetailImage(tTriangles, tDetailChunks, 5 * tDetailChunks + 15, tSize, tPalette, TerrainImageStyle(),
                             &tWaterChunk);
    tRaster.BuildDetailImage(tTriangles, tDetailChunks, 5 * tDetailChunks + 16, tSize, tPalette, TerrainImageStyle(),
                             &tForestChunk);
    bool tAllOpaque = true;
    for (size_t tPixel = 0; tPixel < static_cast<size_t>(tSize) * tSize; ++tPixel)
    {
        if (tWaterChunk[tPixel * 4 + 3] != 255 || tForestChunk[tPixel * 4 + 3] != 255)
        {
            tAllOpaque = false;
            break;
        }
    }
    CHECK(tAllOpaque && tWaterChunk.size() == static_cast<size_t>(tSize) * tSize * 4 &&
              tWaterChunk[0] == tPalette.ColorOf(TerrainClass::Water).mR &&
              tForestChunk[0] == tPalette.ColorOf(TerrainClass::ForestConifer).mR,
          "seviye 4 olculeri");
    TerrainImageKeyHash tHash;
    CHECK(tHash({29, 41, 4, 17, 0}) != tHash({29, 41, 3, 33, 0}) &&
              tHash({29, 41, 4, 1023, 3}) != tHash({29, 41, 4, 1023, 2}) &&
              tHash({29, 41, 5, 16383, 0}) != tHash({29, 41, 5, 16382, 0}) &&
              tHash({29, 41, 5, 16383, 0}) != tHash({29, 42, 5, 16383, 0}),
          "anahtar ozeti carpismaz (seviye 5 dahil)");

    // Seviye 5: 4 kat ince parca. Sol ust ince parca (satir 20, sutun 60) seviye 4'teki
    // (satir 5, sutun 15) parcanin sol ust ceyregi: ikisinde de ayni yer ayni renk.
    CHECK(TerrainStore::ChunksPerSide(TerrainStore::kFineLevel) == 4 * TerrainTriangles::kChunksPerSide &&
              TerrainStore::ImageSize(TerrainStore::kFineLevel) == 1024,
          "seviye 5 olculeri");
    std::vector<uint8_t> tFineChunk;
    const int tFineChunks = TerrainStore::ChunksPerSide(TerrainStore::kFineLevel);
    tRaster.BuildDetailImage(tTriangles, tFineChunks, 20 * tFineChunks + 60, tSize, tPalette, TerrainImageStyle(),
                             &tFineChunk);
    CHECK(tFineChunk.size() == tWaterChunk.size() && tFineChunk[3] == 255 && tFineChunk[0] == tWaterChunk[0] &&
              tFineChunk[2] == tWaterChunk[2],
          "ince parca ust parcanin ceyregiyle ayni");

    // Saydam piksel rengi komsudan dolar, alfa 0 kalir
    std::vector<uint8_t> tBleed = {0, 0, 0, 0, 10, 20, 30, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    TerrainRaster::BleedTransparent(3, &tBleed);
    CHECK(tBleed[0] == 10 && tBleed[3] == 0 && tBleed[8] == 10 && tBleed[2 * 12 + 4 + 1] == 20 &&
              tBleed[2 * 12 + 4 + 3] == 0 && tBleed[4 + 3] == 255,
          "saydam piksel rengi komsudan");

    // Onbellek: ayni anahtarla okunur, farkli anahtarla reddedilir
    const std::string tPath = "map_test_tile.terrain";
    CHECK(tRaster.Save(tPath, "key-1"), "onbellege yazilir");
    TerrainRaster tCached;
    CHECK(tCached.Load(tPath, "key-1") && tCached.ClassAt(tHalf + 100, 100) == TerrainClass::ForestConifer &&
              tCached.West() == 29 && tCached.South() == 41,
          "onbellekten okunur");
    CHECK(!tCached.Load(tPath, "key-2"), "anahtar degisti: yeniden uretilmeli");
    std::remove(tPath.c_str());
}

void TestLandBlend()
{
    // --- 1) Plan: erim metreden piksele cevrilir; kucukse bos plan (filtre kapali) ---
    LandBlendPlan tPlan = PlanLandBlend(0.0, 1.0, 1.0, 24.0);
    CHECK(tPlan.mPad == 0, "plan: blend 0 -> bos");
    tPlan = PlanLandBlend(150.0, 108.0, 108.0, 24.0);
    CHECK(tPlan.mPad == 0, "seviye 2 (~108 m/px): erim 1,5 pikselden kucuk, plan bos");
    tPlan = PlanLandBlend(150.0, 27.0, 27.0, 24.0);
    CHECK(tPlan.mPad > 0 && tPlan.mFactor == 1 && tPlan.mPad >= 6, "seviye 3 (~27 m/px): plan dolu, carpan 1");
    tPlan = PlanLandBlend(11.0, 1.0, 1.6, 24.0);
    CHECK(tPlan.mFactor > 1 && tPlan.mHalfX > tPlan.mHalfY,
          "dogu-bati pikseli kucuk -> hucre buyur, yari genislik dogu-batida buyuk");
    tPlan = PlanLandBlend(150.0, 3.4, 3.4, 24.0);
    CHECK(tPlan.mPad > 0 && tPlan.mPad <= 24 + 3, "tavan 24: erim 24 pikselle sinirli (pay = erim + izgara carpani)");
    // 0.16: [blend] max_pixels = 5 -> yakin seviyede erim kenar yumusatma duzeyinde
    tPlan = PlanLandBlend(150.0, 3.4, 3.4, 5.0);
    CHECK(tPlan.mPad > 0 && tPlan.mPad <= 5 + 2, "max_pixels 5: erim 5 pikselle sinirli");
    tPlan = PlanLandBlend(150.0, 0.58, 0.85, 5.0);
    CHECK(tPlan.mPad > 0 && tPlan.mPad <= 5 + 2, "seviye 5 olceginde de erim 5 pikselle sinirli");
    tPlan = PlanLandBlend(150.0, 0.58, 0.85, 0.0);
    CHECK(tPlan.mPad > 5 + 2, "max_pixels 0 = tavan yok (metre degeri devrede)");

    // --- 2) Duz renk korunur (normalize evrisim karartmaz) ---
    LandColorField tField;
    tField.Reset(64, 1);
    for (int tRow = 0; tRow < 64; ++tRow)
    {
        for (int tColumn = 0; tColumn < 64; ++tColumn)
        {
            tField.Add(tColumn, tRow, {100, 150, 200}, 1.0f);
        }
    }
    tField.Blur(4, 4);
    double tRgb[3] = {0.0, 0.0, 0.0};
    CHECK(tField.ColorAt(32, 32, tRgb) && Near(tRgb[0], 100.0, 0.5) && Near(tRgb[1], 150.0, 0.5) &&
              Near(tRgb[2], 200.0, 0.5),
          "duz renk: merkez korunur");
    CHECK(tField.ColorAt(0, 0, tRgb) && Near(tRgb[0], 100.0, 0.5) && Near(tRgb[1], 150.0, 0.5) &&
              Near(tRgb[2], 200.0, 0.5),
          "duz renk: kose de korunur (pencere kirpilse de oran ayni)");

    // --- 3) Iki renk arasinda gecis olusur ---
    tField.Reset(64, 1);
    for (int tRow = 0; tRow < 64; ++tRow)
    {
        for (int tColumn = 0; tColumn < 64; ++tColumn)
        {
            tField.Add(tColumn, tRow, tColumn < 32 ? TerrainColor{40, 80, 40} : TerrainColor{120, 110, 70}, 1.0f);
        }
    }
    tField.Blur(3, 3);
    CHECK(tField.ColorAt(32, 32, tRgb) && tRgb[1] > 81.0 && tRgb[1] < 109.0,
          "sinir pikseli iki rengin arasinda (G 80..110, kesin uclarda degil)");
    CHECK(tField.ColorAt(52, 32, tRgb) && Near(tRgb[0], 120.0, 1.0) && Near(tRgb[1], 110.0, 1.0) &&
              Near(tRgb[2], 70.0, 1.0),
          "sinirdan 20 px uzak: kendi rengi");

    // --- 4) Veri yok: hanya eklenen yerden uzakta renk yok, erim icinde sol rengin aynisi ---
    tField.Reset(64, 1);
    for (int tRow = 0; tRow < 64; ++tRow)
    {
        for (int tColumn = 0; tColumn < 32; ++tColumn)
        {
            tField.Add(tColumn, tRow, {40, 80, 40}, 1.0f);
        }
    }
    tField.Blur(3, 3);
    CHECK(!tField.ColorAt(63, 32, tRgb), "erimin disinda veri yok");
    CHECK(tField.ColorAt(36, 32, tRgb) && Near(tRgb[0], 40.0, 1.0) && Near(tRgb[1], 80.0, 1.0) &&
              Near(tRgb[2], 40.0, 1.0),
          "erimin icinde: sol rengin aynisi (normalize, karartmaz)");

    // --- 8) Palet surumu: eski dosya renkleri atilir, blend sinirlanir ---
    TerrainPalette tVersionPalette;
    Settings tVersionSettings;
    tVersionSettings.SetString("colors", "forest_conifer", "1 2 3");
    tVersionPalette.Load(tVersionSettings);
    CHECK(!tVersionPalette.LoadedCurrentVersion() &&
              tVersionPalette.ColorOf(TerrainClass::ForestConifer).mR == 52 &&
              tVersionPalette.ColorOf(TerrainClass::ForestConifer).mB == 58,
          "eski surum: dosyadaki renk atilir, yeni varsayilan kalir");
    tVersionSettings.SetInt("palette", "version", 2);
    tVersionPalette.Load(tVersionSettings);
    CHECK(tVersionPalette.LoadedCurrentVersion() &&
              tVersionPalette.ColorOf(TerrainClass::ForestConifer).mR == 1 &&
              tVersionPalette.ColorOf(TerrainClass::ForestConifer).mB == 3,
          "guncel surum: dosyadaki renk okunur");
    tVersionSettings.SetDouble("blend", "meters", 5000.0);
    tVersionPalette.Load(tVersionSettings);
    CHECK(Near(tVersionPalette.BlendMeters(), 1000.0, 1e-9), "blend 1000 metreyle sinirli");
    tVersionPalette.SetBlendMeters(-5.0);
    CHECK(tVersionPalette.BlendMeters() == 0.0, "SetBlendMeters negatif -> 0");
    // 0.16 ayarlari: okunur, sinirlanir, Save/Load tur dondurur
    tVersionSettings.SetDouble("blend", "max_pixels", 500.0);
    tVersionSettings.SetDouble("warp", "meters", 9999.0);
    tVersionSettings.SetDouble("warp", "wavelength", 1.0);
    tVersionSettings.SetDouble("closeup", "class_mix", 5.0);
    tVersionPalette.Load(tVersionSettings);
    CHECK(Near(tVersionPalette.BlendMaxPixels(), 64.0, 1e-9), "max_pixels 64 ile sinirli");
    CHECK(Near(tVersionPalette.Warp().mAmplitudeMeters, 300.0, 1e-9), "warp meters 300 ile sinirli");
    CHECK(Near(tVersionPalette.Warp().mWavelengthMeters, 50.0, 1e-9), "warp wavelength 50 ile sinirli");
    CHECK(Near(tVersionPalette.CloseupClassMix(), 0.8, 1e-9), "class_mix 0,8 ile sinirli");
    tVersionPalette.SetBlendMaxPixels(-1.0);
    tVersionPalette.SetWarp({-5.0, 3000.0, 3});
    tVersionPalette.SetCloseupClassMix(2.0);
    CHECK(tVersionPalette.BlendMaxPixels() == 0.0, "SetBlendMaxPixels negatif -> 0");
    CHECK(tVersionPalette.Warp().mAmplitudeMeters == 0.0 && Near(tVersionPalette.Warp().mWavelengthMeters, 2000.0, 1e-9),
          "SetWarp sinirlar");
    CHECK(Near(tVersionPalette.CloseupClassMix(), 0.8, 1e-9), "SetCloseupClassMix sinirlar");
    const TerrainColor tReference = tVersionPalette.ReferenceLandColor();
    CHECK(tReference.mR > 0 && tReference.mG > 0 && tReference.mB > 0, "referans kara tonu hesaplanir");
    Settings tSavedSettings;
    tVersionPalette.Save(tSavedSettings);
    CHECK(tSavedSettings.GetInt("palette", "version", 0) == TerrainPalette::kPaletteVersion,
          "Save palet surumunu yazar");
    CHECK(Near(tSavedSettings.GetDouble("blend", "meters", -1.0), 1000.0, 1e-9),
          "Save blend degerini yazar (son Load ile sinirli 1000)");
    CHECK(Near(tSavedSettings.GetDouble("blend", "max_pixels", -1.0), 0.0, 1e-9) &&
              Near(tSavedSettings.GetDouble("warp", "meters", -1.0), 0.0, 1e-9) &&
              Near(tSavedSettings.GetDouble("warp", "wavelength", -1.0), 2000.0, 1e-9) &&
              Near(tSavedSettings.GetDouble("closeup", "class_mix", -1.0), 0.8, 1e-9),
          "Save max_pixels / warp / closeup degerlerini yazar");

    // --- 5/6/7) Doku yolu: su keskin, sinir yumusak, parca dikisi yok ---
    // Sentetik karo (10,50): 10.499 boylamina kadar grass, oradan doguya shrub (iki KARA sinifi;
    // sinir 10.5'teki parca sinirinin hemen solunda, yani dikis testi sinirin dibinde yapilir)
    TerrainRaster tLandRaster;
    tLandRaster.Reset(10, 50);
    TerrainTriangles tLandTriangles;
    tLandTriangles.Reset(10, 50);
    const double tBoundary = 10.499;
    auto tAddRect = [&](double pWest, double pEast, TerrainClass pClass)
    {
        const double tLonA[3] = {pWest, pEast, pWest};
        const double tLatA[3] = {50.0, 50.0, 51.0};
        const double tLonB[3] = {pEast, pEast, pWest};
        const double tLatB[3] = {50.0, 51.0, 51.0};
        tLandTriangles.Add(tLonA, tLatA, pClass);
        tLandTriangles.Add(tLonB, tLatB, pClass);
    };
    tAddRect(10.0, tBoundary, TerrainClass::Grass);
    tAddRect(tBoundary, 11.0, TerrainClass::Shrub);

    TerrainPalette tBlendPalette;  // varsayilan [blend] 150 metre
    // 0.16 notu: bu testler kesin sinif rengini dogrular; sinir bukmesi ve yakin zoom karisimi kapali
    // (bukme acikken sinir pikselleri sinif degistirebilir; TestTerrainWarp bukmeyi ayri dogrular)
    tBlendPalette.SetWarp({0.0, 250.0, 3});
    tBlendPalette.SetCloseupClassMix(0.0);
    const int tSize = 64;
    const int tChunks = TerrainTriangles::kChunksPerSide;

    // 5) Su keskin kalir: su/orman DSF'inde su pikseli tam su rengi, kiyidaki kara orman renginde
    const std::vector<uint8_t> tData = MakeTestDsf(false);
    DsfReader tReader;
    std::string tError;
    tReader.Open(tData, &tError);
    TerrainRaster tRaster;
    tRaster.Reset(29, 41);
    TerrainTriangles tSeaTriangles;
    tSeaTriangles.Reset(29, 41);
    tReader.ForEachTriangle(
        [&](const DsfTriangle& pTriangle)
        {
            if ((pTriangle.mPatchFlags & DsfReader::kPatchFlagOverlay) == 0)
            {
                const TerrainClass tClass =
                    ClassifyTerrain(tReader.Info().mTerrainNames[pTriangle.mTerrain]);
                tRaster.FillTriangle(pTriangle.mLon, pTriangle.mLat, tClass);
                tSeaTriangles.Add(pTriangle.mLon, pTriangle.mLat, tClass);
            }
        },
        &tError);
    std::vector<uint8_t> tSeaImage;
    tRaster.BuildDetailImage(tSeaTriangles, tChunks, 5 * tChunks + 15, tSize, tBlendPalette, TerrainImageStyle(),
                             &tSeaImage);
    CHECK(tSeaImage.size() == static_cast<size_t>(tSize) * tSize * 4 &&
              tSeaImage[(32 * tSize + 32) * 4 + 0] == tBlendPalette.ColorOf(TerrainClass::Water).mR &&
              tSeaImage[(32 * tSize + 32) * 4 + 3] == 255,
          "su pikseli tam su rengi ve opak");
    std::vector<uint8_t> tCoastImage;
    tRaster.BuildDetailImage(tSeaTriangles, tChunks, 5 * tChunks + 16, tSize, tBlendPalette, TerrainImageStyle(),
                             &tCoastImage);
    const uint8_t* tCoastLand = tCoastImage.data() + (32 * tSize + 0) * 4;  // kiyiya yakin kara pikseli
    CHECK(tCoastLand[3] == 255 &&
              std::abs(static_cast<int>(tCoastLand[0]) -
                       static_cast<int>(tBlendPalette.ColorOf(TerrainClass::ForestConifer).mR)) <= 2 &&
              std::abs(static_cast<int>(tCoastLand[1]) -
                       static_cast<int>(tBlendPalette.ColorOf(TerrainClass::ForestConifer).mG)) <= 2,
          "kiyidaki kara pikseli su rengine kaymaz (orman renginde)");

    // 6) Sinir yumusar: blend 150'de sinir pikseli ara ton, blend 0'da kendi sinif rengi
    std::vector<uint8_t> tSoftImage;
    tLandRaster.BuildDetailImage(tLandTriangles, tChunks, 5 * tChunks + 15, tSize, tBlendPalette, TerrainImageStyle(),
                                 &tSoftImage);
    // Sinir (10.499) parca icinde sutun ~62; sutun 61 sinirin hemen solunda (grass tarafi)
    const uint8_t* tSoftPixel = tSoftImage.data() + (32 * tSize + 61) * 4;
    CHECK(tSoftPixel[3] == 255 && tSoftPixel[1] > 98 + 1 && tSoftPixel[1] < 106 - 1,
          "sinir pikseli ara ton (G iki sinif rengi arasinda)");
    TerrainPalette tSharpPalette;
    tSharpPalette.SetBlendMeters(0.0);  // yumusatma kapali: ayni piksel keskin kendi renginde
    tSharpPalette.SetWarp({0.0, 250.0, 3});  // bukme de kapali: piksel kaymadan kendi sinifinda
    tSharpPalette.SetCloseupClassMix(0.0);
    std::vector<uint8_t> tSharpImage;
    tLandRaster.BuildDetailImage(tLandTriangles, tChunks, 5 * tChunks + 15, tSize, tSharpPalette, TerrainImageStyle(),
                                 &tSharpImage);
    const uint8_t* tSharpPixel = tSharpImage.data() + (32 * tSize + 61) * 4;
    CHECK(tSharpPixel[0] == tSharpPalette.ColorOf(TerrainClass::Grass).mR &&
              tSharpPixel[1] == tSharpPalette.ColorOf(TerrainClass::Grass).mG,
          "blend 0: sinir pikseli keskin kendi sinif renginde");

    // 7) Dikiş yok: sinira yakin parca kenarlarinda (15. parcanin son sutunu, 16. parcanin
    // ilk sutunu) kanal basina fark <= 3; pay olmasaydi yumusatma farki buyurdu
    std::vector<uint8_t> tRightImage;
    tLandRaster.BuildDetailImage(tLandTriangles, tChunks, 5 * tChunks + 16, tSize, tBlendPalette, TerrainImageStyle(),
                                 &tRightImage);
    bool tSeamOk = true;
    for (int tRow = 0; tRow < tSize; ++tRow)
    {
        const uint8_t* tLeftPixel = tSoftImage.data() + (static_cast<size_t>(tRow) * tSize + 63) * 4;
        const uint8_t* tRightPixel = tRightImage.data() + (static_cast<size_t>(tRow) * tSize + 0) * 4;
        for (int tChannel = 0; tChannel < 3; ++tChannel)
        {
            const int tDiff = static_cast<int>(tLeftPixel[tChannel]) - static_cast<int>(tRightPixel[tChannel]);
            if (tDiff < -3 || tDiff > 3)
            {
                tSeamOk = false;
            }
        }
        if (tLeftPixel[3] != 255 || tRightPixel[3] != 255)
        {
            tSeamOk = false;
        }
    }
    CHECK(tSeamOk, "parca sinirinda dikis yok (payli uretim)");
}

void TestTerrainWarp()
{
    // --- 1) Gurultu: deterministik, [-1, 1] icinde, ortalamasi 0'a yakin, yakin noktalar surekli ---
    double tSum = 0.0;
    double tMaxDiff = 0.0;
    uint32_t tState = 12345u;
    for (int tIndex = 0; tIndex < 10000; ++tIndex)
    {
        tState = tState * 1664525u + 1013904223u;  // LCG: test icin yeterli dagilim
        const double tX = (static_cast<double>(tState) / 4294967296.0 - 0.5) * 100.0;
        tState = tState * 1664525u + 1013904223u;
        const double tY = (static_cast<double>(tState) / 4294967296.0 - 0.5) * 100.0;
        const double tValue = TerrainNoise(tX, tY, 7u, 3);
        CHECK(std::abs(tValue) <= 1.0, "gurultu [-1, 1] icinde");
        CHECK(TerrainNoise(tX, tY, 7u, 3) == tValue, "gurultu deterministik (ayni girdi ayni deger)");
        tSum += tValue;
        // Sureklilik: dalga boyunun %1'i kadar yakin iki nokta
        const double tNear = TerrainNoise(tX + 0.01, tY + 0.01, 7u, 3);
        tMaxDiff = std::max(tMaxDiff, std::abs(tNear - tValue));
    }
    CHECK(std::abs(tSum / 10000.0) < 0.05, "gurultunun isaretli ortalamasi 0'a yakin");
    CHECK(tMaxDiff < 0.1, "yakin noktalar arasi fark kucuk (sureklilik)");

    // --- 2) Kayma siniri: |kayma| <= genlik * koku(2); bilesenler genligi asmaz ---
    TerrainWarpParams tParams;  // varsayilan 65 / 250 / 3
    double tMaxLen = 0.0;
    for (int tIndex = 0; tIndex < 10000; ++tIndex)
    {
        const double tLon = -122.0 + tIndex * 0.0001;
        const double tLat = 47.0 + (tIndex % 97) * 0.001;
        double tEast = 0.0;
        double tNorth = 0.0;
        TerrainWarpOffset(tLon, tLat, tParams, &tEast, &tNorth);
        CHECK(std::abs(tEast) <= tParams.mAmplitudeMeters && std::abs(tNorth) <= tParams.mAmplitudeMeters,
              "kayma bilesenleri genlik icinde");
        tMaxLen = std::max(tMaxLen, std::hypot(tEast, tNorth));
    }
    CHECK(tMaxLen <= tParams.mAmplitudeMeters * std::sqrt(2.0) + 1e-9, "kayma uzunlugu genlik * koku(2) icinde");

    // --- 3) Karo sinirinda sureklilik: 1e-9 derece kayma kaymayi degistirmez ---
    double tEa = 0.0;
    double tNa = 0.0;
    double tEb = 0.0;
    double tNb = 0.0;
    TerrainWarpOffset(-122.0, 47.0, tParams, &tEa, &tNa);
    TerrainWarpOffset(-122.0 + 1e-9, 47.0 + 1e-9, tParams, &tEb, &tNb);
    CHECK(std::hypot(tEb - tEa, tNb - tNa) < 1e-3, "karo sinirinda kayma surekli");

    // --- 4) Plan: uzak seviye etkisiz, yakin seviye aktif; meters 0 = kapali ---
    TerrainWarpPlan tPlan = TerrainPlanWarp(tParams, 108.0, 108.0);
    CHECK(!tPlan.mActive, "seviye 2 (~108 m/px): bukme etkisiz");
    tPlan = TerrainPlanWarp(tParams, 27.0, 27.0);
    CHECK(tPlan.mActive && tPlan.mPad >= 3 && tPlan.mStep == 1, "seviye 3 (~27 m/px): bukme aktif");
    tPlan = TerrainPlanWarp(tParams, 0.85, 0.85);
    CHECK(tPlan.mActive && tPlan.mPad >= 77 && tPlan.mStep > 1, "seviye 5: pay buyuk, izgara adimi > 1");
    tParams.mAmplitudeMeters = 0.0;
    CHECK(!TerrainPlanWarp(tParams, 0.85, 0.85).mActive, "warp meters 0 = duz kenarlar");

    // --- 5) Kara-kara kurali (BuildDetailImage): su birebir ayni, kara-kara siniri degisir ---
    TerrainRaster tRaster;
    tRaster.Reset(10, 50);
    TerrainTriangles tTriangles;
    tTriangles.Reset(10, 50);
    auto tAddRect = [&](double pWest, double pEast, TerrainClass pClass)
    {
        const double tLonA[3] = {pWest, pEast, pWest};
        const double tLatA[3] = {50.0, 50.0, 51.0};
        const double tLonB[3] = {pEast, pEast, pWest};
        const double tLatB[3] = {50.0, 51.0, 51.0};
        tTriangles.Add(tLonA, tLatA, pClass);
        tTriangles.Add(tLonB, tLatB, pClass);
    };
    tAddRect(10.0, 10.25, TerrainClass::Water);
    tAddRect(10.25, 10.3, TerrainClass::ForestConifer);
    tAddRect(10.3, 10.499, TerrainClass::Grass);
    tAddRect(10.499, 11.0, TerrainClass::Shrub);
    TerrainPalette tPlainPalette;
    tPlainPalette.SetBlendMeters(0.0);
    tPlainPalette.SetCloseupClassMix(0.0);
    tPlainPalette.SetWarp({0.0, 250.0, 3});  // karsilastirmanin "kapali" tarafi: duz kenarlar
    const int tChunks = TerrainTriangles::kChunksPerSide;
    const int tSize = 64;
    TerrainPalette tWarpPalette = tPlainPalette;  // bukme acik: varsayilan 65 m geri verilir
    tWarpPalette.SetWarp({65.0, 250.0, 3});
    // 5a) Su: parca 7 (10.21875..10.25) tamamen su; bukme acik/kapali goruntuler birebir ayni
    std::vector<uint8_t> tOffImage;
    std::vector<uint8_t> tOnImage;
    tRaster.BuildDetailImage(tTriangles, tChunks, 10 * tChunks + 7, tSize, tPlainPalette, TerrainImageStyle(),
                             &tOffImage);
    tRaster.BuildDetailImage(tTriangles, tChunks, 10 * tChunks + 7, tSize, tWarpPalette, TerrainImageStyle(), &tOnImage);
    int tWaterOff = 0;
    int tWaterOn = 0;
    bool tWaterSame = true;
    for (int tPixel = 0; tPixel < tSize * tSize; ++tPixel)
    {
        const uint8_t* tA = tOffImage.data() + static_cast<size_t>(tPixel) * 4;
        const uint8_t* tB = tOnImage.data() + static_cast<size_t>(tPixel) * 4;
        const bool tWaterA = tA[3] == 255 && tA[0] == tPlainPalette.ColorOf(TerrainClass::Water).mR &&
                             tA[1] == tPlainPalette.ColorOf(TerrainClass::Water).mG;
        const bool tWaterB = tB[3] == 255 && tB[0] == tPlainPalette.ColorOf(TerrainClass::Water).mR &&
                             tB[1] == tPlainPalette.ColorOf(TerrainClass::Water).mG;
        if (tWaterA)
        {
            ++tWaterOff;
        }
        if (tWaterB)
        {
            ++tWaterOn;
        }
        if (tA[0] != tB[0] || tA[1] != tB[1] || tA[2] != tB[2] || tA[3] != tB[3])
        {
            tWaterSame = false;
        }
    }
    CHECK(tWaterOff > 0 && tWaterOff == tWaterOn, "su piksel sayisi degismedi (kiyi aynen)");
    CHECK(tWaterSame, "su parcasindaki her piksel bukmeyle birebir ayni");
    // 5b) Kara-kara siniri: parca 9'da orman/cimen siniri (10.3) bukmeyle kayar
    tRaster.BuildDetailImage(tTriangles, tChunks, 10 * tChunks + 9, tSize, tPlainPalette, TerrainImageStyle(),
                             &tOffImage);
    tRaster.BuildDetailImage(tTriangles, tChunks, 10 * tChunks + 9, tSize, tWarpPalette, TerrainImageStyle(), &tOnImage);
    int tLandChanged = 0;
    for (int tPixel = 0; tPixel < tSize * tSize; ++tPixel)
    {
        const uint8_t* tA = tOffImage.data() + static_cast<size_t>(tPixel) * 4;
        const uint8_t* tB = tOnImage.data() + static_cast<size_t>(tPixel) * 4;
        if (tA[0] != tB[0] || tA[1] != tB[1] || tA[2] != tB[2])
        {
            ++tLandChanged;
        }
    }
    CHECK(tLandChanged > 0, "bukme acikken bazi kara pikselleri sinif degistirdi (%d piksel)", tLandChanged);

    // --- 6) Dikis yok: bukme acikken parca kenarinda kanal basina fark <= 3.
    // Ornek sinir (10.3) dikişin (10.3125) ~26 piksel onunde: gecis dikiÅŸten once biter
    tWarpPalette.SetBlendMeters(150.0);  // blur + bukme birlikte: toplam pay ile uretim
    tWarpPalette.SetCloseupClassMix(0.3);
    std::vector<uint8_t> tLeftImage;
    std::vector<uint8_t> tRightImage;
    tRaster.BuildDetailImage(tTriangles, tChunks, 10 * tChunks + 9, tSize, tWarpPalette, TerrainImageStyle(),
                             &tLeftImage);
    tRaster.BuildDetailImage(tTriangles, tChunks, 10 * tChunks + 10, tSize, tWarpPalette, TerrainImageStyle(),
                             &tRightImage);
    bool tSeamOk = true;
    for (int tRow = 0; tRow < tSize; ++tRow)
    {
        const uint8_t* tLeftPixel = tLeftImage.data() + (static_cast<size_t>(tRow) * tSize + 63) * 4;
        const uint8_t* tRightPixel = tRightImage.data() + (static_cast<size_t>(tRow) * tSize + 0) * 4;
        for (int tChannel = 0; tChannel < 4; ++tChannel)
        {
            const int tDiff = static_cast<int>(tLeftPixel[tChannel]) - static_cast<int>(tRightPixel[tChannel]);
            if (tDiff < -3 || tDiff > 3)
            {
                tSeamOk = false;
            }
        }
    }
    CHECK(tSeamOk, "bukme acikken parca sinirinda dikis yok");

    // --- 7) Sinif karisimi: orana gore referans kara tona yaklasir; su ve kar degismez ---
    CHECK(Near(tWarpPalette.CloseupMixAt(100.0), 0.0, 1e-9), "piksel 50 m'den buyuk: karisim 0");
    const double tMid = tWarpPalette.CloseupMixAt(27.0);
    CHECK(tMid > 0.0 && tMid < 0.15, "piksel 27 m: karisim dusuk");
    CHECK(Near(tWarpPalette.CloseupMixAt(3.0), tWarpPalette.CloseupClassMix(), 1e-9), "piksel 3 m: tam oran");
    const auto tMixed = tWarpPalette.MixedColors(0.3);
    CHECK(tMixed[static_cast<int>(TerrainClass::Water)].mR == tWarpPalette.ColorOf(TerrainClass::Water).mR &&
              tMixed[static_cast<int>(TerrainClass::SnowIce)].mG == tWarpPalette.ColorOf(TerrainClass::SnowIce).mG,
          "su ve kar karismaz");
    const TerrainColor tRef = tWarpPalette.ReferenceLandColor();
    const TerrainColor tForest = tWarpPalette.ColorOf(TerrainClass::ForestConifer);
    const TerrainColor tMixedForest = tMixed[static_cast<int>(TerrainClass::ForestConifer)];
    CHECK(tMixedForest.mR > std::min(tForest.mR, tRef.mR) - 1 &&
              tMixedForest.mR < std::max(tForest.mR, tRef.mR) + 1 &&
              tMixedForest.mG > std::min(tForest.mG, tRef.mG) - 1 &&
              tMixedForest.mG < std::max(tForest.mG, tRef.mG) + 1,
          "orman rengi referansa yaklasmis (kanallar arada)");
    const auto tUnmixed = tWarpPalette.MixedColors(0.0);
    CHECK(tUnmixed[static_cast<int>(TerrainClass::ForestConifer)].mR == tForest.mR, "karisim 0: paletle ayni");
}

void TestSceneryIndex()
{
    int tWest = 0;
    int tSouth = 0;
    CHECK(SceneryIndex::ParseTileName("+47-123.dsf", &tWest, &tSouth) && tWest == -123 && tSouth == 47, "karo adi");
    CHECK(SceneryIndex::ParseTileName("-05+000.DSF", &tWest, &tSouth) && tWest == 0 && tSouth == -5, "buyuk harf");
    CHECK(!SceneryIndex::ParseTileName("+47-123.txt", &tWest, &tSouth) &&
              !SceneryIndex::ParseTileName("47-123.dsf", &tWest, &tSouth),
          "gecersiz adlar");
    CHECK(SceneryIndex::TileName(-123, 47) == "+47-123" && SceneryIndex::FolderName(-123, 47) == "+40-130" &&
              SceneryIndex::FolderName(5, -5) == "-10+000",
          "ad uretimi");

    // Gecici klasorlerde iki paket: ozel (overlay olabilir) + global
    namespace Fs = std::filesystem;
    const Fs::path tRoot = Fs::temp_directory_path() / "map_plugin_index_test";
    std::error_code tIgnore;
    Fs::remove_all(tRoot, tIgnore);
    Fs::create_directories(tRoot / "Custom" / "Earth nav data" / "+40-130");
    Fs::create_directories(tRoot / "Global" / "Earth nav data" / "+40-130");
    std::ofstream(tRoot / "Custom" / "Earth nav data" / "+40-130" / "+47-123.dsf") << "custom";
    std::ofstream(tRoot / "Global" / "Earth nav data" / "+40-130" / "+47-123.dsf") << "global";
    std::ofstream(tRoot / "Global" / "Earth nav data" / "+40-130" / "+46-124.dsf") << "global";
    std::ofstream(tRoot / "Global" / "Earth nav data" / "+40-130" / "notes.txt") << "x";

    SceneryIndex tIndex;
    tIndex.BuildFromPacks({(tRoot / "Custom").string() + "/"}, {(tRoot / "Global").string() + "/"});
    const std::vector<DsfCandidate>* tCandidates = tIndex.CandidatesFor(-123, 47);
    CHECK(tCandidates && tCandidates->size() == 2 && tCandidates->front().mPath.find("Custom") != std::string::npos,
          "ozel paket once");
    CHECK(tIndex.TileCount() == 2 && tIndex.FileCount() == 3 && tIndex.PackCount() == 2, "sayilar");
    CHECK(tIndex.CoverageOf(-123, 47) == TileCoverage::HasDsf, "DSF var");
    CHECK(tIndex.CoverageOf(-125, 48) == TileCoverage::Ocean, "kurulu bolgede DSF yok: deniz");
    CHECK(tIndex.CoverageOf(29, 41) == TileCoverage::NoData, "kurulmamis bolge: veri yok");
    Fs::remove_all(tRoot, tIgnore);
}

void TestStoreCancel()
{
    // Yukleme bitmeden yok edilen depo takilmadan kapanmali (X-Plane kapanisi)
    AirportGeometryStore tStore(std::make_unique<AptDatIndexer>());
    tStore.StartLoading("Z:\\no_such_folder\\");
}

// LTAC 03R/21L pistini veren kaynak (gercek apt.dat koordinatlari)
class LtacRunwaySource : public IAirportGeometrySource
{
public:
    const char* Name() const override
    {
        return "LTAC test";
    }

    bool LoadAll(const std::string& /*pXPlaneDirectory*/, const std::atomic<bool>& /*pCancel*/,
                 AirportIndex& pOut, std::string& /*pOutReport*/) override
    {
        AirportGeometry tAirport;
        tAirport.mId = "LTAC";
        Runway tRunway;
        tRunway.mWidthM = 59.0;
        tRunway.mEnds[0].mName = "03R";
        tRunway.mEnds[0].mPosition = {40.1140623, 32.9831635};
        tRunway.mEnds[1].mName = "21L";
        tRunway.mEnds[1].mPosition = {40.1413103, 33.0091255};
        tAirport.mRunways.push_back(tRunway);
        pOut.mAirports.push_back(tAirport);
        return true;
    }
};

void WaitUntilLoaded(AirportGeometryStore& pStore)
{
    for (int tTry = 0; tTry < 500 && !pStore.Poll(); ++tTry)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

void TestLocalizerMatch()
{
    // Yon yardimcilari
    CHECK(Near(InitialBearingDeg({0, 0}, {0, 1}), 90.0, 1e-9) && Near(InitialBearingDeg({0, 0}, {1, 0}), 0.0, 1e-9),
          "dogu 90, kuzey 0");
    CHECK(Near(InitialBearingDeg({0, 0}, {0, -1}), 270.0, 1e-9), "bati 270 (eksi degil)");
    CHECK(Near(AngleDifferenceDeg(350.0, 10.0), 20.0, 1e-12) && Near(AngleDifferenceDeg(90.0, 270.0), 180.0, 1e-12),
          "aci farki 0..180");

    // Havalimani turu ve ad oneki
    std::string tClean;
    CHECK(AirportKindFromName("[H] Harborview Medical Center", &tClean) == AirportKind::Heliport &&
              tClean == "Harborview Medical Center",
          "heliport: '%s'", tClean.c_str());
    CHECK(AirportKindFromName("[S] Kenmore Air Harbor", &tClean) == AirportKind::Seaplane, "deniz ussu");
    CHECK(AirportKindFromName("[X] Eski Alan", &tClean) == AirportKind::Closed && tClean == "Eski Alan", "kapali");
    CHECK(AirportKindFromName("Esenboga", &tClean) == AirportKind::Land && tClean == "Esenboga", "kara");
    CHECK(AirportKindFromName("[Q] Garip", &tClean) == AirportKind::Land && tClean == "[Q] Garip",
          "bilinmeyen onek oldugu gibi kalir");

    // ILS -> pist esigi
    AirportGeometryStore tStore(std::make_unique<LtacRunwaySource>());
    LatLon tThreshold = {0.0, 0.0};
    CHECK(!FindLocalizerThreshold({40.14, 33.01}, 36.0, tStore, &tThreshold), "pist verisi yokken bulunamaz");
    tStore.StartLoading("");
    WaitUntilLoaded(tStore);
    CHECK(tStore.IsReady(), "test deposu hazir");

    const LatLon tEnd03R = {40.1140623, 32.9831635};
    const LatLon tEnd21L = {40.1413103, 33.0091255};
    const double tCourse03R = InitialBearingDeg(tEnd03R, tEnd21L);

    // 03R'ye inis: anten 21L ucunun ~300 m otesinde
    const LatLon tAntenna03R = DestinationPoint(tEnd21L, tCourse03R, 0.16);
    CHECK(FindLocalizerThreshold(tAntenna03R, tCourse03R, tStore, &tThreshold) &&
              Near(tThreshold.mLat, tEnd03R.mLat, 1e-9) && Near(tThreshold.mLon, tEnd03R.mLon, 1e-9),
          "03R ILS esigi 03R ucu");

    // Ters yon: 21L'ye inis, anten 03R'nin otesinde
    const LatLon tAntenna21L = DestinationPoint(tEnd03R, tCourse03R + 180.0, 0.16);
    CHECK(FindLocalizerThreshold(tAntenna21L, tCourse03R + 180.0, tStore, &tThreshold) &&
              Near(tThreshold.mLat, tEnd21L.mLat, 1e-9),
          "21L ILS esigi 21L ucu");

    // Eksenden 0.5 NM kayik (LDA gibi) ve yonu tutmayan localizer eslesmez
    const LatLon tOffset = DestinationPoint(tAntenna03R, tCourse03R + 90.0, 0.5);
    CHECK(!FindLocalizerThreshold(tOffset, tCourse03R, tStore, &tThreshold), "kayik localizer eslesmez");
    CHECK(!FindLocalizerThreshold(tAntenna03R, tCourse03R + 90.0, tStore, &tThreshold), "yonu tutmayan eslesmez");
}

void TestMapViewBounds()
{
    const MapView tView = MakeView();
    const GeoBounds tVisible = tView.VisibleBounds();
    RectD tLarger = tView.Viewport();
    tLarger.mLeft -= 100;
    tLarger.mRight += 100;
    tLarger.mBottom -= 100;
    tLarger.mTop += 100;
    const GeoBounds tExpanded = tView.BoundsOf(tLarger);
    CHECK(tExpanded.mWest < tVisible.mWest && tExpanded.mEast > tVisible.mEast &&
              tExpanded.mSouth < tVisible.mSouth && tExpanded.mNorth > tVisible.mNorth,
          "genisletilmis alan daha buyuk");
    const LatLon tCorner = tView.BoxelToLatLon(tView.Viewport().mLeft, tView.Viewport().mTop);
    CHECK(Near(tVisible.mWest, tCorner.mLon, 1e-9) && Near(tVisible.mNorth, tCorner.mLat, 1e-9),
          "sinir kosesi BoxelToLatLon ile ayni");

    CHECK(Near(tView.MetersPerBoxelAt(tView.Center().mLat), tView.MetersPerBoxel(), 1e-9),
          "merkezde iki olcek ayni");
    CHECK(tView.MetersPerBoxelAt(60.0) < tView.MetersPerBoxelAt(0.0), "kuzeyde boxel daha az metre");
}

}  // namespace

int main()
{
    TestScreenMath();
    TestMercator();
    TestMapView();
    TestGeo();
    TestMapTheme();
    TestDrawings();
    TestMapFormat();
    TestMesh();
    TestSettings();
    TestLayerManager();
    TestSubLayers();
    TestNavDatabase();
    TestLabelPlacer();
    TestMapViewBounds();
    TestAptLine();
    TestAptDatScanner();
    TestAptDatDetail();
    TestAptIndexCache();
    TestAirportShapes();
    TestAirportGeometryStore();
    TestStoreCancel();
    TestLocalizerMatch();
    TestDrawingsFile();
    TestAirways();
    TestLabelPriority();
    TestDsfReader();
    TestTerrain();
    TestLandBlend();
    TestTerrainWarp();
    TestSceneryIndex();
    std::printf("%d checks passed, %d failed\n", gPassCount, gFailCount);
    return gFailCount == 0 ? 0 : 1;
}
