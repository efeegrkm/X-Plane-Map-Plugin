
// X-Plane OLMADAN calisan testler: projeksiyon, koordinat matematigi,
// cografi hesaplar, cizim modeli, ayar dosyasi, katman yoneticisi.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Drawings.h"
#include "Geo.h"
#include "LabelPlacer.h"
#include "LayerManager.h"
#include "engine/LayerTable.h"
#include "XMapTheme.h"
#include "Log.h"
#include "XMapFormat.h"
#include "XMapView.h"
#include "ScreenMath.h"
#include "Settings.h"
#include "data/AirportGeometryStore.h"
#include "data/AirwayDatabase.h"
#include "data/AtomicFile.h"
#include "data/AirportShapes.h"
#include "data/AptDatIndexer.h"
#include "data/AptDatParser.h"
#include "data/AptLine.h"
#include "data/LocalizerMatch.h"
#include "data/DsfReader.h"
#include "data/ElevationCodec.h"
#include "data/LandColorField.h"
#include "data/NavDatabase.h"
#include "data/SceneryIndex.h"
#include "data/SevenZip.h"
#include "data/TerrainClasses.h"
#include "data/TerrainElevation.h"
#include "data/TerrainPaletteSource.h"
#include "data/TerrainNoise.h"
#include "data/TerrainRaster.h"
#include "data/TerrainRelief.h"
#include "data/TerrainStore.h"
#include "data/TerrainTriangles.h"
#include "data/TerrainVectors.h"
#include "data/WorldTileMap.h"
#include "gfx/Mesh.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

// Log.cpp (XPLMDebugString kullanir) yerine bos bir taklit
void logLine(std::string_view /*pText*/)
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

bool near(double pA, double pB, double pEpsilon)
{
    return std::fabs(pA - pB) <= pEpsilon;
}

// H-04: arka plan isini kosul bazli bekler; ust sinir 30 sn (sabit bekleme yok: is bitince hemen
// devam eder, yavas ya da yuklu makinede de sinir yetmelidir). Kosul gec de olsa gerceklesmezse
// test kendi CHECK'inde acikca basarisiz olur.
template <typename TCond>
void waitUntil(TCond pCondition)
{
    const auto tDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (!pCondition() && std::chrono::steady_clock::now() < tDeadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

// openGL matris yardimcilari (sutun oncelikli)

struct Mat4
{
    float mM[16];
};

Mat4 identity()
{
    Mat4 tResult{};
    tResult.mM[0] = 1;
    tResult.mM[5] = 1;
    tResult.mM[10] = 1;
    tResult.mM[15] = 1;
    return tResult;
}

Mat4 ortho(double pLeft, double pRight, double pBottom, double pTop)
{
    Mat4 tResult = identity();
    tResult.mM[0] = float(2 / (pRight - pLeft));
    tResult.mM[5] = float(2 / (pTop - pBottom));
    tResult.mM[10] = -1;
    tResult.mM[12] = float(-(pRight + pLeft) / (pRight - pLeft));
    tResult.mM[13] = float(-(pTop + pBottom) / (pTop - pBottom));
    return tResult;
}

// X-Plane pencere cizerken: projeksiyon boxel'den NDC'ye, viewport piksel.
// UI olcek s ise pencerenin W piksel genisligi W/s boxel eder.
void testWindowTransform(const char* pName, double pUiScale, int pWidth, int pHeight, double pOriginX)
{
    const int tViewport[4] = {0, 0, pWidth, pHeight};
    const Mat4 tModelview = identity();
    const Mat4 tProjection = ortho(pOriginX, pOriginX + pWidth / pUiScale, 0, pHeight / pUiScale);

    Affine2D tBoxelToPixel;
    CHECK(affineFromGL(tModelview.mM, tProjection.mM, tViewport, &tBoxelToPixel), "%s: matris okunmali", pName);

    double tPx = 0.0;
    double tPy = 0.0;
    tBoxelToPixel.apply(pOriginX, 0, &tPx, &tPy);
    CHECK(near(tPx, 0, 0.01) && near(tPy, 0, 0.01), "%s: sol alt kose (%.2f,%.2f)", pName, tPx, tPy);
    tBoxelToPixel.apply(pOriginX + pWidth / pUiScale, pHeight / pUiScale, &tPx, &tPy);
    CHECK(near(tPx, pWidth, 0.05) && near(tPy, pHeight, 0.05), "%s: sag ust kose (%.2f,%.2f)", pName, tPx, tPy);

    RectD tBoxels;
    tBoxels.mLeft = pOriginX + 100;
    tBoxels.mRight = pOriginX + 500;
    tBoxels.mBottom = 50;
    tBoxels.mTop = 350;
    const RectD tPixels = transformRectBBox(tBoxelToPixel, tBoxels);
    CHECK(near(tPixels.mLeft, 100 * pUiScale, 0.05) && near(tPixels.mTop, 350 * pUiScale, 0.05),
          "%s: scissor dikdortgeni (%.2f,%.2f)", pName, tPixels.mLeft, tPixels.mTop);

    // Olcek: boxel basina piksel
    const double tScale =
        std::sqrt(std::fabs(tBoxelToPixel.mA * tBoxelToPixel.mD - tBoxelToPixel.mB * tBoxelToPixel.mC));
    CHECK(near(tScale, pUiScale, 1e-4), "%s: UI olcek %.4f", pName, tScale);
}

void testScreenMath()
{
    testWindowTransform("UI %100", 1.0, 1920, 1080, 0);
    testWindowTransform("UI %150", 1.5, 2560, 1440, 0);
    testWindowTransform("UI %200, sol monitor (eksi x)", 2.0, 3840, 2160, -1920);

    {
        Mat4 tProjection = identity();
        tProjection.mM[11] = -1;
        tProjection.mM[15] = 0;
        const Mat4 tModelview = identity();
        const int tViewport[4] = {0, 0, 100, 100};
        Affine2D tResult;
        CHECK(!affineFromGL(tModelview.mM, tProjection.mM, tViewport, &tResult), "perspektif reddedilmeli");
    }

    {
        Affine2D tMatrix;
        tMatrix.mA = 2;
        tMatrix.mB = 0.5;
        tMatrix.mC = -0.3;
        tMatrix.mD = 1.5;
        tMatrix.mTx = 10;
        tMatrix.mTy = -4;
        Affine2D tInverse;
        CHECK(invert(tMatrix, &tInverse), "ters alinmali");
        const Affine2D tIdentity = compose(tInverse, tMatrix);
        CHECK(near(tIdentity.mA, 1, 1e-12) && near(tIdentity.mD, 1, 1e-12) && near(tIdentity.mB, 0, 1e-12) &&
                  near(tIdentity.mC, 0, 1e-12) && near(tIdentity.mTx, 0, 1e-9) && near(tIdentity.mTy, 0, 1e-9),
              "A^-1 * A = birim");
    }

    {
        RectD tFirst{0, 100, 100, 0};
        RectD tSecond{50, 150, 150, 50};
        RectD tOut;
        CHECK(intersectRect(tFirst, tSecond, &tOut) && tOut.mLeft == 50 && tOut.mTop == 100 &&
                  tOut.mRight == 100 && tOut.mBottom == 50,
              "kesisim");
        RectD tFar{200, 300, 300, 200};
        CHECK(!intersectRect(tFirst, tFar, &tOut), "kesismeyen");
    }

    {
        // API Geometry: sol-ust + boyut; y yukari artar (0.24-0.25'te alt kenar ters hesaplaniyordu)
        const RectI tRect = rectFromTopLeft(100, 800, 900, 600);
        CHECK(tRect.mLeft == 100 && tRect.mTop == 800 && tRect.mRight == 1000 && tRect.mBottom == 200,
              "Geometry -> RectI kenarlari");
        CHECK(tRect.width() == 900 && tRect.height() == 600, "Geometry -> RectI boyutu pozitif");
    }
}

void testMercator()
{
    CHECK(near(Mercator::lonToX(-180), 0.0, 1e-12) && near(Mercator::lonToX(180), 1.0, 1e-12), "boylam uclari");
    CHECK(near(Mercator::latToY(0), 0.5, 1e-12), "ekvator ortada");
    CHECK(near(Mercator::latToY(Mercator::kMaxLat), 1.0, 1e-6), "en kuzey enlem = 1");
    CHECK(near(Mercator::latToY(-Mercator::kMaxLat), 0.0, 1e-6), "en guney enlem = 0");
    CHECK(Mercator::latToY(89.9) <= 1.0, "kutup kirpilmali");
    const double tLats[] = {-60.0, -12.5, 0.0, 41.0, 41.2620, 70.0};
    for (double tLat : tLats)
    {
        CHECK(near(Mercator::yToLat(Mercator::latToY(tLat)), tLat, 1e-9), "enlem gidis-donus %.4f", tLat);
    }
    CHECK(near(Mercator::xToLon(Mercator::lonToX(28.742)), 28.742, 1e-12), "boylam gidis-donus");
}

XMapView makeView()
{
    XMapView tView;
    RectD tViewport;
    tViewport.mLeft = 100;
    tViewport.mRight = 1100;
    tViewport.mBottom = 300;
    tViewport.mTop = 900;
    tView.setViewport(tViewport);
    tView.setCenter({41.0, 29.0});
    tView.setZoom(8.25);
    return tView;
}

void testMapView()
{
    {
        const XMapView tView = makeView();
        double tX = 0.0;
        double tY = 0.0;
        tView.latLonToBoxel({41.0, 29.0}, &tX, &tY);
        CHECK(near(tX, 600, 1e-6) && near(tY, 600, 1e-6), "merkez ortada (%.3f,%.3f)", tX, tY);
        const LatLon tCenter = tView.center();
        CHECK(near(tCenter.mLat, 41.0, 1e-9) && near(tCenter.mLon, 29.0, 1e-9), "Center() geri verir");
    }

    {
        const XMapView tView = makeView();
        const LatLon tPoints[] = {{41.2620, 28.7420}, {40.1281, 32.9951}, {38.0, 26.0}, {42.9, 29.5}};
        for (const LatLon& tPoint : tPoints)
        {
            double tX = 0.0;
            double tY = 0.0;
            tView.latLonToBoxel(tPoint, &tX, &tY);
            const LatLon tBack = tView.boxelToLatLon(tX, tY);
            CHECK(near(tBack.mLat, tPoint.mLat, 1e-9) && near(tBack.mLon, tPoint.mLon, 1e-9),
                  "lat/lon gidis-donus %.4f,%.4f -> %.9f,%.9f", tPoint.mLat, tPoint.mLon, tBack.mLat, tBack.mLon);
        }
    }

    {
        const XMapView tView = makeView();
        for (int tIndex = 0; tIndex < 25; ++tIndex)
        {
            const double tX = 100 + 1000.0 * ((tIndex % 5) + 0.5) / 5;
            const double tY = 300 + 600.0 * ((tIndex / 5) + 0.5) / 5;
            double tBackX = 0.0;
            double tBackY = 0.0;
            tView.latLonToBoxel(tView.boxelToLatLon(tX, tY), &tBackX, &tBackY);
            CHECK(near(tBackX, tX, 1e-6) && near(tBackY, tY, 1e-6), "boxel gidis-donus (%.1f,%.1f)", tX, tY);
        }
    }

    // Yukari = kuzey, saga = dogu
    {
        const XMapView tView = makeView();
        const LatLon tUpperRight = tView.boxelToLatLon(1000, 800);
        CHECK(tUpperRight.mLat > 41.0 && tUpperRight.mLon > 29.0, "sag ust kuzeydogu");
    }

    // Zoom imlecin etrafinda: imlecin altindaki yer sabit kalmali
    {
        const double tDeltas[] = {1.5, -2.25, 0.5, 30.0, -30.0};  // son ikisi sinira takilir
        XMapView tView = makeView();
        for (double tDelta : tDeltas)
        {
            const double tCursorX = 250;
            const double tCursorY = 740;
            const LatLon tBefore = tView.boxelToLatLon(tCursorX, tCursorY);
            tView.zoomAt(tCursorX, tCursorY, tDelta);
            const LatLon tAfter = tView.boxelToLatLon(tCursorX, tCursorY);
            CHECK(near(tBefore.mLat, tAfter.mLat, 1e-9) && near(tBefore.mLon, tAfter.mLon, 1e-9),
                  "zoom %+.2f sonrasi imlec kaydi: %.9f,%.9f -> %.9f,%.9f", tDelta, tBefore.mLat, tBefore.mLon,
                  tAfter.mLat, tAfter.mLon);
        }
        CHECK(near(tView.zoom(), XMapView::kMinZoom, 1e-12), "zoom alt sinira kirpildi: %.2f", tView.zoom());
    }

    {
        XMapView tView = makeView();
        const LatLon tPoint = tView.boxelToLatLon(500, 500);
        tView.panByBoxels(30, -20);
        double tX = 0.0;
        double tY = 0.0;
        tView.latLonToBoxel(tPoint, &tX, &tY);
        CHECK(near(tX, 530, 1e-6) && near(tY, 480, 1e-6), "surukleme (%.3f,%.3f)", tX, tY);
    }

    // Olcek: ekvatorda zoom 3, bir boxel = 2*pi*R / 2048 metre
    {
        XMapView tView;
        tView.setCenter({0.0, 0.0});
        tView.setZoom(3.0);
        CHECK(near(tView.metersPerBoxel(), 2 * 3.14159265358979 * 6378137.0 / 2048.0, 0.01), "metre/boxel");
        tView.setCenter({60.0, 0.0});
        CHECK(near(tView.metersPerBoxel(), 2 * 3.14159265358979 * 6378137.0 / 2048.0 * 0.5, 0.01),
              "60 derecede olcek yariya iner");
    }

    // 180. boylam: dunyanin en yakin kopyasi secilir
    {
        XMapView tView = makeView();
        tView.setCenter({0.0, 179.5});
        double tX = 0.0;
        double tY = 0.0;
        tView.latLonToBoxel({0.0, -179.5}, &tX, &tY);
        CHECK(tX > 600, "-179.5 boylami merkezin saginda olmali (x=%.1f)", tX);
        const GeoBounds tBounds = tView.visibleBounds();
        CHECK(tBounds.mWest < 179.5 && tBounds.mEast > 179.5, "gorunen alan merkezi kapsar");
    }
}

double triangleArea(const std::vector<LatLon>& pPoints, const std::vector<int>& pTriangles)
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

double polygonArea(const std::vector<LatLon>& pPoints)
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

void testGeo()
{
    const LatLon tLtfm{41.2620, 28.7420};
    const LatLon tLtac{40.1281, 32.9951};
    const double tDistance = distanceNm(tLtfm, tLtac);
    CHECK(tDistance > 200 && tDistance < 210, "LTFM-LTAC mesafe %.1f NM (gercek ~380 km = ~205 NM)", tDistance);
    CHECK(near(distanceNm({0, 0}, {0, 1}), 60.04, 0.1), "ekvatorda 1 derece ~60 NM");

    const LatLon tMid = greatCircleInterpolate(tLtfm, tLtac, 0.5);
    CHECK(near(distanceNm(tLtfm, tMid), tDistance / 2, 0.01), "orta nokta");

    std::vector<LatLon> tDense;
    densifySegment(tLtfm, tLtac, 5.0, tDense);
    CHECK(tDense.size() >= 41 && tDense.size() <= 42, "5 NM parcalara bolme: %d", static_cast<int>(tDense.size()));
    CHECK(near(tDense[0].mLat, tLtfm.mLat, 1e-12), "ilk nokta baslangic");
    std::vector<LatLon> tShort;
    densifySegment(tLtfm, {41.27, 28.75}, 5.0, tShort);
    CHECK(tShort.size() == 1, "kisa segment bolunmez");

    // Kare
    const std::vector<LatLon> tSquare = {{41, 29}, {41, 30}, {42, 30}, {42, 29}};
    std::vector<int> tTriangles;
    CHECK(triangulatePolygon(tSquare, tTriangles) && tTriangles.size() == 6, "kare 2 ucgen");
    CHECK(near(triangleArea(tSquare, tTriangles), polygonArea(tSquare), 1e-9), "kare alan");

    // Saat yonunde L sekli (icbukey)
    const std::vector<LatLon> tShapeL = {{0, 0}, {2, 0}, {2, 1}, {1, 1}, {1, 2}, {0, 2}};
    CHECK(triangulatePolygon(tShapeL, tTriangles) && tTriangles.size() == 12, "L 4 ucgen");
    CHECK(near(triangleArea(tShapeL, tTriangles), polygonArea(tShapeL), 1e-9), "L alan %.3f vs %.3f",
          triangleArea(tShapeL, tTriangles), polygonArea(tShapeL));
    const std::vector<LatLon> tShapeLReversed(tShapeL.rbegin(), tShapeL.rend());
    CHECK(triangulatePolygon(tShapeLReversed, tTriangles) &&
              near(triangleArea(tShapeLReversed, tTriangles), polygonArea(tShapeLReversed), 1e-9),
          "ters yon L");

    // Kendini kesen (papyon)
    const std::vector<LatLon> tBowTie = {{0, 0}, {1, 1}, {1, 0}, {0, 1}};
    CHECK(!triangulatePolygon(tBowTie, tTriangles), "papyon reddedilmeli");

    // Ayni dogru uzerinde kose
    const std::vector<LatLon> tCollinear = {{0, 0}, {0, 1}, {0, 2}, {1, 2}, {1, 0}};
    CHECK(triangulatePolygon(tCollinear, tTriangles) &&
              near(triangleArea(tCollinear, tTriangles), polygonArea(tCollinear), 1e-9),
          "dogrusal kose");

    // dogru(alansiz)
    const std::vector<LatLon> tLine = {{0, 0}, {0, 1}, {0, 2}};
    CHECK(!triangulatePolygon(tLine, tTriangles), "alansiz reddedilmeli");

    // Hedef nokta: ekvatorda doguya 60 NM ~ 1 derece, kuzeye 60 NM ~ 1 derece
    const LatLon tEast = destinationPoint({0, 0}, 90.0, 60.04);
    CHECK(near(tEast.mLat, 0.0, 1e-9) && near(tEast.mLon, 1.0, 1e-3), "doguya 60 NM: %.5f,%.5f", tEast.mLat,
          tEast.mLon);
    const LatLon tNorth = destinationPoint({41.0, 29.0}, 0.0, 60.04);
    CHECK(near(tNorth.mLat, 42.0, 1e-3) && near(tNorth.mLon, 29.0, 1e-9), "kuzeye 60 NM");
    const LatLon tFar = destinationPoint(tLtfm, 117.0, 123.4);
    CHECK(near(distanceNm(tLtfm, tFar), 123.4, 1e-6), "hedef noktaya mesafe geri tutar");
}

void testDrawings()
{
    Drawings tModel;
    tModel.setMode(Drawings::Mode::Pan);
    tModel.addVertex({1, 1});
    CHECK(tModel.shapeCount() == 0, "gezin modunda ekleme yok");

    tModel.setMode(Drawings::Mode::Point);
    tModel.addVertex({1, 1});
    tModel.addVertex({2, 2});
    CHECK(tModel.shapeCount() == 2 && tModel.shapes()[1].mNumber == 2, "iki nokta P1 P2");

    tModel.setMode(Drawings::Mode::Route);
    tModel.addVertex({0, 0});
    CHECK(tModel.current() && tModel.current()->mPoints.size() == 1, "rota basladi");
    tModel.setMode(Drawings::Mode::Region);  // 1 koseli rota atilmali
    CHECK(tModel.shapeCount() == 2 && !tModel.current(), "1 koseli rota atildi");

    tModel.addVertex({0, 0});
    tModel.addVertex({0, 1});
    tModel.addVertex({1, 1});
    tModel.finish();
    CHECK(tModel.shapeCount() == 3 && tModel.shapes()[2].mKind == Drawings::ShapeKind::Region &&
              tModel.shapes()[2].mTriangles.size() == 3,
          "bolge B1 ucgenlendi");

    tModel.setMode(Drawings::Mode::Route);
    tModel.addVertex({0, 0});
    tModel.addVertex({0, 1});
    tModel.addVertex({0, 2});
    tModel.undo();
    CHECK(tModel.current() && tModel.current()->mPoints.size() == 2, "geri al: kose");
    tModel.finish();
    CHECK(tModel.shapeCount() == 4 && tModel.shapes()[3].mNumber == 1 && tModel.shapes()[3].mPoints.size() == 2,
          "rota R1");
    tModel.undo();
    CHECK(tModel.shapeCount() == 3, "geri al: sekil");
    tModel.addVertex({5, 5});
    tModel.addVertex({5, 6});
    tModel.finish();
    CHECK(tModel.shapes()[3].mNumber == 1, "numara geri alinca tekrar kullanilir");

    tModel.clear();
    CHECK(tModel.shapeCount() == 0 && !tModel.current(), "temizle");
}

RectD meshBounds(const Gfx::Mesh& pMesh)
{
    RectD tBounds;
    tBounds.mLeft = 1e9;
    tBounds.mBottom = 1e9;
    tBounds.mRight = -1e9;
    tBounds.mTop = -1e9;
    for (const Gfx::ColorVertex& tVertex : pMesh.vertices())
    {
        tBounds.mLeft = std::min<double>(tBounds.mLeft, tVertex.mX);
        tBounds.mRight = std::max<double>(tBounds.mRight, tVertex.mX);
        tBounds.mBottom = std::min<double>(tBounds.mBottom, tVertex.mY);
        tBounds.mTop = std::max<double>(tBounds.mTop, tVertex.mY);
    }
    return tBounds;
}

double meshArea(const Gfx::Mesh& pMesh)
{
    const std::vector<Gfx::ColorVertex>& tVertices = pMesh.vertices();
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

void testMesh()
{
    const Rgba tRed = {1.0f, 0.0f, 0.0f, 1.0f};

    Gfx::Mesh tLine;
    tLine.addLine({0.0f, 0.0f}, {10.0f, 0.0f}, 2.0f, tRed);
    const RectD tLineBounds = meshBounds(tLine);
    CHECK(tLine.vertices().size() == 6, "cizgi = 2 ucgen");
    CHECK(near(tLineBounds.mBottom, -1.0, 1e-6) && near(tLineBounds.mTop, 1.0, 1e-6) &&
              near(tLineBounds.mRight, 10.0, 1e-6),
          "cizgi kalinligi iki yana yari yariya");
    CHECK(near(meshArea(tLine), 20.0, 1e-4), "cizgi alani = boy x kalinlik");
    CHECK(tLine.vertices()[0].mColor.mR == 1.0f, "renk koseye yazildi");

    Gfx::Mesh tEmpty;
    tEmpty.addLine({3.0f, 3.0f}, {3.0f, 3.0f}, 2.0f, tRed);
    CHECK(tEmpty.isEmpty(), "sifir uzunluklu cizgi atlanir");

    Gfx::Mesh tOpen;
    tOpen.addPolyline({{0.0f, 0.0f}, {10.0f, 0.0f}, {10.0f, 10.0f}}, false, 2.0f, tRed);
    CHECK(tOpen.vertices().size() == 2 * 6 + 1 * 6, "acik cizgi: 2 parca + 1 kose birlesimi");

    Gfx::Mesh tClosed;
    tClosed.addPolyline({{0.0f, 0.0f}, {10.0f, 0.0f}, {10.0f, 10.0f}, {0.0f, 10.0f}}, true, 2.0f, tRed);
    const RectD tClosedBounds = meshBounds(tClosed);
    CHECK(tClosed.vertices().size() == 4 * 6 + 4 * 6, "kapali cizgi: 4 parca + 4 kose");
    CHECK(near(tClosedBounds.mLeft, -1.0, 1e-6) && near(tClosedBounds.mTop, 11.0, 1e-6),
          "kose birlesimi disari en fazla yari kalinlik tasar");

    Gfx::Mesh tFrame;
    RectD tBox;
    tBox.mLeft = 0.0;
    tBox.mBottom = 0.0;
    tBox.mRight = 20.0;
    tBox.mTop = 10.0;
    tFrame.addFrame(tBox, 1.0f, tRed);
    const RectD tFrameBounds = meshBounds(tFrame);
    CHECK(tFrame.vertices().size() == 4 * 6, "cerceve = 4 dikdortgen");
    CHECK(near(tFrameBounds.mLeft, 0.0, 1e-9) && near(tFrameBounds.mRight, 20.0, 1e-9) &&
              near(tFrameBounds.mTop, 10.0, 1e-9),
          "cerceve kutunun icinde kalir");
    CHECK(near(meshArea(tFrame), 20.0 * 10.0 - 18.0 * 8.0, 1e-4), "cerceve ust uste binmez");

    Gfx::Mesh tDisc;
    tDisc.addDisc({0.0f, 0.0f}, 10.0f, tRed);
    CHECK(tDisc.vertices().size() == static_cast<size_t>(Gfx::Mesh::segmentsFor(10.0f)) * 3, "daire parca sayisi");
    CHECK(meshArea(tDisc) > 3.0 * 100.0 && meshArea(tDisc) < 3.1416 * 100.0, "daire alani ~ pi r^2");

    Gfx::Mesh tDots;
    tDots.addDottedCircle({0.0f, 0.0f}, 7.0f, 12, 2.0f, tRed);
    CHECK(tDots.vertices().size() == 12 * 6 && near(meshArea(tDots), 12 * 4.0, 1e-3), "12 kare nokta");

    Gfx::OutlinedLines tOutlined(2.0f);
    tOutlined.addLine({0.0f, 0.0f}, {10.0f, 0.0f}, 1.5f, tRed);
    CHECK(near(meshBounds(tOutlined.outline()).mTop, 1.75, 1e-6) && near(meshBounds(tOutlined.fill()).mTop, 0.75, 1e-6),
          "kontur cizgiden 2 boxel kalin");
    CHECK(tOutlined.outline().vertices()[0].mColor.mA == Gfx::kOutline.mA, "kontur rengi");
}

void testMapFormat()
{
    std::string tText = formatLatLon({41.26201, -28.74205});
    CHECK(tText == "41.26201 N  28.74205 W", "lat/lon bicimi: '%s'", tText.c_str());

    tText = formatGridLabel(41.0, true, 1.0);
    CHECK(tText == "41N", "izgara etiketi derece: '%s'", tText.c_str());
    tText = formatGridLabel(41.5, true, 0.5);
    CHECK(tText == "41 30N", "izgara etiketi dakika: '%s'", tText.c_str());
    tText = formatGridLabel(28.0 + 5.5 / 60.0, false, 0.5 / 60.0);
    CHECK(tText == "28 05.5E", "izgara etiketi yarim dakika: '%s'", tText.c_str());
    tText = formatGridLabel(-(12.0 + 20.0 / 60.0), false, 10.0 / 60.0);
    CHECK(tText == "12 20W", "izgara etiketi bati: '%s'", tText.c_str());
    tText = formatGridLabel(29.0 + 59.9999999 / 60.0, true, 1.0 / 60.0);
    CHECK(tText == "30 00N", "yuvarlama tasmasi: '%s'", tText.c_str());

    CHECK(near(chooseGridStepDeg(100.0, 90.0), 1.0, 1e-12), "izgara araligi 1 derece");
    CHECK(near(chooseGridStepDeg(1000.0, 90.0), 10.0 / 60.0, 1e-12), "izgara araligi 10 dakika");
    CHECK(near(chooseGridStepDeg(1.0, 90.0), 30.0, 1e-12), "izgara araligi ust sinir");

    CHECK(near(chooseScaleBarNm(0.1, 120.0), 10.0, 1e-9), "olcek 10 NM");
    CHECK(near(chooseScaleBarNm(1.0, 120.0), 100.0, 1e-9), "olcek 100 NM");
    CHECK(near(chooseScaleBarNm(0.013, 120.0), 1.0, 1e-9), "olcek 1 NM");
    CHECK(near(chooseScaleBarNm(0.003, 120.0), 0.2, 1e-9), "olcek 0.2 NM");
}

void testSettings()
{
    const char* tPath = "map_test_settings.json";
    {
        FILE* tFile = std::fopen(tPath, "w");
        CHECK(tFile != nullptr, "test dosyasi acilmali");
        if (tFile)
        {
            std::fputs("{\n  \"window\": { \"left\": \" -1200 \", \"top\": \"900\" },\n"
                       "  \"view\": { \"zoom\": \"7.25\", \"follow_aircraft\": \"0\" }\n}\n",
                       tFile);
            std::fclose(tFile);
        }
    }

    Settings tSettings;
    CHECK(tSettings.load(tPath), "dosya okunmali");
    CHECK(tSettings.getInt("window", "left", 0) == -1200, "bosluklu deger");
    CHECK(tSettings.getInt("window", "top", 0) == 900, "tamsayi");
    CHECK(near(tSettings.getDouble("view", "zoom", 0.0), 7.25, 1e-12), "ondalik nokta ile");
    CHECK(!tSettings.getBool("view", "follow_aircraft", true), "bool 0");
    CHECK(tSettings.getInt("window", "missing", 42) == 42, "olmayan anahtar varsayilan");
    CHECK(near(tSettings.getDouble("missing", "missing", 1.5), 1.5, 1e-12), "olmayan bolum varsayilan");

    tSettings.setDouble("view", "center_lat", 41.123456789);
    tSettings.setBool("layers", "grid", true);
    CHECK(tSettings.save(), "kaydedilmeli");

    Settings tReloaded;
    CHECK(tReloaded.load(tPath), "yeniden okunmali");
    CHECK(near(tReloaded.getDouble("view", "center_lat", 0.0), 41.123456789, 1e-8), "ondalik geri okunur");
    CHECK(tReloaded.getBool("layers", "grid", false), "bool geri okunur");
    CHECK(tReloaded.getInt("window", "left", 0) == -1200, "eski deger korunur");
    std::remove(tPath);

    Settings tMissing;
    CHECK(!tMissing.load("no_such_file.json"), "olmayan dosya false");
    CHECK(tMissing.getInt("a", "b", 3) == 3, "bos ayarlar varsayilan verir");
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

    void draw(const drawContext& /*pContext*/) override
    {
        *mTrace += "D" + id() + " ";
    }

    void drawLabels(const drawContext& /*pContext*/) override
    {
        *mTrace += "L" + id() + " ";
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
        addSubLayer("one", "One");
        addSubLayer("two", "Two", false);
    }

    void draw(const drawContext& /*pContext*/) override
    {
    }
};

void testSubLayers()
{
    LayerManager tLayers;
    tLayers.add(std::make_unique<PartedLayer>());
    Layer& tLayer = *tLayers.at(0);
    CHECK(tLayer.subLayerCount() == 2 && tLayer.isSubLayerVisible(0) && !tLayer.isSubLayerVisible(1),
          "alt katman varsayilanlari");
    CHECK(tLayer.isSubLayerVisible(7) && tLayer.visibleSubLayerCount() == 1, "gecersiz indeks gorunur sayilir");
    CHECK(LayerManager::subLayerKey(tLayer, 1) == "parted.two", "alt katman anahtari");

    const XMapTheme& tScenery = themeFor(XMapMode::Scenery);
    Settings tSettings;
    tLayer.setSubLayerVisible(0, false);
    tLayer.setSubLayerVisible(1, true);
    tLayers.saveVisibility(tSettings, tScenery);
    CHECK(!tSettings.getBool("layers.scenery", "parted.one", true) &&
              tSettings.getBool("layers.scenery", "parted.two", false),
          "alt katman kaydi");

    Settings tEmpty;
    tLayers.loadVisibility(tEmpty, tScenery);
    CHECK(tLayer.isSubLayerVisible(0) && !tLayer.isSubLayerVisible(1), "ayar yoksa alt katman varsayilani");
    tLayers.loadVisibility(tSettings, tScenery);
    CHECK(!tLayer.isSubLayerVisible(0) && tLayer.isSubLayerVisible(1), "alt katman geri okunur");
}

void testLayerManager()
{
    std::string tTrace;
    LayerManager tLayers;
    tLayers.add(std::make_unique<CountingLayer>("a", true, &tTrace));
    tLayers.add(std::make_unique<CountingLayer>("b", false, &tTrace));
    tLayers.add(std::make_unique<CountingLayer>("c", true, &tTrace));
    CHECK(tLayers.count() == 3 && tLayers.at(3) == nullptr && tLayers.at(-1) == nullptr, "katman sayisi");

    drawContext tContext;
    tLayers.drawAll(tContext);
    CHECK(tTrace == "Da Dc La Lc ", "cizim sirasi: once geometri sonra yazi, kapali atlanir: '%s'", tTrace.c_str());

    const XMapTheme& tScenery = themeFor(XMapMode::Scenery);
    Settings tSettings;
    tLayers.at(0)->setVisible(false);
    tLayers.saveVisibility(tSettings, tScenery);
    CHECK(!tSettings.getBool("layers.scenery", "a", true) && !tSettings.getBool("layers.scenery", "b", true) &&
              tSettings.getBool("layers.scenery", "c", false),
          "gorunurluk kaydi (mod bolumu)");

    tSettings.setBool("layers.scenery", "b", true);
    tLayers.at(0)->setVisible(true);
    tLayers.loadVisibility(tSettings, tScenery);
    CHECK(!tLayers.at(0)->isVisible() && tLayers.at(1)->isVisible() && tLayers.at(2)->isVisible(),
          "gorunurluk okuma");

    Settings tEmpty;
    tLayers.loadVisibility(tEmpty, tScenery);
    CHECK(tLayers.at(0)->isVisible() && !tLayers.at(1)->isVisible(), "ayar yoksa katmanin varsayilani");

    CHECK(!LayerManager::themeDefault(*tLayers.at(1), tScenery) && LayerManager::themeDefault(*tLayers.at(0), tScenery),
          "katmanin mod varsayilani");

    // Mod varsayilani: VFR'de ILS kapali
    LayerManager tModeLayers;
    tModeLayers.add(std::make_unique<CountingLayer>("ils", true, &tTrace));
    tModeLayers.loadVisibility(tEmpty, themeFor(XMapMode::Vfr));
    CHECK(!tModeLayers.at(0)->isVisible(), "VFR temasi ILS'i varsayilan kapatir");
    tModeLayers.loadVisibility(tEmpty, themeFor(XMapMode::Dark));
    CHECK(tModeLayers.at(0)->isVisible(), "Dark'ta ILS acik");
}

void testMapTheme()
{
    CHECK(modeFromId("vfr", XMapMode::Scenery) == XMapMode::Vfr && modeFromId("ifr_high", XMapMode::Scenery) ==
              XMapMode::IfrHigh && modeFromId("xyz", XMapMode::IfrLow) == XMapMode::IfrLow,
          "mod kimligi okuma");
    CHECK(nextMode(XMapMode::IfrHigh) == XMapMode::Dark && nextMode(XMapMode::Dark) == XMapMode::Scenery &&
              nextMode(XMapMode::Scenery) == XMapMode::Vfr,
          "siradaki mod donguseldir");
    CHECK(layerSectionFor(XMapMode::IfrLow) == "layers.ifr_low", "mod bolumu adi");
    for (int tIndex = 0; tIndex < kXMapModeCount; ++tIndex)
    {
        const XMapTheme& tTheme = themeFor(static_cast<XMapMode>(tIndex));
        CHECK(static_cast<int>(tTheme.mMode) == tIndex, "tema sirasi: %s", tTheme.mId);
    }
    CHECK(allTerrainStyles().size() == static_cast<size_t>(kXMapModeCount) &&
              allTerrainStyles()[static_cast<int>(XMapMode::Vfr)].mStyle == TerrainStyle::Elevation,
          "zemin bicimleri mod sirasiyla");

    // Rakim rampasi: alcakta yesil baskin, yuksekte kahverengi (kirmizi > mavi, koyulasir)
    const TerrainColor tLow = elevationColor(0.0);
    const TerrainColor tHigh = elevationColor(4000.0);
    CHECK(tLow.mG > tLow.mR && tHigh.mR > tHigh.mB && tHigh.mG < tLow.mG, "rakim rampasi");
    CHECK(elevationColor(-50.0).mG == tLow.mG && elevationColor(20000.0).mR == elevationColor(9000.0).mR,
          "rampa uclarda sabit");
}

NavRecord makeRecord(NavKind pKind, const char* pId, float pLat, float pLon)
{
    NavRecord tRecord;
    tRecord.mKind = pKind;
    tRecord.mId = pId;
    tRecord.mLat = pLat;
    tRecord.mLon = pLon;
    return tRecord;
}

bool contains(const std::vector<int>& pIndices, int pValue)
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

void testNavDatabase()
{
    // ILS yonu: "manyetik * 360 + gercek"
    float tTrue = 0.0f;
    short tMagnetic = 0;
    decodeLocalizerCourse(84112.655, &tTrue, &tMagnetic);
    CHECK(near(tTrue, 232.655, 1e-3) && tMagnetic == 233, "ILS kodlu yon: %.3f / %d", tTrue, tMagnetic);
    decodeLocalizerCourse(11556.087, &tTrue, &tMagnetic);
    CHECK(near(tTrue, 36.087, 1e-3) && tMagnetic == 32, "LTAC 03L yonu: %.3f / %d", tTrue, tMagnetic);
    decodeLocalizerCourse(36.5, &tTrue, &tMagnetic);
    CHECK(near(tTrue, 36.5, 1e-6) && tMagnetic == -1, "kodsuz yon");

    CHECK(isPlaceholderId("----") && isPlaceholderId("") && !isPlaceholderId("IANC") && !isPlaceholderId("A-1"),
          "yer tutucu kimlik");
    CHECK(isTacanChannel(85) && !isTacanChannel(11380) && !isTacanChannel(0), "TACAN kanali");

    CHECK(isMajorAirportId("LTFM") && isMajorAirportId("KSEA"), "ICAO kodu buyuk");
    CHECK(!isMajorAirportId("LT51") && !isMajorAirportId("K1A2") && !isMajorAirportId("XPA0001") &&
              !isMajorAirportId("ABC"),
          "rakamli / farkli uzunluk kucuk");

    NavDatabase tDatabase;
    CHECK(!tDatabase.isReady(), "Build'den once hazir degil");
    tDatabase.add(makeRecord(NavKind::Airport, "LTFM", 41.262f, 28.742f));   // 0
    tDatabase.add(makeRecord(NavKind::Airport, "LTAC", 40.128f, 32.995f));   // 1
    tDatabase.add(makeRecord(NavKind::Vor, "IST", 40.9615f, 28.8107f));      // 2
    tDatabase.add(makeRecord(NavKind::Dme, "IST", 40.9615f, 28.8107f));      // 3: VOR ile eslesmeli
    tDatabase.add(makeRecord(NavKind::Dme, "IST", 43.5327f, 16.2891f));      // 4: cok uzak, tek basina
    tDatabase.add(makeRecord(NavKind::Ils, "IAKR", 40.1442f, 33.0088f));     // 5
    tDatabase.add(makeRecord(NavKind::Dme, "IAKR", 40.1200f, 32.9900f));     // 6: ILS DME (~1.6 NM)
    tDatabase.add(makeRecord(NavKind::Fix, "ABISI", 40.3627f, 31.9524f));    // 7
    tDatabase.add(makeRecord(NavKind::Fix, "WEST1", 10.5f, -179.5f));        // 8: 180. boylamin batisi
    tDatabase.add(makeRecord(NavKind::Fix, "EAST1", 10.5f, 179.5f));         // 9: 180. boylamin dogusu
    tDatabase.build();
    CHECK(tDatabase.isReady() && tDatabase.count() == 10, "hazir, 10 kayit");

    CHECK(tDatabase.at(2).mHasDme && tDatabase.at(3).mHidden, "VOR/DME eslesti");
    CHECK(!tDatabase.at(4).mHidden, "uzak DME eslesmez");
    CHECK(tDatabase.at(5).mHasDme && tDatabase.at(6).mHidden, "ILS DME eslesti");
    CHECK(tDatabase.countOf(NavKind::Dme) == 1, "izgarada tek basina 1 DME: %d", tDatabase.countOf(NavKind::Dme));
    CHECK(tDatabase.countOf(NavKind::Airport) == 2 && tDatabase.countOf(NavKind::Fix) == 3, "tur sayilari");

    // Istanbul cevresi: sadece LTFM gelmeli (LTAC baska hucrede)
    GeoBounds tIstanbul;
    tIstanbul.mSouth = 40.5;
    tIstanbul.mNorth = 41.5;
    tIstanbul.mWest = 28.0;
    tIstanbul.mEast = 29.5;
    std::vector<int> tResult;
    tDatabase.query(NavKind::Airport, tIstanbul, tResult);
    CHECK(tResult.size() == 1 && tResult[0] == 0, "Istanbul sorgusu: %d sonuc", static_cast<int>(tResult.size()));
    tResult.clear();
    tDatabase.query(NavKind::Vor, tIstanbul, tResult);
    CHECK(tResult.size() == 1 && tResult[0] == 2, "Istanbul VOR");
    tResult.clear();
    tDatabase.query(NavKind::Dme, tIstanbul, tResult);
    CHECK(tResult.empty(), "eslesen DME sorguda gelmez");

    // Turkiye: iki havalimani
    GeoBounds tTurkey;
    tTurkey.mSouth = 35.0;
    tTurkey.mNorth = 43.0;
    tTurkey.mWest = 25.0;
    tTurkey.mEast = 45.0;
    tResult.clear();
    tDatabase.query(NavKind::Airport, tTurkey, tResult);
    CHECK(tResult.size() == 2 && contains(tResult, 0) && contains(tResult, 1), "Turkiye sorgusu");

    // 180. boylami gecen alan (bati 179, dogu 181): iki fix de gelmeli
    GeoBounds tDateLine;
    tDateLine.mSouth = 10.0;
    tDateLine.mNorth = 11.0;
    tDateLine.mWest = 179.0;
    tDateLine.mEast = 181.0;
    tResult.clear();
    tDatabase.query(NavKind::Fix, tDateLine, tResult);
    CHECK(tResult.size() == 2 && contains(tResult, 8) && contains(tResult, 9), "180. boylam sorgusu: %d",
          static_cast<int>(tResult.size()));

    // Tum dunya: her kayit bir kez
    GeoBounds tWorld;
    tWorld.mSouth = -90.0;
    tWorld.mNorth = 90.0;
    tWorld.mWest = -200.0;
    tWorld.mEast = 300.0;
    tResult.clear();
    tDatabase.query(NavKind::Fix, tWorld, tResult);
    CHECK(tResult.size() == 3, "dunya sorgusu tekrarsiz: %d", static_cast<int>(tResult.size()));

    tDatabase.clear();
    tResult.clear();
    tDatabase.query(NavKind::Fix, tWorld, tResult);
    CHECK(!tDatabase.isReady() && tResult.empty(), "temizlendi");
}

RectD box(double pLeft, double pBottom, double pRight, double pTop)
{
    RectD tBox;
    tBox.mLeft = pLeft;
    tBox.mBottom = pBottom;
    tBox.mRight = pRight;
    tBox.mTop = pTop;
    return tBox;
}

void testLabelPlacer()
{
    LabelPlacer tPlacer;
    tPlacer.reset(box(0, 0, 500, 300));
    CHECK(tPlacer.tryPlace(box(10, 10, 60, 22)), "ilk etiket");
    CHECK(!tPlacer.tryPlace(box(50, 15, 100, 27)), "cakisan reddedilir");
    CHECK(tPlacer.tryPlace(box(60, 10, 110, 22)), "kenar kenara degen kabul");
    CHECK(!tPlacer.tryPlace(box(480, 10, 520, 22)), "alan disina tasan reddedilir");
    CHECK(tPlacer.tryPlace(box(200, 100, 400, 200)), "buyuk kutu (cok hucre)");
    CHECK(!tPlacer.tryPlace(box(250, 150, 260, 160)), "buyuk kutunun icindeki reddedilir");
    CHECK(tPlacer.placedCount() == 3, "3 kutu yerlesti");

    tPlacer.reset(box(-1000, -500, -200, 100));  // eksi koordinatli alan (sol monitor)
    CHECK(tPlacer.placedCount() == 0, "reset bosaltir");
    CHECK(tPlacer.tryPlace(box(-990, -490, -950, -470)) && tPlacer.tryPlace(box(-240, 80, -201, 99)),
          "eksi koordinatlar");

    // Cok sayida kucuk etiket: izgara gibi dizilenler hep kabul
    tPlacer.reset(box(0, 0, 1000, 1000));
    int tPlaced = 0;
    for (int tIndex = 0; tIndex < 400; ++tIndex)
    {
        const double tX = (tIndex % 20) * 50.0;
        const double tY = (tIndex / 20) * 50.0;
        if (tPlacer.tryPlace(box(tX, tY, tX + 40, tY + 12)))
        {
            ++tPlaced;
        }
    }
    CHECK(tPlaced == 400, "400 ayrik etiket: %d", tPlaced);

    // Reserve: sembol yeri kontrolsuz ayrilir, etiketler ustune binemez
    tPlacer.reset(box(0, 0, 500, 300));
    tPlacer.reserve(box(100, 100, 120, 120));
    tPlacer.reserve(box(110, 110, 130, 130));  // cakissa da ayrilir
    CHECK(tPlacer.placedCount() == 2, "iki sembol ayrildi");
    CHECK(!tPlacer.tryPlace(box(115, 90, 160, 102)), "etiket sembolun ustune binmez");
    CHECK(tPlacer.tryPlace(box(131, 110, 170, 122)), "sembolun yanina etiket konur");
    tPlacer.reserve(box(-50, -50, 10, 10));  // alan disina tasan kismi kirpilir
    CHECK(!tPlacer.tryPlace(box(0, 0, 8, 8)), "kirpilmis sembol de korunur");
    tPlacer.reserve(box(600, 600, 700, 700));  // tamamen disarida: yok sayilir
    CHECK(tPlacer.placedCount() == 4, "disaridaki sembol eklenmez: %d", tPlacer.placedCount());
}

void scanText(AptDatScanner& pScanner, std::string_view pText)
{
    size_t tStart = 0;
    while (tStart < pText.size())
    {
        size_t tStop = pText.find('\n', tStart);
        if (tStop == std::string_view::npos)
        {
            tStop = pText.size();
        }
        pScanner.addLine(pText.substr(tStart, tStop - tStart), tStart);
        tStart = tStop + 1;
    }
    pScanner.finish(pText.size());
}

void testAptLine()
{
    AptLine tLine("  100\t29.87 -122.30000000 03R  Boeing Field  King \r");
    int tCode = 0;
    double tWidth = 0.0;
    double tLon = 0.0;
    std::string tName;
    CHECK(tLine.readInt(tCode) && tCode == 100, "satir kodu (bas bosluk, tab)");
    CHECK(!tLine.readInt(tCode), "29.87 tamsayi olarak okunmaz");
    CHECK(tLine.readDouble(tWidth) && near(tWidth, 29.87, 1e-12), "ondalik nokta (yerel ayardan bagimsiz)");
    CHECK(tLine.readDouble(tLon) && near(tLon, -122.3, 1e-12), "eksi sayi");
    CHECK(tLine.token(tName) && tName == "03R", "kelime");
    CHECK(tLine.rest() == "Boeing Field  King" && tLine.atEnd(), "satirin geri kalani (ic bosluklar korunur)");
    CHECK(aptRowCode("# yorum") == -1 && aptRowCode("") == -1 && aptRowCode("1302 city X") == 1302, "satir kodu");

    // Gercek apt.dat satiri (Global Airports, LTAC)
    Runway tRunway;
    CHECK(AptDatParser::parseRunway("100 59.00 1 0 0.25 1 2 0 03R 40.1140623 32.9831635 0 65 3 2 1 0 "
                                    "21L 40.1413103 33.0091255 0 60 3 2 1 0",
                                    &tRunway),
          "pist satiri okunmali");
    CHECK(near(tRunway.mWidthM, 59.0, 1e-9) && tRunway.mSurface == RunwaySurface::Paved && tRunway.mCenterLights &&
              tRunway.mEdgeLights == 2,
          "genislik, yuzey, isiklar");
    CHECK(tRunway.mEnds[0].mName == "03R" && tRunway.mEnds[1].mName == "21L" && tRunway.mEnds[0].mMarkings == 3 &&
              tRunway.mEnds[0].mApproachLights == 2 && tRunway.mEnds[0].mTouchdownLights,
          "pist uclari: numara, isaret, yaklasma isigi, TDZ");
    CHECK(near(tRunway.mEnds[0].mOverrunM, 65.0, 1e-9) && near(tRunway.mEnds[1].mPosition.mLon, 33.0091255, 1e-9),
          "overrun, konum");
    const double tLength = distanceNm(tRunway.mEnds[0].mPosition, tRunway.mEnds[1].mPosition) * 1852.0;
    CHECK(tLength > 3700 && tLength < 3800, "LTAC 03R/21L boyu %.0f m (gercek 3750 m)", tLength);
    CHECK(AptDatParser::parseRunway("100 30 3 0 0.25 0 0 0 09 41.0 29.0 150.5 0 1 0 0 0 27 41.0 29.02 0 0 1 0 0 0\r",
                                    &tRunway) &&
              tRunway.mSurface == RunwaySurface::Unpaved && near(tRunway.mEnds[0].mDisplacedThresholdM, 150.5, 1e-9),
          "cim pist, kayik esik, CRLF");
    CHECK(!AptDatParser::parseRunway("100 30 3 0 0.25 0 0 0 09 41.0", &tRunway), "eksik satir reddedilir");
    CHECK(AptDatParser::parseWaterRunway("101 182.88 0 09 47.900116 -92.577834 27 47.899159 -92.536945", &tRunway) &&
              tRunway.mWater && tRunway.mSurface == RunwaySurface::Water && tRunway.mEnds[1].mName == "27",
          "su pisti (gercek satir)");

    AirportType tType = AirportType::Land;
    int tElevation = 0;
    std::string tId;
    std::string tAirportName;
    CHECK(AptDatParser::parseHeader("1   3125 0 0 LTAC Esenboga Intl \r", &tType, &tElevation, &tId, &tAirportName) &&
              tId == "LTAC" && tAirportName == "Esenboga Intl" && tElevation == 3125,
          "baslik: '%s' '%s'", tId.c_str(), tAirportName.c_str());
    CHECK(AptDatParser::parseHeader("17 -12 0 0 XHEL Heliport", &tType, &tElevation, &tId, &tAirportName) &&
              tType == AirportType::Heliport && tElevation == -12,
          "heliport, deniz seviyesi alti");
}

void testAptDatScanner()
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
    scanText(tScanFirst, tFirst);
    AptDatScanner tScanSecond(1, tSeen, tAirports);
    scanText(tScanSecond, tSecond);
    CHECK(tScanFirst.airportCount() == 2 && tScanSecond.airportCount() == 1 && tAirports.size() == 3,
          "LTAC + heliport, sonra sadece LTFM (eski LTAC atlandi): %d", static_cast<int>(tAirports.size()));
    if (tAirports.size() != 3)
    {
        return;
    }
    const AirportGeometry& tLtac = tAirports[0];
    CHECK(tLtac.mId == "LTAC" && tLtac.mName == "Esenboga" && tLtac.mIcaoCode == "LTAC" && tLtac.mHasTower &&
              tLtac.mRunways.size() == 1 && tLtac.mPavementCount == 1 && tLtac.mNodeCount == 2,
          "LTAC ozeti");
    CHECK(near(tLtac.mReference.mLat, 40.128, 1e-9) && near(tLtac.mReference.mLon, 32.995, 1e-9),
          "referans = datum");
    CHECK(near(tLtac.mBounds.mWest, 32.98, 1e-9) && near(tLtac.mBounds.mEast, 33.05, 1e-9) &&
              near(tLtac.mBounds.mNorth, 40.14, 1e-9),
          "kapsayan kutu pist ve dugumlerden");
    const AirportGeometry& tHeliport = tAirports[1];
    CHECK(tHeliport.mType == AirportType::Heliport && tHeliport.mRunways.empty() &&
              near(tHeliport.mReference.mLat, 40.0, 1e-9),
          "pistsiz heliport da alinir (helipad konumu)");
    CHECK(tAirports[2].mId == "LTFM" && tAirports[2].mLocation.mFile == 1 && tAirports[2].mRunways.size() == 2,
          "son satirda 99 olmasa da havalimani kapanir");

    // Bayt araligi: dosyada tam o havalimaninin metnini gosterir
    const AptFileLocation& tLocation = tLtac.mLocation;
    const std::string tSlice = tFirst.substr(tLocation.mOffset, tLocation.mLength);
    CHECK(tSlice.rfind("1 3125 0 0 LTAC", 0) == 0 && tSlice.find("XHEL") == std::string::npos, "LTAC bayt araligi");
    const AirportDetail tDetail = AptDatParser::parseAirport(tSlice);
    CHECK(tDetail.mId == "LTAC" && tDetail.mPavements.size() == 1 && tDetail.mRunways.size() == 1 &&
              tDetail.mSkippedLineCount == 0,
          "araliktan ayrinti okunur");
}

void testAptIndexCache()
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
    CHECK(AptDatIndexer::saveCache(tPath, "anahtar-1", tIndex), "onbellek yazilir");
    AirportIndex tLoaded;
    CHECK(AptDatIndexer::loadCache(tPath, "anahtar-1", tLoaded), "ayni anahtarla okunur");
    CHECK(tLoaded.mFiles == tIndex.mFiles && tLoaded.mAirports.size() == 1, "dosya ve havalimani sayisi");
    if (tLoaded.mAirports.size() == 1)
    {
        const AirportGeometry& tBack = tLoaded.mAirports[0];
        CHECK(tBack.mId == "LTAC" && tBack.mName == "Esenboga Intl" && tBack.mElevationFt == 3125 && tBack.mHasTower &&
                  near(tBack.mBounds.mEast, 33.02, 1e-12) && tBack.mLocation.mOffset == 123456789012ull &&
                  tBack.mLocation.mLength == 4567 && tBack.mNodeCount == 345,
              "ozet alanlari birebir");
        CHECK(tBack.mRunways.size() == 1 && tBack.mRunways[0].mEnds[1].mName == "21L" &&
                  near(tBack.mRunways[0].mEnds[1].mOverrunM, 60.0, 1e-12) &&
                  tBack.mRunways[0].mEnds[1].mTouchdownLights,
              "pistler birebir");
    }
    AirportIndex tStale;
    CHECK(!AptDatIndexer::loadCache(tPath, "anahtar-2", tStale), "dosyalar degisti (anahtar farkli): reddedilir");
    CHECK(!AptDatIndexer::loadCache("yok_boyle_dosya.bin", "anahtar-1", tStale), "dosya yok: reddedilir");
    std::remove(tPath.c_str());
}

void testAptDatDetail()
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
    const AirportDetail tDetail = AptDatParser::parseAirport(tText);
    CHECK(tDetail.mId == "KBFI" && tDetail.mElevationFt == 21 && tDetail.mSkippedLineCount == 0,
          "baslik, bozuk satir yok (%d)", tDetail.mSkippedLineCount);
    CHECK(tDetail.mRunways.size() == 2 && tDetail.mRunways[1].mWater && tDetail.mHelipads.size() == 1 &&
              near(tDetail.mHelipads[0].mWidthM, 10.06, 1e-9),
          "kara + su pisti, helipad");
    CHECK(tDetail.mLightObjects.size() == 1 && tDetail.mLightObjects[0].mType == 2 &&
              near(tDetail.mLightObjects[0].mGlideslopeDeg, 3.3, 1e-9),
          "PAPI (6 alanli satir)");

    CHECK(tDetail.mPavements.size() == 1 && tDetail.mPavements[0].mName == "A2 Exit" &&
              tDetail.mPavements[0].mRings.size() == 2,
          "yuzey: dis sinir + delik");
    if (tDetail.mPavements.size() == 1 && tDetail.mPavements[0].mRings.size() == 2)
    {
        const AptChain& tOuter = tDetail.mPavements[0].mRings[0];
        CHECK(tOuter.mClosed && tOuter.mNodes.size() == 4 && tOuter.mNodes[2].mHasControl &&
                  near(tOuter.mNodes[2].mControl.mLat, 47.53757385, 1e-12),
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
              near(tDetail.mViewpoints[0].mHeightFt, 242.78, 1e-9) &&
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

void testAirportShapes()
{
    const LocalFrame tFrame({47.0, 8.0});
    const LocalPoint tEast = tFrame.toLocal({47.0, 8.001});
    CHECK(near(tEast.mX, 111320.0 * std::cos(47.0 * 3.14159265358979 / 180.0) * 0.001, 1e-6) &&
              near(tEast.mY, 0.0, 1e-9),
          "yerel duzlem: dogu +x");
    const LatLon tBack = tFrame.toLatLon(tEast);
    CHECK(near(tBack.mLon, 8.001, 1e-12) && near(tBack.mLat, 47.0, 1e-12), "yerel duzlemden geri");

    // Duz parca: sadece baslangic noktasi
    AptNode tA;
    tA.mPosition = {47.0, 8.0};
    AptNode tB;
    tB.mPosition = {47.0, 8.001};
    std::vector<LocalPoint> tPoints;
    Shapes::addSegment(tA, tB, tFrame, 2.0, tPoints);
    CHECK(tPoints.size() == 1, "kontrolsuz parca duz");

    // Egri: kural "kontrol noktasi cikan egriye ait, gelende aynasi"
    tA.mHasControl = true;
    tA.mControl = {47.0003, 8.0};  // A'dan kuzeye cik
    tB.mHasControl = true;
    tB.mControl = {46.9997, 8.001};  // B'ye kuzeyden gel: aynasi (47.0003) kullanilir
    tPoints.clear();
    Shapes::addSegment(tA, tB, tFrame, 2.0, tPoints);
    CHECK(tPoints.size() > 10, "egri parcalara bolundu: %d", static_cast<int>(tPoints.size()));
    const LocalPoint tMiddle = tPoints[tPoints.size() / 2];
    CHECK(tMiddle.mY > 20.0, "egri kuzeye siskin (ayna kurali): y = %.1f m", tMiddle.mY);

    // kare(100 x 100 m) icinde kare delik (40 x 40 m)
    const std::vector<LocalPoint> tOuter = {{0, 0}, {100, 0}, {100, 100}, {0, 100}};
    const std::vector<LocalPoint> tHole = {{30, 30}, {30, 70}, {70, 70}, {70, 30}};
    std::vector<LocalPoint> tVertices;
    std::vector<int> tTriangles;
    auto tTriangleArea = [&]()
    {
        double tArea = 0.0;
        for (size_t tIndex = 0; tIndex + 2 < tTriangles.size(); tIndex += 3)
        {
            tArea += std::fabs(Shapes::signedArea(
                {tVertices[tTriangles[tIndex]], tVertices[tTriangles[tIndex + 1]], tVertices[tTriangles[tIndex + 2]]}));
        }
        return tArea;
    };
    CHECK(Shapes::triangulate(tOuter, {tHole}, tVertices, tTriangles), "delikli kare ucgenlenir");
    CHECK(near(tTriangleArea(), 10000.0 - 1600.0, 1e-6), "alan = dis - delik: %.1f", tTriangleArea());

    // Ters yonlu girdi ve iki delik
    std::vector<LocalPoint> tReversed(tOuter.rbegin(), tOuter.rend());
    const std::vector<LocalPoint> tHole2 = {{5, 5}, {5, 15}, {15, 15}, {15, 5}};
    CHECK(Shapes::triangulate(tReversed, {tHole, tHole2}, tVertices, tTriangles) &&
              near(tTriangleArea(), 10000.0 - 1600.0 - 100.0, 1e-6),
          "ters yon + iki delik: %.1f", tTriangleArea());

    // icbukey(L) sekil, tekrar eden nokta ve dogrusal kose
    const std::vector<LocalPoint> tL = {{0, 0}, {50, 0}, {100, 0}, {100, 40}, {40, 40}, {40, 100}, {40, 100}, {0, 100}};
    CHECK(Shapes::triangulate(tL, {}, tVertices, tTriangles) && near(tTriangleArea(), 100.0 * 40.0 + 40.0 * 60.0, 1e-6),
          "L sekli: %.1f", tTriangleArea());

    CHECK(!Shapes::triangulate({{0, 0}, {10, 0}}, {}, tVertices, tTriangles), "iki nokta ucgenlenmez");

    // GERCEK VERI: kendini kesen sinir (kelebek) cokmeden ucgenlenir
    const std::vector<LocalPoint> tBowtie = {{0, 0}, {100, 100}, {100, 0}, {0, 100}};
    CHECK(Shapes::triangulate(tBowtie, {}, tVertices, tTriangles) && tTriangles.size() % 3 == 0,
          "kendini kesen sinir: %d ucgen", static_cast<int>(tTriangles.size() / 3));

    // Buyuk poligon (Z-duzeni yolu, > 80 nokta): 400 kenarli daire icinde kare delik
    std::vector<LocalPoint> tCircle;
    for (int tStep = 0; tStep < 400; ++tStep)
    {
        const double tAngle = 2.0 * 3.14159265358979 * tStep / 400.0;
        tCircle.push_back({500.0 * std::cos(tAngle), 500.0 * std::sin(tAngle)});
    }
    const std::vector<LocalPoint> tSquareHole = {{-50, -50}, {-50, 50}, {50, 50}, {50, -50}};
    CHECK(Shapes::triangulate(tCircle, {tSquareHole}, tVertices, tTriangles) &&
              near(tTriangleArea(), std::fabs(Shapes::signedArea(tCircle)) - 10000.0, 1e-3),
          "buyuk poligon + delik: %.1f", tTriangleArea());

    // Delik dis sinira degiyor (ortak kose)
    const std::vector<LocalPoint> tTouching = {{0, 0}, {0, 40}, {40, 40}, {40, 0}};
    CHECK(Shapes::triangulate(tOuter, {tTouching}, tVertices, tTriangles) &&
              near(tTriangleArea(), 10000.0 - 1600.0, 1e-6),
          "koseden degen delik: %.1f", tTriangleArea());

    // Mercator: sekillerin kaydirma + olcek ile cizilmesi lat/lon yoluyla ayni yere duser
    XMapView tView;
    RectD tViewport;
    tViewport.mLeft = 0;
    tViewport.mBottom = 0;
    tViewport.mRight = 1000;
    tViewport.mTop = 800;
    tView.setViewport(tViewport);
    tView.setCenter({47.45, -122.31});
    tView.setZoom(15.0);
    const LatLon tReference = {47.449888889, -122.311777778};
    const LatLon tCorner = {47.4600, -122.3185};
    const double tMercX = Mercator::lonToX(tCorner.mLon) - Mercator::lonToX(tReference.mLon);
    const double tMercY = Mercator::latToY(tCorner.mLat) - Mercator::latToY(tReference.mLat);
    const PointF tOffset = tView.mercatorToScreen(Mercator::lonToX(tReference.mLon), Mercator::latToY(tReference.mLat));
    const PointF tDirect = tView.toScreen(tCorner);
    const double tGpuX = tOffset.mX + static_cast<float>(tMercX) * tView.worldSize();
    const double tGpuY = tOffset.mY + static_cast<float>(tMercY) * tView.worldSize();
    CHECK(near(tGpuX, tDirect.mX, 0.05) && near(tGpuY, tDirect.mY, 0.05),
          "GPU donusumu = toScreen(float fark): %.3f %.3f / %.3f %.3f", tGpuX, tGpuY, tDirect.mX, tDirect.mY);

    // Ayrintidan sekiller: yuzey + kenar + cizgi
    const AirportDetail tDetail = AptDatParser::parseAirport("1 0 0 0 TEST Test\n"
                                                             "110 1 0.25 0 Apron\n"
                                                             "111 47.000 8.000 1\n"
                                                             "111 47.000 8.002\n"
                                                             "111 47.001 8.002\n"
                                                             "113 47.001 8.000\n"
                                                             "110 3 0.25 0 Grass\n"
                                                             "111 47.002 8.000\n"
                                                             "111 47.002 8.001\n"
                                                             "113 47.003 8.001\n");
    const AirportShapes tShapes = Shapes::build(tDetail, {47.0, 8.0});
    CHECK(tShapes.mPavements.size() == 2 && tShapes.mFailedPolygons == 0 && tShapes.mPavements[0].mSurfaceCode == 3,
          "iki yuzey; cizim sirasi dosyanin tersi (once cim)");
    CHECK(tShapes.mPavementEdges.size() == 2 && tShapes.mLines.size() == 1 && tShapes.mLines[0].mLineTypes[0] == 1 &&
              tShapes.mLines[0].mLineTypes[1] == 0,
          "kenar cizgisi stili ilk parcada");
}

class FakeAirportSource : public IAirportGeometrySource
{
public:
    const char* name() const override
    {
        return "sahte kaynak";
    }

    bool loadAll(const std::string& pXPlaneDirectory, const std::atomic<bool>& /*pCancel*/,
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

void testAirportGeometryStore()
{
    AirportGeometryStore tStore(std::make_unique<FakeAirportSource>());
    CHECK(!tStore.isReady() && !tStore.isLoading(), "baslangicta bos");
    tStore.startLoading("C:\\X-Plane 11\\");
    CHECK(tStore.isLoading(), "yukleniyor");

    bool tDone = false;
    waitUntil([&]
    {
        tDone = tStore.poll();
        return tDone;
    });
    CHECK(tDone && tStore.isReady() && !tStore.isLoading() && tStore.lastLoadSucceeded(), "yukleme bitti");
    CHECK(!tStore.poll(), "Poll bitisi bir kez bildirir");
    CHECK(tStore.count() == 2 && tStore.runwayCount() == 2, "2 havalimani, 2 pist");
    CHECK(tStore.lastReport() == "klasor: C:\\X-Plane 11\\", "rapor ana thread'e gelir");
    CHECK(tStore.find("KSEA") && tStore.find("KSEA")->mId == "KSEA" && !tStore.find("YOK1"), "kimlikle bulma");

    GeoBounds tAnkara;
    tAnkara.mSouth = 39.5;
    tAnkara.mNorth = 40.5;
    tAnkara.mWest = 32.5;
    tAnkara.mEast = 33.5;
    std::vector<int> tResult;
    tStore.query(tAnkara, tResult);
    CHECK(tResult.size() == 1 && tStore.at(tResult[0]).mId == "LTAC", "Ankara sorgusu");
}

void testDrawingsFile()
{
    Drawings tModel;
    tModel.setMode(Drawings::Mode::Point);
    tModel.addVertex({41.123456789, 29.987654321});
    tModel.setMode(Drawings::Mode::Route);
    tModel.addVertex({41.0, 29.0});
    tModel.addVertex({40.5, 30.5});
    tModel.addVertex({40.0, 32.0});
    tModel.finish();
    tModel.setMode(Drawings::Mode::Region);
    tModel.addVertex({0.0, 0.0});
    tModel.addVertex({0.0, 1.0});
    tModel.addVertex({1.0, 1.0});
    tModel.addVertex({1.0, 0.0});
    tModel.finish();
    tModel.addVertex({5.0, 5.0});  // yarim kalan bolge: kaydedilmez
    CHECK(Drawings::labelOf(tModel.shapes()[0]) == "P1" && Drawings::labelOf(tModel.shapes()[1]) == "R1" &&
              Drawings::labelOf(tModel.shapes()[2]) == "A1",
          "etiketler P1 R1 A1");

    const std::string tText = tModel.serialize();
    Drawings tLoaded;
    CHECK(tLoaded.deserialize(tText) == 3, "3 sekil geri okunur");
    CHECK(!tLoaded.current(), "yarim cizim kaydedilmez");
    CHECK(tLoaded.shapes()[1].mPoints.size() == 3 && near(tLoaded.shapes()[1].mPoints[2].mLon, 32.0, 1e-9),
          "rota koseleri");
    CHECK(near(tLoaded.shapes()[0].mPoints[0].mLat, 41.123456789, 1e-8), "hassasiyet korunur");
    CHECK(tLoaded.shapes()[2].mTriangles.size() == 6, "bolge yeniden ucgenlenir");

    // Numaralar devam eder: yeni nokta P2 olmali
    tLoaded.setMode(Drawings::Mode::Point);
    tLoaded.addVertex({1.0, 1.0});
    CHECK(tLoaded.shapes().back().mNumber == 2, "numara kaldigi yerden devam eder");

    CHECK(tLoaded.deserialize("# yorum\nX 1 2 3\nP 0 1 1\nR 4 1 1\nP 7 41 29\n") == 1 &&
              tLoaded.shapes()[0].mNumber == 7,
          "bozuk satirlar atlanir");

    // Alan: ekvatorda 1x1 derece ~ 60x60 NM; 60 derecede yarisi
    CHECK(near(polygonAreaNm2({{0, 0}, {0, 1}, {1, 1}, {1, 0}}), 3600.0, 1.0), "ekvator alani %.1f",
          polygonAreaNm2({{0, 0}, {0, 1}, {1, 1}, {1, 0}}));
    CHECK(near(polygonAreaNm2({{59.5, 0}, {59.5, 1}, {60.5, 1}, {60.5, 0}}), 1800.0, 5.0), "60 derece alani");
    CHECK(polygonAreaNm2({{0, 0}, {0, 1}}) == 0.0, "iki nokta alansiz");
}

void testAirways()
{
    // Gercek earth_awy.dat satiri
    AirwayLine tLine;
    CHECK(AirwayDatabase::parseLine("ABKAL LT 11 ULSAB LT 11 N 1 195 285 G8-L614", &tLine), "satir okunur");
    CHECK(tLine.mFromId == "ABKAL" && tLine.mFromType == 11 && tLine.mToId == "ULSAB" && tLine.mDirection == 'N' &&
              tLine.mLevel == 1 && tLine.mBaseFl == 195 && tLine.mTopFl == 285 && tLine.mNames == "G8-L614",
          "alanlar");
    CHECK(AirwayDatabase::parseLine(" ZZV K5  3 SABGE K5 11 F 2  30 179 V214", &tLine) && tLine.mFromType == 3 &&
              tLine.mDirection == 'F' && tLine.mLevel == 2,
          "bosluklu satir, VOR ucu, tek yon");
    CHECK(!AirwayDatabase::parseLine("99", &tLine) && !AirwayDatabase::parseLine("I", &tLine) &&
              !AirwayDatabase::parseLine("1100 Version - data cycle 1802", &tLine),
          "baslik ve son satirlar atlanir");

    // Uc nokta cozme: ayni kimlikte iki fix var; diger uca yakin olan secilmeli
    NavDatabase tNav;
    tNav.add(makeRecord(NavKind::Fix, "ABKAL", 38.0f, 41.5f));
    tNav.add(makeRecord(NavKind::Fix, "ULSAB", 38.5f, 42.0f));
    tNav.add(makeRecord(NavKind::Fix, "ULSAB", -30.0f, 150.0f));  // ayni ad, dunyanin obur ucu
    tNav.add(makeRecord(NavKind::Vor, "ZZV", 40.0f, -82.0f));
    tNav.add(makeRecord(NavKind::Fix, "SABGE", 40.5f, -81.0f));
    tNav.build();

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
    tAirways.startLoading({"no_such_awy.dat", tPath}, tNav);
    waitUntil([&]
    {
        return tAirways.poll();
    });
    std::remove(tPath);
    CHECK(tAirways.isReady() && tAirways.count() == 2, "2 segment cozuldu: %d", tAirways.count());
    CHECK(tAirways.lastReport().find("1 unresolved") != std::string::npos, "rapor: '%s'",
          tAirways.lastReport().c_str());
    if (tAirways.count() == 2)
    {
        CHECK(near(tAirways.at(0).mTo.mLat, 38.5, 1e-5) && !tAirways.at(0).mHigh, "yakin ULSAB secildi, alcak");
        CHECK(tAirways.at(1).mHigh && tAirways.at(1).mDirection == 'F', "yuksek, tek yon");
    }

    // Sorgu: segmentin ortasindan gecen bir alan (uclar disarida) yine bulur
    GeoBounds tMiddle;
    tMiddle.mSouth = 40.1;
    tMiddle.mNorth = 40.4;
    tMiddle.mWest = -81.7;
    tMiddle.mEast = -81.3;
    std::vector<int> tResult;
    tAirways.query(tMiddle, tResult);
    CHECK(contains(tResult, 1), "uclari ekran disinda olan segment de bulunur");
}

void testLabelPriority()
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
        void draw(const drawContext& /*pContext*/) override
        {
            *mTrace += "D" + id() + " ";
        }
        void drawLabels(const drawContext& /*pContext*/) override
        {
            *mTrace += "L" + id() + " ";
        }
        int labelPriority() const override
        {
            return mPriority;
        }

    private:
        int mPriority;
        std::string* mTrace;
    };
    LayerManager tLayers;
    tLayers.add(std::make_unique<PriorityLayer>("awy", 70, &tTrace));
    tLayers.add(std::make_unique<PriorityLayer>("apt", 10, &tTrace));
    tLayers.add(std::make_unique<PriorityLayer>("fix", 60, &tTrace));
    drawContext tContext;
    tLayers.drawAll(tContext);
    CHECK(tTrace == "Dawy Dapt Dfix Lapt Lfix Lawy ", "geometri liste sirasi, yazi oncelik sirasi: '%s'",
          tTrace.c_str());
}

// Sentetik DSF (kucuk-endian bayt dizisi)

void putU8(std::vector<uint8_t>& pOut, int pValue)
{
    pOut.push_back(static_cast<uint8_t>(pValue));
}

void putU16(std::vector<uint8_t>& pOut, int pValue)
{
    pOut.push_back(static_cast<uint8_t>(pValue & 0xFF));
    pOut.push_back(static_cast<uint8_t>((pValue >> 8) & 0xFF));
}

void putU32(std::vector<uint8_t>& pOut, uint32_t pValue)
{
    for (int tShift = 0; tShift < 32; tShift += 8)
    {
        pOut.push_back(static_cast<uint8_t>((pValue >> tShift) & 0xFF));
    }
}

void putF32(std::vector<uint8_t>& pOut, float pValue)
{
    uint32_t tBits = 0;
    std::memcpy(&tBits, &pValue, sizeof(tBits));
    putU32(pOut, tBits);
}

// Atom: kimlik harfleri dosyada ters ("HEAD" -> "DAEH"), boyut basligi icerir
std::vector<uint8_t> atom(const char* pId, const std::vector<uint8_t>& pPayload)
{
    std::vector<uint8_t> tAtom;
    putU32(tAtom, (static_cast<uint32_t>(pId[0]) << 24) | (static_cast<uint32_t>(pId[1]) << 16) |
                      (static_cast<uint32_t>(pId[2]) << 8) | static_cast<uint32_t>(pId[3]));
    putU32(tAtom, static_cast<uint32_t>(pPayload.size() + 8));
    tAtom.insert(tAtom.end(), pPayload.begin(), pPayload.end());
    return tAtom;
}

std::vector<uint8_t> strings(const std::vector<std::string>& pStrings)
{
    std::vector<uint8_t> tOut;
    for (const std::string& tString : pStrings)
    {
        tOut.insert(tOut.end(), tString.begin(), tString.end());
        tOut.push_back(0);
    }
    return tOut;
}

void append(std::vector<uint8_t>& pOut, const std::vector<uint8_t>& pMore)
{
    pOut.insert(pOut.end(), pMore.begin(), pMore.end());
}

// DSF raster katmani: DEMI (surum 1, 2 bayt, bayraklar, boyut, olcek 1, kaydirma 0) + DEMD (int16, satir 0 = guney)
void appendTestRaster(std::vector<uint8_t>& pDems, int pFlags, int pSize, const std::vector<int>& pValues)
{
    std::vector<uint8_t> tInfo;
    putU8(tInfo, 1);
    putU8(tInfo, 2);
    putU16(tInfo, pFlags);
    putU32(tInfo, static_cast<uint32_t>(pSize));
    putU32(tInfo, static_cast<uint32_t>(pSize));
    putF32(tInfo, 1.0f);
    putF32(tInfo, 0.0f);
    std::vector<uint8_t> tData;
    for (const int tValue : pValues)
    {
        putU16(tData, tValue & 0xFFFF);
    }
    append(pDems, atom("DEMI", tInfo));
    append(pDems, atom("DEMD", tData));
}

// 1x1 derece karo (+41+029): sol yari su, sag yari orman (4 nokta, 2 ucgen).
// pWithDem: 5 x 5 "elevation" (post-centric, yukseklik = 10 * sutun + satir) ve 4 x 4 "sea_level" (-50)
std::vector<uint8_t> makeTestDsf(bool pOverlay, bool pWithDem = false)
{
    std::vector<uint8_t> tProperties = strings({"sim/west", "29", "sim/east", "30", "sim/south", "41", "sim/north",
                                                "42", "sim/overlay", pOverlay ? "1" : "0"});
    std::vector<uint8_t> tTerrains = strings({"terrain_Water", "lib/g10/terrain10/coni_tmp_wet_hill.ter"});

    // Havuz: 6 nokta, 5 duzlem (lon, lat, yukseklik, normal x, normal z).
    // Noktalar: (29,41) (29.5,41) (29.5,42) (29,42) (30,41) (30,42)
    std::vector<uint8_t> tPool;
    putU32(tPool, 6);
    putU8(tPool, 5);
    // Duzlem 0 (boylam): RLE + delta (kodlama 3): farklar 0, 32767, 0, -32767, 65535, 0
    putU8(tPool, 3);
    putU8(tPool, 0x80 | 1);  // 1 kez tekrar: 0
    putU16(tPool, 0);
    putU8(tPool, 5);          // 5 deger oldugu gibi
    putU16(tPool, 32767);     // 32767
    putU16(tPool, 0);         // 32767
    putU16(tPool, 32769);     // 32767 + 32769 = 65536 -> 0 (16 bit tasma)
    putU16(tPool, 65535);     // 65535
    putU16(tPool, 0);         // 65535
    // Duzlem 1 (enlem): ham (kodlama 0)
    putU8(tPool, 0);
    for (int tValue : {0, 0, 65535, 65535, 0, 65535})
    {
        putU16(tPool, tValue);
    }
    // Duzlem 2..4: RLE (kodlama 2): hepsi 0
    for (int tPlane = 2; tPlane < 5; ++tPlane)
    {
        putU8(tPool, 2);
        putU8(tPool, 0x80 | 6);
        putU16(tPool, 0);
    }
    std::vector<uint8_t> tScale;
    putF32(tScale, 1.0f);
    putF32(tScale, 29.0f);  // boylam = 29 + ham / 65535
    putF32(tScale, 1.0f);
    putF32(tScale, 41.0f);
    for (int tPlane = 2; tPlane < 5; ++tPlane)
    {
        putF32(tScale, 0.0f);
        putF32(tScale, 0.0f);
    }

    std::vector<uint8_t> tCommands;
    putU8(tCommands, 2);  // junction offset (atlanir)
    putU32(tCommands, 0);
    putU8(tCommands, 15);  // nested polygon range: param, sayac 2, 3 indeks (sayac + 1)
    putU16(tCommands, 0);
    putU8(tCommands, 2);
    putU16(tCommands, 0);
    putU16(tCommands, 2);
    putU16(tCommands, 4);
    putU8(tCommands, 32);  // yorum
    putU8(tCommands, 3);
    putU8(tCommands, 'a');
    putU8(tCommands, 'b');
    putU8(tCommands, 'c');
    putU8(tCommands, 1);  // havuz 0
    putU16(tCommands, 0);
    putU8(tCommands, 3);  // tanim 0 (su)
    putU8(tCommands, 0);
    putU8(tCommands, 18);  // patch: bayrak 1 (fiziksel), LOD 0..-1
    putU8(tCommands, 1);
    putF32(tCommands, 0.0f);
    putF32(tCommands, -1.0f);
    putU8(tCommands, 23);  // ucgen: 0 1 2 ve 0 2 3 (sol yari)
    putU8(tCommands, 6);
    for (int tIndex : {0, 1, 2, 0, 2, 3})
    {
        putU16(tCommands, tIndex);
    }
    putU8(tCommands, 3);  // tanim 1 (orman)
    putU8(tCommands, 1);
    putU8(tCommands, 16);  // yeni patch (ayni bayrak)
    putU8(tCommands, 26);  // serit: 1 4 2 5 -> (1,4,2) (4,2,5): sag yari
    putU8(tCommands, 4);
    for (int tIndex : {1, 4, 2, 5})
    {
        putU16(tCommands, tIndex);
    }
    putU8(tCommands, 17);  // overlay patch: ucgen sayilir ama overlay bayragi tasir
    putU8(tCommands, 3);
    putU8(tCommands, 25);  // ucgen araligi 0..3
    putU16(tCommands, 0);
    putU16(tCommands, 3);

    std::vector<uint8_t> tDefinitions = atom("TERT", tTerrains);
    std::vector<uint8_t> tGeometry = atom("POOL", tPool);
    append(tGeometry, atom("SCAL", tScale));
    std::vector<uint8_t> tDems;
    if (pWithDem)
    {
        append(tDefinitions, atom("DEMN", strings({"elevation", "sea_level"})));
        std::vector<int> tHeights;
        for (int tRow = 0; tRow < 5; ++tRow)
        {
            for (int tColumn = 0; tColumn < 5; ++tColumn)
            {
                tHeights.push_back(10 * tColumn + tRow);
            }
        }
        appendTestRaster(tDems, 1 | 4, 5, tHeights);  // isaretli tamsayi + post-centric
        appendTestRaster(tDems, 1, 4, std::vector<int>(16, -50));
    }

    std::vector<uint8_t> tFile = {'X', 'P', 'L', 'N', 'E', 'D', 'S', 'F'};
    putU32(tFile, 1);
    append(tFile, atom("HEAD", atom("PROP", tProperties)));
    append(tFile, atom("DEFN", tDefinitions));
    append(tFile, atom("GEOD", tGeometry));
    if (pWithDem)
    {
        append(tFile, atom("DEMS", tDems));
    }
    append(tFile, atom("CMDS", tCommands));
    tFile.resize(tFile.size() + 16, 0);  // MD5 yeri (okuyucu dogrulamaz)
    return tFile;
}

void testDsfReader()
{
    const std::vector<uint8_t> tData = makeTestDsf(false);
    CHECK(!SevenZip::isArchive(tData.data(), tData.size()), "duz DSF 7z degil");
    const uint8_t tSevenZip[6] = {'7', 'z', 0xBC, 0xAF, 0x27, 0x1C};
    CHECK(SevenZip::isArchive(tSevenZip, 6), "7z imzasi");

    DsfReader tReader;
    std::string tError;
    CHECK(tReader.open(tData, &tError), "DSF acilir: %s", tError.c_str());
    const DsfInfo& tInfo = tReader.info();
    CHECK(near(tInfo.mWest, 29, 1e-9) && near(tInfo.mNorth, 42, 1e-9) && !tInfo.mOverlay, "ozellikler");
    CHECK(tInfo.mTerrainNames.size() == 2 && tInfo.mTerrainNames[0] == "terrain_Water", "arazi adlari");
    CHECK(tReader.poolCount() == 1, "bir havuz");

    std::vector<DsfTriangle> tTriangles;
    CHECK(tReader.forEachTriangle(
              [&](const DsfTriangle& pTriangle)
              {
                  tTriangles.push_back(pTriangle);
              },
              &tError),
          "komut akisi okunur: %s", tError.c_str());
    CHECK(tTriangles.size() == 5, "5 ucgen (2 su + 2 serit + 1 overlay): %d", static_cast<int>(tTriangles.size()));
    if (tTriangles.size() == 5)
    {
        CHECK(tTriangles[0].mTerrain == 0 && near(tTriangles[0].mLon[1], 29.0 + 32767.0 / 65535.0, 1e-9) &&
                  near(tTriangles[0].mLat[2], 42.0, 1e-9),
              "RLE + delta cozuldu, olcek uygulandi");
        CHECK(tTriangles[2].mTerrain == 1 && near(tTriangles[2].mLon[1], 30.0, 1e-9), "serit ucgeni sag yarida");
        CHECK((tTriangles[4].mPatchFlags & DsfReader::kPatchFlagOverlay) != 0 &&
                  (tTriangles[0].mPatchFlags & DsfReader::kPatchFlagOverlay) == 0,
              "overlay bayragi");
    }

    // Bozuk akis: bilinmeyen komut
    std::vector<uint8_t> tBroken = tData;
    DsfReader tBrokenReader;
    CHECK(tBrokenReader.open(tBroken, &tError), "bozuk akisli dosya acilir");
    std::vector<uint8_t> tBadCommands = atom("CMDS", {200});
    tBroken.resize(tBroken.size() - 16);
    append(tBroken, tBadCommands);
    tBroken.resize(tBroken.size() + 16, 0);
    CHECK(tBrokenReader.open(tBroken, &tError), "ikinci CMDS");
    CHECK(!tBrokenReader.forEachTriangle(
              [](const DsfTriangle&)
              {
              },
              &tError) &&
              tError.find("unknown") != std::string::npos,
          "bilinmeyen komut hata verir: %s", tError.c_str());

    std::vector<uint8_t> tNotDsf = {'N', 'O', 'T'};
    CHECK(!tReader.open(tNotDsf, &tError), "DSF olmayan reddedilir");
}

// Overlay envanteri: her komut turunden biri; arada atlanmasi gereken ucgen / yorum
// komutlari var (bir komutun uzunlugu yanlis okunursa sonraki sayimlar kayar)
void testDsfCommandCount()
{
    std::vector<uint8_t> tDefinitions = atom("TERT", strings({"terrain_Water"}));
    append(tDefinitions, atom("OBJT", strings({"tower.obj", "house.obj"})));
    append(tDefinitions, atom("POLY", strings({"lib/g8/coni_tmp_wet.for", "lib/g10/autogen/natural.ags"})));
    append(tDefinitions, atom("NETW", strings({"lib/g10/roads.net"})));

    std::vector<uint8_t> tCommands;
    putU8(tCommands, 3);  // tanim 1 (house.obj)
    putU8(tCommands, 1);
    putU8(tCommands, 7);  // nesne: 1 tane
    putU16(tCommands, 5);
    putU8(tCommands, 8);  // nesne araligi 10..14: 4 tane
    putU16(tCommands, 10);
    putU16(tCommands, 14);
    putU8(tCommands, 3);  // tanim 0: orman poligonu
    putU8(tCommands, 0);
    putU8(tCommands, 12);  // poligon: param, 3 kose
    putU16(tCommands, 255);
    putU8(tCommands, 3);
    for (int tIndex : {0, 1, 2})
    {
        putU16(tCommands, tIndex);
    }
    putU8(tCommands, 13);  // poligon araligi: param, 0..5 (5 kose)
    putU16(tCommands, 255);
    putU16(tCommands, 0);
    putU16(tCommands, 5);
    putU8(tCommands, 23);  // ucgen (sayilmaz, atlanir): 3 indeks
    putU8(tCommands, 3);
    for (int tIndex : {0, 1, 2})
    {
        putU16(tCommands, tIndex);
    }
    putU8(tCommands, 4);  // tanim 1 (16 bit): autogen
    putU16(tCommands, 1);
    putU8(tCommands, 14);  // ic ice poligon: 2 halka (4 + 3 kose)
    putU16(tCommands, 0);
    putU8(tCommands, 2);
    putU8(tCommands, 4);
    for (int tIndex : {0, 1, 2, 3})
    {
        putU16(tCommands, tIndex);
    }
    putU8(tCommands, 3);
    for (int tIndex : {4, 5, 6})
    {
        putU16(tCommands, tIndex);
    }
    putU8(tCommands, 15);  // ic ice poligon araligi: sayac 2 -> 3 indeks (0, 4, 7): 2 halka, 7 kose
    putU16(tCommands, 0);
    putU8(tCommands, 2);
    for (int tIndex : {0, 4, 7})
    {
        putU16(tCommands, tIndex);
    }
    putU8(tCommands, 33);  // yorum 16: 2 bayt
    putU16(tCommands, 2);
    putU8(tCommands, 'x');
    putU8(tCommands, 'y');
    putU8(tCommands, 3);  // tanim 0: yol agi
    putU8(tCommands, 0);
    putU8(tCommands, 6);  // alt tur 50 (local)
    putU8(tCommands, 50);
    putU8(tCommands, 9);  // zincir: 3 nokta
    putU8(tCommands, 3);
    for (int tIndex : {0, 1, 2})
    {
        putU16(tCommands, tIndex);
    }
    putU8(tCommands, 10);  // zincir araligi 2..6: 4 nokta
    putU16(tCommands, 2);
    putU16(tCommands, 6);
    putU8(tCommands, 6);  // alt tur 151 (demiryolu)
    putU8(tCommands, 151);
    putU8(tCommands, 11);  // 32 bitlik zincir: 2 nokta
    putU8(tCommands, 2);
    putU32(tCommands, 70000);
    putU32(tCommands, 70001);
    putU8(tCommands, 16);  // patch (sayilir)

    std::vector<uint8_t> tFile = {'X', 'P', 'L', 'N', 'E', 'D', 'S', 'F'};
    putU32(tFile, 1);
    append(tFile, atom("HEAD", atom("PROP", strings({"sim/west", "29", "sim/overlay", "1"}))));
    append(tFile, atom("DEFN", tDefinitions));
    append(tFile, atom("CMDS", tCommands));
    tFile.resize(tFile.size() + 16, 0);

    DsfReader tReader;
    std::string tError;
    CHECK(tReader.open(tFile, &tError), "envanter DSF'i acilir: %s", tError.c_str());
    const DsfInfo& tInfo = tReader.info();
    CHECK(tInfo.mObjectNames.size() == 2 && tInfo.mPolygonNames.size() == 2 && tInfo.mNetworkNames.size() == 1 &&
              tInfo.mPolygonNames[1] == "lib/g10/autogen/natural.ags",
          "OBJT / POLY / NETW tablolari okunur");

    DsfCommandStatistics tStatistics;
    CHECK(tReader.countCommands(&tStatistics, &tError), "komutlar sayilir: %s", tError.c_str());
    CHECK(tStatistics.mObjects.size() == 2 && tStatistics.mObjects[0] == 0 && tStatistics.mObjects[1] == 5,
          "nesne: 1 + aralik 4 = 5");
    if (tStatistics.mPolygons.size() == 2)
    {
        const DsfCommandStatistics::PolygonCount& tForest = tStatistics.mPolygons[0];
        const DsfCommandStatistics::PolygonCount& tAutogen = tStatistics.mPolygons[1];
        CHECK(tForest.mPolygons == 2 && tForest.mWindings == 2 && tForest.mPoints == 8,
              "orman: 2 poligon, 3 + 5 kose (%lld %lld %lld)", tForest.mPolygons, tForest.mWindings,
              tForest.mPoints);
        CHECK(tAutogen.mPolygons == 2 && tAutogen.mWindings == 4 && tAutogen.mPoints == 14,
              "ic ice: 2 + 2 halka, 7 + 7 kose (%lld %lld %lld)", tAutogen.mPolygons, tAutogen.mWindings,
              tAutogen.mPoints);
    }
    else
    {
        CHECK(false, "iki poligon tanimi sayilmali");
    }
    long long tLocalChains = 0;
    long long tLocalPoints = 0;
    long long tRailPoints = 0;
    for (const auto& tEntry : tStatistics.mChains)
    {
        if (tEntry.first == std::make_pair(0, 50))
        {
            tLocalChains = tEntry.second.mChains;
            tLocalPoints = tEntry.second.mPoints;
        }
        if (tEntry.first == std::make_pair(0, 151))
        {
            tRailPoints = tEntry.second.mPoints;
        }
    }
    CHECK(tLocalChains == 2 && tLocalPoints == 7, "yol: 2 zincir, 3 + 4 nokta");
    CHECK(tRailPoints == 2, "demiryolu (alt tur 151): 32 bitlik zincir 2 nokta");
    CHECK(tStatistics.mTerrainPatches == 1, "patch sayisi");
}

void testTerrain()
{
    // siniflandirma(gercek XP11 adlari)
    CHECK(classifyTerrain("terrain_Water") == TerrainClass::Water, "su");
    CHECK(classifyTerrain("lib/g10/terrain10/coni_tmp_wet_hill.ter") == TerrainClass::ForestConifer, "igne yaprak");
    CHECK(classifyTerrain("lib/g10/terrain10/urban_tmp_wet_sflat.ter") == TerrainClass::Urban, "sehir");
    CHECK(classifyTerrain("lib/g10/terrain10/sq_grass_tmp_wet_fl.ter") == TerrainClass::Grass, "sq_ on eki");
    CHECK(classifyTerrain("lib/g10/terrain10/sq_tmp_crptwnhill.ter") == TerrainClass::Crop, "crptwn");
    CHECK(classifyTerrain("lib/g10/terrain10/sp_tmp_wet_fl.ter") == TerrainClass::Grass &&
              classifyTerrain("lib/g10/terrain10/spr_tmp_wet_fl.ter") == TerrainClass::Grass,
          "sp / spr");
    CHECK(classifyTerrain("lib/g10/terrain10/snow_tmp.ter") == TerrainClass::SnowIce, "kar");
    CHECK(classifyTerrain("terrain/ortho_+47-123_16.ter") == TerrainClass::Land, "taninmayan: kara");

    // Renk dosyasi gidis-donus
    TerrainPalette tPalette;
    Settings tColors;
    tPalette.save(tColors);
    tColors.setString("palette", "source", "colors");  // Asama 3: varsayilan kaynak X-Plane dokulari
    tColors.setString("colors", "water", "10 20 30");
    tColors.setDouble("shading", "strength", 2.0);  // sinira kirpilmali
    TerrainPalette tLoaded;
    tLoaded.load(tColors);
    CHECK(tLoaded.colorOf(TerrainClass::Water).mR == 10 && tLoaded.colorOf(TerrainClass::Water).mB == 30,
          "renk okunur");
    CHECK(near(tLoaded.shadingStrength(), 1.0, 1e-12), "golge gucu 0..1");

    // Sentetik DSF'ten raster: sol yari su, sag yari orman
    const std::vector<uint8_t> tData = makeTestDsf(false);
    DsfReader tReader;
    std::string tError;
    tReader.open(tData, &tError);
    TerrainRaster tRaster;
    tRaster.reset(29, 41);
    tReader.forEachTriangle(
        [&](const DsfTriangle& pTriangle)
        {
            if ((pTriangle.mPatchFlags & DsfReader::kPatchFlagOverlay) == 0)
            {
                tRaster.fillTriangle(pTriangle.mLon, pTriangle.mLat,
                                     classifyTerrain(tReader.info().mTerrainNames[pTriangle.mTerrain]));
            }
        },
        &tError);
    const int tHalf = TerrainRaster::kSize / 2;
    CHECK(tRaster.classAt(100, 100) == TerrainClass::Water && tRaster.classAt(tHalf + 100, 100) ==
                                                                   TerrainClass::ForestConifer,
          "sol su, sag orman");
    const int tWater = tRaster.countOf(TerrainClass::Water);
    const int tTotal = TerrainRaster::kSize * TerrainRaster::kSize;
    CHECK(tWater > tTotal * 0.49 && tWater < tTotal * 0.51, "su yari yariya: %d / %d", tWater, tTotal);
    CHECK(tRaster.countOf(TerrainClass::None) == 0, "bosluk yok (ucgenler karoyu tam kapliyor)");

    // Goruntu: 64 px, sol ust su rengi, saydam degil
    std::vector<uint8_t> tImage;
    tRaster.buildImage(0, 0, TerrainRaster::kSize, TerrainRaster::kSize, 64, tPalette, TerrainImageStyle(), &tImage);
    CHECK(tImage.size() == 64 * 64 * 4 && tImage[3] == 255 &&
              tImage[0] == tPalette.colorOf(TerrainClass::Water).mR,
          "goruntu: su pikseli");

    // Seviye 4: DSF ucgenlerinden boyanan 1/32 derecelik parcalar
    TerrainTriangles tTriangles;
    tTriangles.reset(29, 41);
    tReader.forEachTriangle(
        [&](const DsfTriangle& pTriangle)
        {
            if ((pTriangle.mPatchFlags & DsfReader::kPatchFlagOverlay) == 0)
            {
                tTriangles.add(pTriangle.mLon, pTriangle.mLat,
                               classifyTerrain(tReader.info().mTerrainNames[pTriangle.mTerrain]));
            }
        },
        &tError);
    const int tDetailChunks = TerrainTriangles::kChunksPerSide;
    std::vector<uint8_t> tWaterChunk;
    std::vector<uint8_t> tForestChunk;
    const int tSize = 64;
    tRaster.buildDetailImage(tTriangles, tDetailChunks, 5 * tDetailChunks + 15, tSize, tPalette, TerrainImageStyle(),
                             &tWaterChunk);
    tRaster.buildDetailImage(tTriangles, tDetailChunks, 5 * tDetailChunks + 16, tSize, tPalette, TerrainImageStyle(),
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
    // orman parcasinin 0. sutunu tam kiyida (kiyi cizgisi bilerek koyulastirir): kiyidan uzak sutun 10
    CHECK(tAllOpaque && tWaterChunk.size() == static_cast<size_t>(tSize) * tSize * 4 &&
              tWaterChunk[0] == tPalette.colorOf(TerrainClass::Water).mR &&
              tForestChunk[10 * 4] == tPalette.colorOf(TerrainClass::ForestConifer).mR,
          "seviye 4 olculeri");
    // Kiyi cizgisi suyun tarafinda: su parcasinin son sutunu (orman parcasina komsu) su renginden koyu
    CHECK(tWaterChunk[63 * 4 + 2] < tPalette.colorOf(TerrainClass::Water).mB - 5 &&
              tForestChunk[0] == tForestChunk[10 * 4],
          "kiyi cizgisi suyun son pikselinde, kara pikseli etkilenmez");
    TerrainImageKeyHash tHash;
    CHECK(tHash({29, 41, 4, 17, 0}) != tHash({29, 41, 3, 33, 0}) &&
              tHash({29, 41, 4, 1023, 3}) != tHash({29, 41, 4, 1023, 2}) &&
              tHash({29, 41, 5, 16383, 0}) != tHash({29, 41, 5, 16382, 0}) &&
              tHash({29, 41, 5, 16383, 0}) != tHash({29, 42, 5, 16383, 0}),
          "anahtar ozeti carpismaz (seviye 5 dahil)");

    // Seviye 5: 4 kat ince parca. Sol ust ince parca (satir 20, sutun 60) seviye 4'teki
    // (satir 5, sutun 15) parcanin sol ust ceyregi: ikisinde de ayni yer ayni renk.
    CHECK(TerrainStore::chunksPerSide(TerrainStore::kFineLevel) == 4 * TerrainTriangles::kChunksPerSide &&
              TerrainStore::imageSize(TerrainStore::kFineLevel) == 1024,
          "seviye 5 olculeri");
    std::vector<uint8_t> tFineChunk;
    const int tFineChunks = TerrainStore::chunksPerSide(TerrainStore::kFineLevel);
    tRaster.buildDetailImage(tTriangles, tFineChunks, 20 * tFineChunks + 60, tSize, tPalette, TerrainImageStyle(),
                             &tFineChunk);
    CHECK(tFineChunk.size() == tWaterChunk.size() && tFineChunk[3] == 255 && tFineChunk[0] == tWaterChunk[0] &&
              tFineChunk[2] == tWaterChunk[2],
          "ince parca ust parcanin ceyregiyle ayni");

    // Saydam piksel rengi komsudan dolar, alfa 0 kalir
    std::vector<uint8_t> tBleed = {0, 0, 0, 0, 10, 20, 30, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    TerrainRaster::bleedTransparent(3, &tBleed);
    CHECK(tBleed[0] == 10 && tBleed[3] == 0 && tBleed[8] == 10 && tBleed[2 * 12 + 4 + 1] == 20 &&
              tBleed[2 * 12 + 4 + 3] == 0 && tBleed[4 + 3] == 255,
          "saydam piksel rengi komsudan");

    // Onbellek: ayni anahtarla okunur, farkli anahtarla reddedilir
    const std::string tPath = "map_test_tile.terrain";
    CHECK(tRaster.save(tPath, "key-1"), "onbellege yazilir");
    TerrainRaster tCached;
    CHECK(tCached.load(tPath, "key-1") && tCached.classAt(tHalf + 100, 100) == TerrainClass::ForestConifer &&
              tCached.west() == 29 && tCached.south() == 41,
          "onbellekten okunur");
    CHECK(!tCached.load(tPath, "key-2"), "anahtar degisti: yeniden uretilmeli");
    std::remove(tPath.c_str());
}

void testLandBlend()
{
    // 1) Plan: erim metreden piksele cevrilir; kucukse bos plan (filtre kapali)
    LandBlendPlan tPlan = planLandBlend(0.0, 1.0, 1.0, 24.0);
    CHECK(tPlan.mPad == 0, "plan: blend 0 -> bos");
    tPlan = planLandBlend(150.0, 108.0, 108.0, 24.0);
    CHECK(tPlan.mPad == 0, "seviye 2 (~108 m/px): erim 1,5 pikselden kucuk, plan bos");
    tPlan = planLandBlend(150.0, 27.0, 27.0, 24.0);
    CHECK(tPlan.mPad > 0 && tPlan.mFactor == 1 && tPlan.mPad >= 6, "seviye 3 (~27 m/px): plan dolu, carpan 1");
    tPlan = planLandBlend(11.0, 1.0, 1.6, 24.0);
    CHECK(tPlan.mFactor > 1 && tPlan.mHalfX > tPlan.mHalfY,
          "dogu-bati pikseli kucuk -> hucre buyur, yari genislik dogu-batida buyuk");
    tPlan = planLandBlend(150.0, 3.4, 3.4, 24.0);
    CHECK(tPlan.mPad > 0 && tPlan.mPad <= 24 + 3, "tavan 24: erim 24 pikselle sinirli (pay = erim + izgara carpani)");
    // [blend] max_pixels = 5 -> yakin seviyede erim kenar yumusatma duzeyinde
    tPlan = planLandBlend(150.0, 3.4, 3.4, 5.0);
    CHECK(tPlan.mPad > 0 && tPlan.mPad <= 5 + 2, "max_pixels 5: erim 5 pikselle sinirli");
    tPlan = planLandBlend(150.0, 0.58, 0.85, 5.0);
    CHECK(tPlan.mPad > 0 && tPlan.mPad <= 5 + 2, "seviye 5 olceginde de erim 5 pikselle sinirli");
    tPlan = planLandBlend(150.0, 0.58, 0.85, 0.0);
    CHECK(tPlan.mPad > 5 + 2, "max_pixels 0 = tavan yok (metre degeri devrede)");

    // 2) Duz renk korunur (normalize evrisim karartmaz)
    LandColorField tField;
    tField.reset(64, 1);
    for (int tRow = 0; tRow < 64; ++tRow)
    {
        for (int tColumn = 0; tColumn < 64; ++tColumn)
        {
            tField.add(tColumn, tRow, {100, 150, 200}, 1.0f);
        }
    }
    tField.blur(4, 4);
    double tRgb[3] = {0.0, 0.0, 0.0};
    CHECK(tField.colorAt(32, 32, tRgb) && near(tRgb[0], 100.0, 0.5) && near(tRgb[1], 150.0, 0.5) &&
              near(tRgb[2], 200.0, 0.5),
          "duz renk: merkez korunur");
    CHECK(tField.colorAt(0, 0, tRgb) && near(tRgb[0], 100.0, 0.5) && near(tRgb[1], 150.0, 0.5) &&
              near(tRgb[2], 200.0, 0.5),
          "duz renk: kose de korunur (pencere kirpilse de oran ayni)");

    // 3) Iki renk arasinda gecis olusur
    tField.reset(64, 1);
    for (int tRow = 0; tRow < 64; ++tRow)
    {
        for (int tColumn = 0; tColumn < 64; ++tColumn)
        {
            tField.add(tColumn, tRow, tColumn < 32 ? TerrainColor{40, 80, 40} : TerrainColor{120, 110, 70}, 1.0f);
        }
    }
    tField.blur(3, 3);
    CHECK(tField.colorAt(32, 32, tRgb) && tRgb[1] > 81.0 && tRgb[1] < 109.0,
          "sinir pikseli iki rengin arasinda (G 80..110, kesin uclarda degil)");
    CHECK(tField.colorAt(52, 32, tRgb) && near(tRgb[0], 120.0, 1.0) && near(tRgb[1], 110.0, 1.0) &&
              near(tRgb[2], 70.0, 1.0),
          "sinirdan 20 px uzak: kendi rengi");

    // 4) Veri yok: hanya eklenen yerden uzakta renk yok, erim icinde sol rengin aynisi
    tField.reset(64, 1);
    for (int tRow = 0; tRow < 64; ++tRow)
    {
        for (int tColumn = 0; tColumn < 32; ++tColumn)
        {
            tField.add(tColumn, tRow, {40, 80, 40}, 1.0f);
        }
    }
    tField.blur(3, 3);
    CHECK(!tField.colorAt(63, 32, tRgb), "erimin disinda veri yok");
    CHECK(tField.colorAt(36, 32, tRgb) && near(tRgb[0], 40.0, 1.0) && near(tRgb[1], 80.0, 1.0) &&
              near(tRgb[2], 40.0, 1.0),
          "erimin icinde: sol rengin aynisi (normalize, karartmaz)");

    // 8) Palet surumu: eski dosya renkleri atilir, blend sinirlanir
    TerrainPalette tVersionPalette;
    Settings tVersionSettings;
    tVersionSettings.setString("colors", "forest_conifer", "1 2 3");
    tVersionPalette.load(tVersionSettings);
    CHECK(!tVersionPalette.loadedCurrentVersion() &&
              tVersionPalette.colorOf(TerrainClass::ForestConifer).mR == 52 &&
              tVersionPalette.colorOf(TerrainClass::ForestConifer).mB == 58,
          "eski surum: dosyadaki renk atilir, yeni varsayilan kalir");
    tVersionSettings.setInt("palette", "version", TerrainPalette::kPaletteVersion);
    tVersionSettings.setString("palette", "source", "colors");
    tVersionPalette.load(tVersionSettings);
    CHECK(tVersionPalette.loadedCurrentVersion() &&
              tVersionPalette.colorOf(TerrainClass::ForestConifer).mR == 1 &&
              tVersionPalette.colorOf(TerrainClass::ForestConifer).mB == 3,
          "guncel surum: dosyadaki renk okunur");
    tVersionSettings.setDouble("blend", "meters", 5000.0);
    tVersionPalette.load(tVersionSettings);
    CHECK(near(tVersionPalette.blendMeters(), 1000.0, 1e-9), "blend 1000 metreyle sinirli");
    tVersionPalette.setBlendMeters(-5.0);
    CHECK(tVersionPalette.blendMeters() == 0.0, "setBlendMeters negatif -> 0");
    // okunur, sinirlanir, Save/Load tur dondurur
    tVersionSettings.setDouble("blend", "max_pixels", 500.0);
    tVersionSettings.setDouble("warp", "meters", 9999.0);
    tVersionSettings.setDouble("warp", "wavelength", 1.0);
    tVersionSettings.setDouble("closeup", "class_mix", 5.0);
    tVersionPalette.load(tVersionSettings);
    CHECK(near(tVersionPalette.blendMaxPixels(), 64.0, 1e-9), "max_pixels 64 ile sinirli");
    CHECK(near(tVersionPalette.warp().mAmplitudeMeters, 300.0, 1e-9), "warp meters 300 ile sinirli");
    CHECK(near(tVersionPalette.warp().mWavelengthMeters, 50.0, 1e-9), "warp wavelength 50 ile sinirli");
    CHECK(near(tVersionPalette.closeupClassMix(), 0.8, 1e-9), "class_mix 0,8 ile sinirli");
    tVersionPalette.setBlendMaxPixels(-1.0);
    tVersionPalette.setWarp({-5.0, 3000.0, 3});
    tVersionPalette.setCloseupClassMix(2.0);
    CHECK(tVersionPalette.blendMaxPixels() == 0.0, "setBlendMaxPixels negatif -> 0");
    CHECK(tVersionPalette.warp().mAmplitudeMeters == 0.0 &&
              near(tVersionPalette.warp().mWavelengthMeters, 2000.0, 1e-9),
          "setWarp sinirlar");
    CHECK(near(tVersionPalette.closeupClassMix(), 0.8, 1e-9), "setCloseupClassMix sinirlar");
    const TerrainColor tReference = tVersionPalette.referenceLandColor();
    CHECK(tReference.mR > 0 && tReference.mG > 0 && tReference.mB > 0, "referans kara tonu hesaplanir");
    Settings tSavedSettings;
    tVersionPalette.save(tSavedSettings);
    CHECK(tSavedSettings.getInt("palette", "version", 0) == TerrainPalette::kPaletteVersion,
          "Save palet surumunu yazar");
    CHECK(near(tSavedSettings.getDouble("blend", "meters", -1.0), 1000.0, 1e-9),
          "Save blend degerini yazar (son Load ile sinirli 1000)");
    CHECK(near(tSavedSettings.getDouble("blend", "max_pixels", -1.0), 0.0, 1e-9) &&
              near(tSavedSettings.getDouble("warp", "meters", -1.0), 0.0, 1e-9) &&
              near(tSavedSettings.getDouble("warp", "wavelength", -1.0), 2000.0, 1e-9) &&
              near(tSavedSettings.getDouble("closeup", "class_mix", -1.0), 0.8, 1e-9),
          "Save max_pixels / warp / closeup degerlerini yazar");

    // 5/6/7) Doku yolu: su keskin, sinir yumusak, parca dikisi yok
    // Sentetik karo (10,50): 10.499 boylamina kadar grass, oradan doguya shrub (iki KARA sinifi;
    // sinir 10.5'teki parca sinirinin hemen solunda, yani dikis testi sinirin dibinde yapilir)
    TerrainRaster tLandRaster;
    tLandRaster.reset(10, 50);
    TerrainTriangles tLandTriangles;
    tLandTriangles.reset(10, 50);
    const double tBoundary = 10.499;
    auto tAddRect = [&](double pWest, double pEast, TerrainClass pClass)
    {
        const double tLonA[3] = {pWest, pEast, pWest};
        const double tLatA[3] = {50.0, 50.0, 51.0};
        const double tLonB[3] = {pEast, pEast, pWest};
        const double tLatB[3] = {50.0, 51.0, 51.0};
        tLandTriangles.add(tLonA, tLatA, pClass);
        tLandTriangles.add(tLonB, tLatB, pClass);
    };
    tAddRect(10.0, tBoundary, TerrainClass::Grass);
    tAddRect(tBoundary, 11.0, TerrainClass::Shrub);

    TerrainPalette tBlendPalette;  // varsayilan [blend] 150 metre
    // bu testler kesin sinif rengini dogrular; sinir bukmesi ve yakin zoom karisimi kapali
    // (bukme acikken sinir pikselleri sinif degistirebilir; TestTerrainWarp bukmeyi ayri dogrular)
    tBlendPalette.setWarp({0.0, 250.0, 3});
    tBlendPalette.setCloseupClassMix(0.0);
    // kiyi cizgisi ve kiyi seridi kiyidaki pikseli bilerek degistirir; bu testler sadece
    // yumusatmanin su rengini karaya tasimadigini olcer (kiyi stili TestTerrainRelief'te)
    TerrainWaterParams tNoCoastStyle = tBlendPalette.water();
    tNoCoastStyle.mCoastline = false;
    tNoCoastStyle.mBandMeters = 0.0;
    tBlendPalette.setWater(tNoCoastStyle);
    const int tSize = 64;
    const int tChunks = TerrainTriangles::kChunksPerSide;

    // 5) Su keskin kalir: su/orman DSF'inde su pikseli tam su rengi, kiyidaki kara orman renginde
    const std::vector<uint8_t> tData = makeTestDsf(false);
    DsfReader tReader;
    std::string tError;
    tReader.open(tData, &tError);
    TerrainRaster tRaster;
    tRaster.reset(29, 41);
    TerrainTriangles tSeaTriangles;
    tSeaTriangles.reset(29, 41);
    tReader.forEachTriangle(
        [&](const DsfTriangle& pTriangle)
        {
            if ((pTriangle.mPatchFlags & DsfReader::kPatchFlagOverlay) == 0)
            {
                const TerrainClass tClass =
                    classifyTerrain(tReader.info().mTerrainNames[pTriangle.mTerrain]);
                tRaster.fillTriangle(pTriangle.mLon, pTriangle.mLat, tClass);
                tSeaTriangles.add(pTriangle.mLon, pTriangle.mLat, tClass);
            }
        },
        &tError);
    std::vector<uint8_t> tSeaImage;
    tRaster.buildDetailImage(tSeaTriangles, tChunks, 5 * tChunks + 15, tSize, tBlendPalette, TerrainImageStyle(),
                             &tSeaImage);
    CHECK(tSeaImage.size() == static_cast<size_t>(tSize) * tSize * 4 &&
              tSeaImage[(32 * tSize + 32) * 4 + 0] == tBlendPalette.colorOf(TerrainClass::Water).mR &&
              tSeaImage[(32 * tSize + 32) * 4 + 3] == 255,
          "su pikseli tam su rengi ve opak");
    std::vector<uint8_t> tCoastImage;
    tRaster.buildDetailImage(tSeaTriangles, tChunks, 5 * tChunks + 16, tSize, tBlendPalette, TerrainImageStyle(),
                             &tCoastImage);
    const uint8_t* tCoastLand = tCoastImage.data() + (32 * tSize + 0) * 4;  // kiyiya yakin kara pikseli
    CHECK(tCoastLand[3] == 255 &&
              std::abs(static_cast<int>(tCoastLand[0]) -
                       static_cast<int>(tBlendPalette.colorOf(TerrainClass::ForestConifer).mR)) <= 2 &&
              std::abs(static_cast<int>(tCoastLand[1]) -
                       static_cast<int>(tBlendPalette.colorOf(TerrainClass::ForestConifer).mG)) <= 2,
          "kiyidaki kara pikseli su rengine kaymaz (orman renginde)");

    // 6) Sinir yumusar: blend 150'de sinir pikseli ara ton, blend 0'da kendi sinif rengi
    std::vector<uint8_t> tSoftImage;
    tLandRaster.buildDetailImage(tLandTriangles, tChunks, 5 * tChunks + 15, tSize, tBlendPalette, TerrainImageStyle(),
                                 &tSoftImage);
    // sinir(10.499) parca icinde sutun ~62; sutun 61 sinirin hemen solunda (grass tarafi)
    const uint8_t* tSoftPixel = tSoftImage.data() + (32 * tSize + 61) * 4;
    CHECK(tSoftPixel[3] == 255 && tSoftPixel[1] > 98 + 1 && tSoftPixel[1] < 106 - 1,
          "sinir pikseli ara ton (G iki sinif rengi arasinda)");
    TerrainPalette tSharpPalette;
    tSharpPalette.setBlendMeters(0.0);  // yumusatma kapali: ayni piksel keskin kendi renginde
    tSharpPalette.setWarp({0.0, 250.0, 3});  // bukme de kapali: piksel kaymadan kendi sinifinda
    tSharpPalette.setCloseupClassMix(0.0);
    std::vector<uint8_t> tSharpImage;
    tLandRaster.buildDetailImage(tLandTriangles, tChunks, 5 * tChunks + 15, tSize, tSharpPalette, TerrainImageStyle(),
                                 &tSharpImage);
    const uint8_t* tSharpPixel = tSharpImage.data() + (32 * tSize + 61) * 4;
    CHECK(tSharpPixel[0] == tSharpPalette.colorOf(TerrainClass::Grass).mR &&
              tSharpPixel[1] == tSharpPalette.colorOf(TerrainClass::Grass).mG,
          "blend 0: sinir pikseli keskin kendi sinif renginde");

    // 7) Dikis yok: sinira yakin parca kenarlarinda (15. parcanin son sutunu, 16. parcanin
    // ilk sutunu) kanal basina fark <= 3; pay olmasaydi yumusatma farki buyurdu
    std::vector<uint8_t> tRightImage;
    tLandRaster.buildDetailImage(tLandTriangles, tChunks, 5 * tChunks + 16, tSize, tBlendPalette, TerrainImageStyle(),
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

void testTerrainWarp()
{
    // 1) Gurultu: deterministik, [-1, 1] icinde, ortalamasi 0'a yakin, yakin noktalar surekli
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

    // 2) Kayma siniri: |kayma| <= genlik * koku(2); bilesenler genligi asmaz
    TerrainWarpParams tParams;  // varsayilan 65 / 250 / 3
    double tMaxLen = 0.0;
    for (int tIndex = 0; tIndex < 10000; ++tIndex)
    {
        const double tLon = -122.0 + tIndex * 0.0001;
        const double tLat = 47.0 + (tIndex % 97) * 0.001;
        double tEast = 0.0;
        double tNorth = 0.0;
        terrainWarpOffset(tLon, tLat, tParams, &tEast, &tNorth);
        CHECK(std::abs(tEast) <= tParams.mAmplitudeMeters && std::abs(tNorth) <= tParams.mAmplitudeMeters,
              "kayma bilesenleri genlik icinde");
        tMaxLen = std::max(tMaxLen, std::hypot(tEast, tNorth));
    }
    CHECK(tMaxLen <= tParams.mAmplitudeMeters * std::sqrt(2.0) + 1e-9, "kayma uzunlugu genlik * koku(2) icinde");

    // 3) Karo sinirinda sureklilik: 1e-9 derece kayma kaymayi degistirmez
    double tEa = 0.0;
    double tNa = 0.0;
    double tEb = 0.0;
    double tNb = 0.0;
    terrainWarpOffset(-122.0, 47.0, tParams, &tEa, &tNa);
    terrainWarpOffset(-122.0 + 1e-9, 47.0 + 1e-9, tParams, &tEb, &tNb);
    CHECK(std::hypot(tEb - tEa, tNb - tNa) < 1e-3, "karo sinirinda kayma surekli");

    // 4) Plan: uzak seviye etkisiz, yakin seviye aktif; meters 0 = kapali
    TerrainWarpPlan tPlan = terrainPlanWarp(tParams, 108.0, 108.0);
    CHECK(!tPlan.mActive, "seviye 2 (~108 m/px): bukme etkisiz");
    tPlan = terrainPlanWarp(tParams, 27.0, 27.0);
    CHECK(tPlan.mActive && tPlan.mPad >= 3 && tPlan.mStep == 1, "seviye 3 (~27 m/px): bukme aktif");
    tPlan = terrainPlanWarp(tParams, 0.85, 0.85);
    CHECK(tPlan.mActive && tPlan.mPad >= 77 && tPlan.mStep > 1, "seviye 5: pay buyuk, izgara adimi > 1");
    tParams.mAmplitudeMeters = 0.0;
    CHECK(!terrainPlanWarp(tParams, 0.85, 0.85).mActive, "warp meters 0 = duz kenarlar");

    // 5) Kara-kara kurali (buildDetailImage): su birebir ayni, kara-kara siniri degisir
    TerrainRaster tRaster;
    tRaster.reset(10, 50);
    TerrainTriangles tTriangles;
    tTriangles.reset(10, 50);
    auto tAddRect = [&](double pWest, double pEast, TerrainClass pClass)
    {
        const double tLonA[3] = {pWest, pEast, pWest};
        const double tLatA[3] = {50.0, 50.0, 51.0};
        const double tLonB[3] = {pEast, pEast, pWest};
        const double tLatB[3] = {50.0, 51.0, 51.0};
        tTriangles.add(tLonA, tLatA, pClass);
        tTriangles.add(tLonB, tLatB, pClass);
    };
    tAddRect(10.0, 10.25, TerrainClass::Water);
    tAddRect(10.25, 10.3, TerrainClass::ForestConifer);
    tAddRect(10.3, 10.499, TerrainClass::Grass);
    tAddRect(10.499, 11.0, TerrainClass::Shrub);
    TerrainPalette tPlainPalette;
    tPlainPalette.setBlendMeters(0.0);
    tPlainPalette.setCloseupClassMix(0.0);
    tPlainPalette.setWarp({0.0, 250.0, 3});  // karsilastirmanin "kapali" tarafi: duz kenarlar
    const int tChunks = TerrainTriangles::kChunksPerSide;
    const int tSize = 64;
    TerrainPalette tWarpPalette = tPlainPalette;  // bukme acik: varsayilan 65 m geri verilir
    tWarpPalette.setWarp({65.0, 250.0, 3});
    // 5a) Su: parca 7 (10.21875..10.25) tamamen su; bukme acik/kapali goruntuler birebir ayni
    std::vector<uint8_t> tOffImage;
    std::vector<uint8_t> tOnImage;
    tRaster.buildDetailImage(tTriangles, tChunks, 10 * tChunks + 7, tSize, tPlainPalette, TerrainImageStyle(),
                             &tOffImage);
    tRaster.buildDetailImage(tTriangles, tChunks, 10 * tChunks + 7, tSize, tWarpPalette, TerrainImageStyle(),
                             &tOnImage);
    int tWaterOff = 0;
    int tWaterOn = 0;
    bool tWaterSame = true;
    for (int tPixel = 0; tPixel < tSize * tSize; ++tPixel)
    {
        const uint8_t* tA = tOffImage.data() + static_cast<size_t>(tPixel) * 4;
        const uint8_t* tB = tOnImage.data() + static_cast<size_t>(tPixel) * 4;
        const bool tWaterA = tA[3] == 255 && tA[0] == tPlainPalette.colorOf(TerrainClass::Water).mR &&
                             tA[1] == tPlainPalette.colorOf(TerrainClass::Water).mG;
        const bool tWaterB = tB[3] == 255 && tB[0] == tPlainPalette.colorOf(TerrainClass::Water).mR &&
                             tB[1] == tPlainPalette.colorOf(TerrainClass::Water).mG;
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
    tRaster.buildDetailImage(tTriangles, tChunks, 10 * tChunks + 9, tSize, tPlainPalette, TerrainImageStyle(),
                             &tOffImage);
    tRaster.buildDetailImage(tTriangles, tChunks, 10 * tChunks + 9, tSize, tWarpPalette, TerrainImageStyle(),
                             &tOnImage);
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

    // 6) Dikis yok: bukme acikken parca kenarinda kanal basina fark <= 3.
    // Ornek sinir (10.3) dikisin (10.3125) ~26 piksel onunde: gecis dikisten once biter
    tWarpPalette.setBlendMeters(150.0);  // blur + bukme birlikte: toplam pay ile uretim
    tWarpPalette.setCloseupClassMix(0.3);
    std::vector<uint8_t> tLeftImage;
    std::vector<uint8_t> tRightImage;
    tRaster.buildDetailImage(tTriangles, tChunks, 10 * tChunks + 9, tSize, tWarpPalette, TerrainImageStyle(),
                             &tLeftImage);
    tRaster.buildDetailImage(tTriangles, tChunks, 10 * tChunks + 10, tSize, tWarpPalette, TerrainImageStyle(),
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

    // 7) Sinif karisimi: orana gore referans kara tona yaklasir; su ve kar degismez
    CHECK(near(tWarpPalette.closeupMixAt(100.0), 0.0, 1e-9), "piksel 50 m'den buyuk: karisim 0");
    const double tMid = tWarpPalette.closeupMixAt(27.0);
    CHECK(tMid > 0.0 && tMid < 0.15, "piksel 27 m: karisim dusuk");
    CHECK(near(tWarpPalette.closeupMixAt(3.0), tWarpPalette.closeupClassMix(), 1e-9), "piksel 3 m: tam oran");
    const auto tMixed = tWarpPalette.mixedColors(0.3);
    CHECK(tMixed[static_cast<int>(TerrainClass::Water)].mR == tWarpPalette.colorOf(TerrainClass::Water).mR &&
              tMixed[static_cast<int>(TerrainClass::SnowIce)].mG == tWarpPalette.colorOf(TerrainClass::SnowIce).mG,
          "su ve kar karismaz");
    const TerrainColor tRef = tWarpPalette.referenceLandColor();
    const TerrainColor tForest = tWarpPalette.colorOf(TerrainClass::ForestConifer);
    const TerrainColor tMixedForest = tMixed[static_cast<int>(TerrainClass::ForestConifer)];
    CHECK(tMixedForest.mR > std::min(tForest.mR, tRef.mR) - 1 &&
              tMixedForest.mR < std::max(tForest.mR, tRef.mR) + 1 &&
              tMixedForest.mG > std::min(tForest.mG, tRef.mG) - 1 &&
              tMixedForest.mG < std::max(tForest.mG, tRef.mG) + 1,
          "orman rengi referansa yaklasmis (kanallar arada)");
    const auto tUnmixed = tWarpPalette.mixedColors(0.0);
    CHECK(tUnmixed[static_cast<int>(TerrainClass::ForestConifer)].mR == tForest.mR, "karisim 0: paletle ayni");
}

void testSceneryIndex()
{
    int tWest = 0;
    int tSouth = 0;
    CHECK(SceneryIndex::parseTileName("+47-123.dsf", &tWest, &tSouth) && tWest == -123 && tSouth == 47, "karo adi");
    CHECK(SceneryIndex::parseTileName("-05+000.DSF", &tWest, &tSouth) && tWest == 0 && tSouth == -5, "buyuk harf");
    CHECK(!SceneryIndex::parseTileName("+47-123.txt", &tWest, &tSouth) &&
              !SceneryIndex::parseTileName("47-123.dsf", &tWest, &tSouth),
          "gecersiz adlar");
    CHECK(SceneryIndex::tileName(-123, 47) == "+47-123" && SceneryIndex::folderName(-123, 47) == "+40-130" &&
              SceneryIndex::folderName(5, -5) == "-10+000",
          "ad uretimi");

    // Gecici klasorlerde iki paket: ozel (overlay olabilir) + global
    namespace Fs = std::filesystem;
    const Fs::path tRoot = Fs::temp_directory_path() / "prime_map_plugin_index_test";
    std::error_code tIgnore;
    Fs::remove_all(tRoot, tIgnore);
    Fs::create_directories(tRoot / "Custom" / "Earth nav data" / "+40-130");
    Fs::create_directories(tRoot / "Global" / "Earth nav data" / "+40-130");
    std::ofstream(tRoot / "Custom" / "Earth nav data" / "+40-130" / "+47-123.dsf") << "custom";
    std::ofstream(tRoot / "Global" / "Earth nav data" / "+40-130" / "+47-123.dsf") << "global";
    std::ofstream(tRoot / "Global" / "Earth nav data" / "+40-130" / "+46-124.dsf") << "global";
    std::ofstream(tRoot / "Global" / "Earth nav data" / "+40-130" / "notes.txt") << "x";

    SceneryIndex tIndex;
    tIndex.buildFromPacks({(tRoot / "Custom").string() + "/"}, {(tRoot / "Global").string() + "/"});
    const std::vector<DsfCandidate>* tCandidates = tIndex.candidatesFor(-123, 47);
    CHECK(tCandidates && tCandidates->size() == 2 && tCandidates->front().mPath.find("Custom") != std::string::npos,
          "ozel paket once");
    CHECK(tIndex.tileCount() == 2 && tIndex.fileCount() == 3 && tIndex.packCount() == 2, "sayilar");
    CHECK(tIndex.coverageOf(-123, 47) == TileCoverage::hasDsf, "DSF var");
    CHECK(tIndex.coverageOf(-125, 48) == TileCoverage::Ocean, "kurulu bolgede DSF yok: deniz");
    CHECK(tIndex.coverageOf(29, 41) == TileCoverage::NoData, "kurulmamis bolge: veri yok");
    Fs::remove_all(tRoot, tIgnore);
}

void testStoreCancel()
{
    // Yukleme bitmeden yok edilen depo takilmadan kapanmali (X-Plane kapanisi)
    AirportGeometryStore tStore(std::make_unique<AptDatIndexer>());
    tStore.startLoading("Z:\\no_such_folder\\");
}

// LTAC 03R/21L pistini veren kaynak (gercek apt.dat koordinatlari)
class LtacRunwaySource : public IAirportGeometrySource
{
public:
    const char* name() const override
    {
        return "LTAC test";
    }

    bool loadAll(const std::string& /*pXPlaneDirectory*/, const std::atomic<bool>& /*pCancel*/,
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

void waitUntilLoaded(AirportGeometryStore& pStore)
{
    waitUntil([&]
    {
        return pStore.poll();
    });
}

void testLocalizerMatch()
{
    CHECK(near(initialBearingDeg({0, 0}, {0, 1}), 90.0, 1e-9) && near(initialBearingDeg({0, 0}, {1, 0}), 0.0, 1e-9),
          "dogu 90, kuzey 0");
    CHECK(near(initialBearingDeg({0, 0}, {0, -1}), 270.0, 1e-9), "bati 270 (eksi degil)");
    CHECK(near(angleDifferenceDeg(350.0, 10.0), 20.0, 1e-12) && near(angleDifferenceDeg(90.0, 270.0), 180.0, 1e-12),
          "aci farki 0..180");

    std::string tClean;
    CHECK(airportKindFromName("[H] Harborview Medical Center", &tClean) == AirportKind::Heliport &&
              tClean == "Harborview Medical Center",
          "heliport: '%s'", tClean.c_str());
    CHECK(airportKindFromName("[S] Kenmore Air Harbor", &tClean) == AirportKind::Seaplane, "deniz ussu");
    CHECK(airportKindFromName("[X] Eski Alan", &tClean) == AirportKind::Closed && tClean == "Eski Alan", "kapali");
    CHECK(airportKindFromName("Esenboga", &tClean) == AirportKind::Land && tClean == "Esenboga", "kara");
    CHECK(airportKindFromName("[Q] Garip", &tClean) == AirportKind::Land && tClean == "[Q] Garip",
          "bilinmeyen onek oldugu gibi kalir");

    AirportGeometryStore tStore(std::make_unique<LtacRunwaySource>());
    LatLon tThreshold = {0.0, 0.0};
    CHECK(!findLocalizerThreshold({40.14, 33.01}, 36.0, tStore, &tThreshold), "pist verisi yokken bulunamaz");
    tStore.startLoading("");
    waitUntilLoaded(tStore);
    CHECK(tStore.isReady(), "test deposu hazir");

    const LatLon tEnd03R = {40.1140623, 32.9831635};
    const LatLon tEnd21L = {40.1413103, 33.0091255};
    const double tCourse03R = initialBearingDeg(tEnd03R, tEnd21L);

    // 03R'ye inis: anten 21L ucunun ~300 m otesinde
    const LatLon tAntenna03R = destinationPoint(tEnd21L, tCourse03R, 0.16);
    CHECK(findLocalizerThreshold(tAntenna03R, tCourse03R, tStore, &tThreshold) &&
              near(tThreshold.mLat, tEnd03R.mLat, 1e-9) && near(tThreshold.mLon, tEnd03R.mLon, 1e-9),
          "03R ILS esigi 03R ucu");

    // Ters yon: 21L'ye inis, anten 03R'nin otesinde
    const LatLon tAntenna21L = destinationPoint(tEnd03R, tCourse03R + 180.0, 0.16);
    CHECK(findLocalizerThreshold(tAntenna21L, tCourse03R + 180.0, tStore, &tThreshold) &&
              near(tThreshold.mLat, tEnd21L.mLat, 1e-9),
          "21L ILS esigi 21L ucu");

    // Eksenden 0.5 NM kayik (LDA gibi) ve yonu tutmayan localizer eslesmez
    const LatLon tOffset = destinationPoint(tAntenna03R, tCourse03R + 90.0, 0.5);
    CHECK(!findLocalizerThreshold(tOffset, tCourse03R, tStore, &tThreshold), "kayik localizer eslesmez");
    CHECK(!findLocalizerThreshold(tAntenna03R, tCourse03R + 90.0, tStore, &tThreshold), "yonu tutmayan eslesmez");
}

void testMapViewBounds()
{
    const XMapView tView = makeView();
    const GeoBounds tVisible = tView.visibleBounds();
    RectD tLarger = tView.viewport();
    tLarger.mLeft -= 100;
    tLarger.mRight += 100;
    tLarger.mBottom -= 100;
    tLarger.mTop += 100;
    const GeoBounds tExpanded = tView.boundsOf(tLarger);
    CHECK(tExpanded.mWest < tVisible.mWest && tExpanded.mEast > tVisible.mEast &&
              tExpanded.mSouth < tVisible.mSouth && tExpanded.mNorth > tVisible.mNorth,
          "genisletilmis alan daha buyuk");
    const LatLon tCorner = tView.boxelToLatLon(tView.viewport().mLeft, tView.viewport().mTop);
    CHECK(near(tVisible.mWest, tCorner.mLon, 1e-9) && near(tVisible.mNorth, tCorner.mLat, 1e-9),
          "sinir kosesi BoxelToLatLon ile ayni");

    CHECK(near(tView.metersPerBoxelAt(tView.center().mLat), tView.metersPerBoxel(), 1e-9),
          "merkezde iki olcek ayni");
    CHECK(tView.metersPerBoxelAt(60.0) < tView.metersPerBoxelAt(0.0), "kuzeyde boxel daha az metre");
}

}  // namespace

// rakim, kabartma, su, dunya tablosu

// Sentetik DEM, DSF duzeninde (satir 0 = GUNEY, post-centric). pHeight(sutun, guneyden satir) metre.
DsfRaster makeDem(int pSize, const std::function<double(int, int)>& pHeight)
{
    DsfRaster tRaster;
    tRaster.mWidth = pSize;
    tRaster.mHeight = pSize;
    tRaster.mPostCentric = true;
    tRaster.mValues.resize(static_cast<size_t>(pSize) * pSize);
    for (int tRow = 0; tRow < pSize; ++tRow)
    {
        for (int tColumn = 0; tColumn < pSize; ++tColumn)
        {
            tRaster.mValues[static_cast<size_t>(tRow) * pSize + tColumn] =
                static_cast<float>(pHeight(tColumn, tRow));
        }
    }
    return tRaster;
}

void testElevationCodec()
{
    // Kayipsiz: rampa, eksi degerler, uc degerler, sozde rastgele gurultu
    const int tWidth = 37;
    const int tHeight = 23;
    std::vector<int16_t> tValues(static_cast<size_t>(tWidth) * tHeight);
    uint32_t tSeed = 12345;
    for (size_t tIndex = 0; tIndex < tValues.size(); ++tIndex)
    {
        tSeed = tSeed * 1664525u + 1013904223u;
        tValues[tIndex] = static_cast<int16_t>(static_cast<int>(tIndex % 50) * 7 - 200 + static_cast<int>(tSeed >> 28));
    }
    tValues[0] = -32000;
    tValues[1] = 32000;
    std::vector<uint8_t> tPacked;
    std::vector<int16_t> tBack;
    CHECK(ElevationCodec::compress(tValues, tWidth, tHeight, &tPacked) &&
              ElevationCodec::decompress(tPacked.data(), tPacked.size(), tWidth, tHeight, &tBack) && tBack == tValues,
          "rakim kodlayici kayipsiz (eksi ve uc degerler dahil)");
    CHECK(!ElevationCodec::decompress(tPacked.data(), tPacked.size(), tWidth + 1, tHeight, &tBack),
          "boyut tutmazsa reddedilir");
    CHECK(!ElevationCodec::decompress(tPacked.data(), 4, tWidth, tHeight, &tBack), "kisa veri reddedilir");

    // Puruzsuz arazi iyi sikisir (satir farki + LZMA)
    const int tSize = 257;
    std::vector<int16_t> tSmooth(static_cast<size_t>(tSize) * tSize);
    for (int tRow = 0; tRow < tSize; ++tRow)
    {
        for (int tColumn = 0; tColumn < tSize; ++tColumn)
        {
            tSmooth[static_cast<size_t>(tRow) * tSize + tColumn] =
                static_cast<int16_t>(500.0 + 300.0 * std::sin(tColumn * 0.05) * std::cos(tRow * 0.04));
        }
    }
    CHECK(ElevationCodec::compress(tSmooth, tSize, tSize, &tPacked) && tPacked.size() < tSmooth.size() * 2 / 4,
          "puruzsuz DEM ham boyutun dortte birinden kucuk: %d / %d", static_cast<int>(tPacked.size()),
          static_cast<int>(tSmooth.size() * 2));
}

void testElevationGrid()
{
    // Duzlem: yukseklik = 100 + 4 * sutun + 2 * (guneyden satir). Tam sayi: int16 saklamada kayip yok.
    const int tSize = 33;
    auto tPlane = [](int pColumn, int pRowFromSouth)
    {
        return 100.0 + 4.0 * pColumn + 2.0 * pRowFromSouth;
    };
    ElevationGrid tGrid;
    CHECK(tGrid.build(makeDem(tSize, tPlane), nullptr) && !tGrid.isEmpty(), "DEM'den izgara kurulur");
    CHECK(near(tGrid.post(0, 0, 0), tPlane(0, tSize - 1), 1e-6) &&
              near(tGrid.post(0, 5, tSize - 1), tPlane(5, 0), 1e-6),
          "satir 0 = kuzey (DSF satir 0 = guney cevrildi)");
    CHECK(tGrid.size(1) == 17 && tGrid.size(2) == 9, "piramit boyutlari 33 -> 17 -> 9");
    // [1 2 1] suzgec dogrusal yuzeyi korur: seviye 1 noktasi (i, j) = taban (2i, 2j); kenarlar dahil
    bool tPyramidOk = true;
    for (int tRow = 0; tRow < 17; ++tRow)
    {
        for (int tColumn = 0; tColumn < 17; ++tColumn)
        {
            tPyramidOk = tPyramidOk && near(tGrid.post(1, tColumn, tRow), tGrid.post(0, 2 * tColumn, 2 * tRow), 1e-4);
        }
    }
    CHECK(tPyramidOk, "seviye 1 dogrusal yuzeyi korur (kenar noktalari dahil)");
    CHECK(near(tGrid.cellMeters(0), 110574.0 / 32.0, 1e-6) && tGrid.levelFor(1.0) == 0 &&
              tGrid.levelFor(tGrid.cellMeters(1) + 1.0) == 1 && tGrid.levelFor(1e9) == 2,
          "seviye secimi: nokta araligi piksel boyunu gecmeyen en kaba seviye");

    // Kaba seviye gurultuyu ortalar (ortusme yok): satranc tahtasi (+-50 m) seviye 1'de duzlesir
    ElevationGrid tChecker;
    tChecker.build(makeDem(tSize,
                           [](int pColumn, int pRow)
                           {
                               return 1000.0 + (((pColumn + pRow) % 2 == 0) ? 50.0 : -50.0);
                           }),
                   nullptr);
    CHECK(std::fabs(tChecker.post(1, 8, 8) - 1000.0) < 1.0, "seviye 1 ic noktasi satranc deseni ortalamasi: %.1f",
          tChecker.post(1, 8, 8));

    // sea_level: hucre merkezli, satir 0 = kuzey
    DsfRaster tSea;
    tSea.mWidth = 4;
    tSea.mHeight = 4;
    tSea.mValues = {-10, -10, -10, -10, -20, -20, -20, -20, -30, -30, -30, -30, -40, -40, -40, -40};
    ElevationGrid tWithSea;
    tWithSea.build(makeDem(tSize, tPlane), &tSea);
    CHECK(tWithSea.hasSeaLevel() && tWithSea.seaLevelSize() == 4 && near(tWithSea.seaLevelCell(0, 0), -40, 1e-6) &&
              near(tWithSea.seaLevelCell(3, 3), -10, 1e-6),
          "sea_level satir 0 = kuzey");

    // Onbellek dosyasi: gidis-donus, yanlis anahtar reddedilir
    namespace Fs = std::filesystem;
    const Fs::path tPath = Fs::temp_directory_path() / "prime_map_plugin_elevation_test.elevation";
    CHECK(tWithSea.save(tPath.string(), "key-a"), "rakim dosyasi yazilir");
    ElevationGrid tLoaded;
    CHECK(tLoaded.load(tPath.string(), "key-a") && tLoaded.size(0) == tSize &&
              near(tLoaded.post(0, 7, 9), tWithSea.post(0, 7, 9), 1e-6) &&
              near(tLoaded.post(2, 4, 4), tWithSea.post(2, 4, 4), 1e-4) && tLoaded.hasSeaLevel() &&
              near(tLoaded.seaLevelCell(2, 1), tWithSea.seaLevelCell(2, 1), 1e-6),
          "rakim dosyasi gidis-donus (piramit yeniden kurulur)");
    ElevationGrid tWrongKey;
    CHECK(!tWrongKey.load(tPath.string(), "key-b"), "kaynak DSF degismis: dosya gecersiz");
    ElevationGrid tEmpty;
    CHECK(tEmpty.save(tPath.string(), "key-c"), "bos izgara yazilir (DEM'siz karo)");
    CHECK(tEmpty.load(tPath.string(), "key-c") && tEmpty.isEmpty(), "bos izgara bos okunur (yeniden uretilmez)");
    std::error_code tIgnore;
    Fs::remove(tPath, tIgnore);

    // Ince seviyeleri birakma (kaba rakim bellegi): seviye 2+ okunur, ince seviyeler yok, yazilamaz
    ElevationGrid tCoarse;
    tCoarse.build(makeDem(tSize, tPlane), nullptr);
    const float tLevel2 = tCoarse.post(2, 3, 3);
    tCoarse.dropFineLevels(2);
    CHECK(tCoarse.finestLevel() == 2 && !tCoarse.hasLevel(0) && !tCoarse.hasLevel(1) && tCoarse.hasLevel(2) &&
              near(tCoarse.post(2, 3, 3), tLevel2, 1e-6) && tCoarse.levelFor(1.0) == 2 && !tCoarse.isEmpty() &&
              !tCoarse.save(tPath.string(), "key-d") && tCoarse.memoryBytes() < tGrid.memoryBytes() / 4,
          "ince seviyeleri birakilmis izgara: seviye 2 ayni, bellek kucuk");
    CHECK(ElevationGrid::nominalLevelFor(30.0) == 0 && ElevationGrid::nominalLevelFor(190.0) == 1 &&
              ElevationGrid::nominalLevelFor(1700.0) == 4 && ElevationGrid::nominalLevelFor(1e6) == 4,
          "1201 noktali izgara icin seviye tahmini");

    // Hucre merkezli DEM de kabul edilir (post izgarasina orneklenir)
    DsfRaster tPixelCentric = makeDem(tSize, tPlane);
    tPixelCentric.mPostCentric = false;
    ElevationGrid tFromPixels;
    CHECK(tFromPixels.build(tPixelCentric, nullptr) && tFromPixels.size(0) == tSize, "hucre merkezli DEM");
}

void testElevationSampler()
{
    // Karolar arasi surekli bir yuzey: X dogu, Y kuzey (karo birimi; merkez karonun guney-bati kosesi 0,0)
    const int tSize = 33;
    const int tSpan = tSize - 1;
    auto tGridFor = [&](int pTileX, int pTileY, const std::function<double(double, double)>& pSurface,
                        double pOffset)
    {
        ElevationGrid tGrid;
        tGrid.build(makeDem(tSize,
                            [&](int pColumn, int pRow)
                            {
                                return std::round(pSurface(pTileX + static_cast<double>(pColumn) / tSpan,
                                                           pTileY + static_cast<double>(pRow) / tSpan)) +
                                       pOffset;
                            }),
                    nullptr);
        return tGrid;
    };
    auto tPlane = [](double pX, double pY)
    {
        return 1000.0 + 320.0 * pX - 160.0 * pY;
    };

    // 1) Duzlem: ic ve kenar (komsulu / komsusuz) her yerde tam deger ve egim
    const ElevationGrid tCenter = tGridFor(0, 0, tPlane, 0.0);
    const ElevationGrid tEast = tGridFor(1, 0, tPlane, 0.0);
    ElevationTiles tAlone;
    tAlone.mTiles[1][1] = &tCenter;
    ElevationTiles tWithEast = tAlone;
    tWithEast.mTiles[1][2] = &tEast;
    double tHeight = 0.0;
    double tDhDu = 0.0;
    double tDhDv = 0.0;
    const ElevationSampler tPlaneAlone(tAlone.mTiles, 0);
    tPlaneAlone.sample(0.37, 0.61, &tHeight, &tDhDu, &tDhDv);
    CHECK(near(tHeight, tPlane(0.37, 1.0 - 0.61), 0.6) && near(tDhDu, 320.0, 1.0) && near(tDhDv, 160.0, 1.0),
          "duzlem: ic noktada yukseklik ve egim (v guneye: dh/dv = +160)");
    tPlaneAlone.sample(0.995, 0.5, &tHeight, &tDhDu, &tDhDv);
    CHECK(near(tDhDu, 320.0, 1.0), "duzlem: komsu yokken kenarda egim (aynali dogrusal uzatma) %.2f", tDhDu);

    // 2) Egri yuzey: kenarda komsulu egim, komsusuzdan dogru
    auto tCurve = [](double pX, double pY)
    {
        return 800.0 + 900.0 * pX * pX + 50.0 * pY;
    };
    const ElevationGrid tCurveCenter = tGridFor(0, 0, tCurve, 0.0);
    const ElevationGrid tCurveEast = tGridFor(1, 0, tCurve, 0.0);
    ElevationTiles tCurveAlone;
    tCurveAlone.mTiles[1][1] = &tCurveCenter;
    ElevationTiles tCurveBoth = tCurveAlone;
    tCurveBoth.mTiles[1][2] = &tCurveEast;
    const double tU = 1.0 - 0.3 / tSpan;  // kenara 0.3 nokta mesafe
    const double tTrue = 1800.0 * tU;     // d/dx (900 x^2)
    ElevationSampler(tCurveAlone.mTiles, 0).sample(tU, 0.5, &tHeight, &tDhDu, &tDhDv);
    const double tAloneError = std::fabs(tDhDu - tTrue);
    ElevationSampler(tCurveBoth.mTiles, 0).sample(tU, 0.5, &tHeight, &tDhDu, &tDhDv);
    const double tBothError = std::fabs(tDhDu - tTrue);
    // Esik 20 (~%1,1): Catmull-Rom'un ikinci derece yuzeyde kendi turev hatasi ~%0,9 (komsudan bagimsiz)
    CHECK(tBothError < tAloneError && tBothError < 20.0,
          "egri yuzey: komsulu kenar egimi daha dogru (%.1f < %.1f)", tBothError, tAloneError);

    // 3) Ortak kenar ayni degil (GERCEK VERI: ~1-30 m): iki taraf sinirda ortalamayi gorur -> dikis yok
    const ElevationGrid tShifted = tGridFor(1, 0, tPlane, 10.0);  // dogu karo 10 m yukarida
    ElevationTiles tWestView;
    tWestView.mTiles[1][1] = &tCenter;
    tWestView.mTiles[1][2] = &tShifted;
    ElevationTiles tEastView;
    tEastView.mTiles[1][1] = &tShifted;
    tEastView.mTiles[1][0] = &tCenter;
    double tWestEdge = 0.0;
    double tEastEdge = 0.0;
    ElevationSampler(tWestView.mTiles, 0).sample(1.0, 0.5, &tWestEdge, &tDhDu, &tDhDv);
    ElevationSampler(tEastView.mTiles, 0).sample(0.0, 0.5, &tEastEdge, &tDhDu, &tDhDv);
    CHECK(near(tWestEdge, tEastEdge, 1e-6) && near(tWestEdge, tPlane(1.0, 0.5) + 5.0, 0.6),
          "ortak kenar: iki karo ayni ortalamayi gorur (%.2f / %.2f)", tWestEdge, tEastEdge);
    ElevationSampler(tAlone.mTiles, 0).sample(1.0, 0.5, &tWestEdge, &tDhDu, &tDhDv);
    CHECK(near(tWestEdge, tPlane(1.0, 0.5), 0.6), "komsu yokken kenar kendi degeri");

    // 4) Kaba seviye de ayni yuzeyi verir
    const ElevationSampler tCoarse(tWithEast.mTiles, 2);
    tCoarse.sample(0.4, 0.4, &tHeight, &tDhDu, &tDhDv);
    CHECK(tCoarse.level() == 2 && near(tHeight, tPlane(0.4, 0.6), 1.0) && near(tDhDu, 320.0, 2.0),
          "seviye 2: duzlem korunur");

    // 5) sea_level: komsu karo hucresiyle cift dogrusal (karo sinirinda dikis yok)
    auto tSeaFloor = [](int /*pColumn*/, int /*pRow*/)
    {
        return 0.0;  // duz kara: sadece sea_level olculuyor
    };
    DsfRaster tSeaWest;
    tSeaWest.mWidth = 4;
    tSeaWest.mHeight = 4;
    tSeaWest.mValues.assign(16, -100.0f);
    DsfRaster tSeaEast = tSeaWest;
    tSeaEast.mValues.assign(16, -300.0f);
    ElevationGrid tWaterWest;
    ElevationGrid tWaterEast;
    tWaterWest.build(makeDem(tSize, tSeaFloor), &tSeaWest);
    tWaterEast.build(makeDem(tSize, tSeaFloor), &tSeaEast);
    ElevationTiles tSeaTiles;
    tSeaTiles.mTiles[1][1] = &tWaterWest;
    tSeaTiles.mTiles[1][2] = &tWaterEast;
    double tSeaLevel = 0.0;
    CHECK(ElevationSampler(tSeaTiles.mTiles, 0).seaLevelAt(1.0, 0.5, &tSeaLevel) && near(tSeaLevel, -200.0, 1e-6),
          "sea_level karo sinirinda iki karonun ortasi: %.1f", tSeaLevel);
}

void testTerrainRelief()
{
    const TerrainReliefParams tRelief;
    // 1) Kabartma: duz arazi 1; kuzeybatiya bakan yamac aydinlik, guneydoguya bakan karanlik
    CHECK(near(reliefFactor(0.0, 0.0, 0.45, 2.5, tRelief), 1.0, 1e-9), "duz arazi: carpan 1");
    const double tNorthWestFacing = reliefFactor(0.2, -0.2, 0.45, 2.5, tRelief);  // dogu yukari, kuzey asagi
    const double tSouthEastFacing = reliefFactor(-0.2, 0.2, 0.45, 2.5, tRelief);
    CHECK(tNorthWestFacing > 1.05 && tSouthEastFacing < 0.95,
          "kuzeybati yamaci aydinlik (%.2f), guneydogu karanlik (%.2f)", tNorthWestFacing, tSouthEastFacing);
    TerrainReliefParams tSingle = tRelief;
    tSingle.mMultiDirectional = false;
    tSingle.mSlopeTone = 0.0;
    TerrainReliefParams tMulti = tRelief;
    tMulti.mSlopeTone = 0.0;
    // Kuzeydogu yamaci tek isikta (315) isiga dik: duz; cok yonlu isikta kuzey isigi da var
    CHECK(near(reliefFactor(-0.2, -0.2, 0.45, 2.5, tSingle), reliefFactor(0.0, 0.0, 0.45, 2.5, tSingle), 0.15) &&
              std::fabs(reliefFactor(0.2, -0.2, 0.45, 2.5, tMulti) - 1.0) <
                  std::fabs(reliefFactor(0.2, -0.2, 0.45, 2.5, tSingle) - 1.0),
          "cok yonlu isik tek yonluden yumusak");
    TerrainReliefParams tSteep = tMulti;
    tSteep.mSlopeTone = 0.5;
    CHECK(reliefFactor(0.0, 1.0, 0.45, 2.5, tSteep) < reliefFactor(0.0, 1.0, 0.45, 2.5, tMulti),
          "egim tonu dik yamaci koyulastirir");
    // Duz arazi esigi: 1 m / 92 m (~%1) basamak neredeyse etkisiz; gercek yamac (%15) hemen hemen ayni
    TerrainReliefParams tNoFlat = tRelief;
    tNoFlat.mFlatSlope = 0.0;
    CHECK(std::fabs(reliefFactor(0.0, 0.011, 0.45, 2.5, tRelief) - 1.0) <
                  0.3 * std::fabs(reliefFactor(0.0, 0.011, 0.45, 2.5, tNoFlat) - 1.0) &&
              near(reliefFactor(0.0, 0.15, 0.45, 2.5, tRelief), reliefFactor(0.0, 0.15, 0.45, 2.5, tNoFlat), 0.02),
          "duz arazi esigi: 1 m basamaklari bastirir, yamaclari korur");
    CHECK(reliefFactor(-50.0, 50.0, 1.0, 20.0, tRelief) >= 0.45 &&
              reliefFactor(50.0, -50.0, 1.0, 20.0, tRelief) <= 1.55,
          "carpan sinirli");
    CHECK(near(reliefExaggeration(tRelief, 92.0, 92.0), 2.5, 1e-9) &&
              reliefExaggeration(tRelief, 1472.0, 92.0) > 2.5 * 2.0,
          "kaba seviyede abartma artar");

    // 2) VFR bantlari: bant icinde tam renk, sinirda iki bandin ortasi, kontur istege bagli
    const TerrainVfrParams tVfr = defaultVfrParams();
    double tRgb[3];
    vfrBandColor(500.0 / 3.28084, 10.0, tVfr, tRgb);
    CHECK(near(tRgb[0], tVfr.mBandColors[0].mR, 1e-9) && near(tRgb[1], tVfr.mBandColors[0].mG, 1e-9),
          "500 ft: ilk bant tam renk");
    vfrBandColor(1000.0 / 3.28084, 10.0, tVfr, tRgb);
    CHECK(near(tRgb[0], (tVfr.mBandColors[0].mR + tVfr.mBandColors[1].mR) * 0.5, 1e-6),
          "1000 ft siniri: iki bandin ortasi (kenar yumusatma)");
    vfrBandColor(1004.0 / 3.28084, 10.0, tVfr, tRgb);
    CHECK(tRgb[0] > (tVfr.mBandColors[0].mR + tVfr.mBandColors[1].mR) * 0.5 && tRgb[0] < tVfr.mBandColors[1].mR,
          "sinira 0.4 piksel: ust banda yakin karisim");
    vfrBandColor(1050.0 / 3.28084, 10.0, tVfr, tRgb);
    CHECK(near(tRgb[0], tVfr.mBandColors[1].mR, 1e-9), "sinirdan 5 piksel: keskin bant rengi");
    vfrBandColor(-30.0, 10.0, tVfr, tRgb);
    CHECK(near(tRgb[0], tVfr.mBandColors[0].mR, 1e-9), "deniz seviyesi alti: ilk bant");
    vfrBandColor(14000.0 / 3.28084, 10.0, tVfr, tRgb);
    CHECK(near(tRgb[0], tVfr.mBandColors[7].mR, 1e-9), "12000+ ft: son bant");
    TerrainVfrParams tContour = tVfr;
    tContour.mContourLines = true;
    double tPlain[3];
    vfrBandColor(3000.0 / 3.28084, 10.0, tVfr, tPlain);
    vfrBandColor(3000.0 / 3.28084, 10.0, tContour, tRgb);
    CHECK(tRgb[1] < tPlain[1] - 5.0, "kontur cizgisi sinirda koyulastirir");
    vfrBandColor(3200.0 / 3.28084, 10.0, tContour, tRgb);
    vfrBandColor(3200.0 / 3.28084, 10.0, tVfr, tPlain);
    CHECK(near(tRgb[1], tPlain[1], 1e-9), "kontur sinirdan uzakta yok");

    // 3) Su: derinlik tonu ve kiyi seridi
    const TerrainWaterParams tWater;
    CHECK(depthDarkening(-10.0, tWater) == 0.0 && depthDarkening(-5.0, tWater) == 0.0 &&
              depthDarkening(450.0, tWater) == 0.0,
          "-10 (veri yok), kiyi ve gol (>= 0): derinlik tonu yok");
    CHECK(near(depthDarkening(-200.0, tWater), tWater.mDepthTone, 1e-9) &&
              near(depthDarkening(-105.0, tWater), tWater.mDepthTone * 0.5, 1e-9) &&
              near(depthDarkening(-900.0, tWater), tWater.mDepthTone, 1e-9),
          "derinlik tonu dogrusal, 200 m'de tam");
    CHECK(near(coastBandLightening(1.0, 10.0, tWater), tWater.mBandStrength, 1e-9) &&
              coastBandLightening(6.0, 10.0, tWater) < tWater.mBandStrength &&
              coastBandLightening(11.0, 10.0, tWater) == 0.0 && coastBandLightening(1.0, 0.3, tWater) == 0.0,
          "kiyi seridi kiyi cizgisinin (1 piksel) arkasinda en guclu, genislik sonunda 0");

    // 4) Kiyiya uzaklik (3-4 chamfer) ve kiyi cizgisi
    const int tSize = 9;
    std::vector<uint8_t> tLand(static_cast<size_t>(tSize) * tSize, 0);
    for (int tRow = 0; tRow < tSize; ++tRow)
    {
        tLand[static_cast<size_t>(tRow) * tSize] = 1;  // sol sutun kara
    }
    std::vector<float> tDistance;
    distanceToLand(tLand, tSize, 100.0f, &tDistance);
    CHECK(near(tDistance[4 * tSize + 4], 4.0, 1e-6) && tDistance[4 * tSize] == 0.0f, "eksen boyunca uzaklik tam");
    std::vector<uint8_t> tCorner(static_cast<size_t>(tSize) * tSize, 0);
    tCorner[0] = 1;
    distanceToLand(tCorner, tSize, 3.0f, &tDistance);
    CHECK(near(tDistance[3 * tSize + 3], 3.0, 1e-6), "en buyuk uzaklikta kesilir (4.24 -> 3)");
    std::vector<float> tFraction(static_cast<size_t>(8) * 8, 0.0f);
    for (int tRow = 0; tRow < 8; ++tRow)
    {
        for (int tColumn = 0; tColumn < 4; ++tColumn)
        {
            tFraction[static_cast<size_t>(tRow) * 8 + tColumn] = 1.0f;
        }
    }
    tFraction[0] = -1.0f;  // veri yok
    std::vector<float> tLine;
    coastlineStrength(tFraction, 8, &tLine);
    CHECK(tLine[4 * 8 + 4] == 1.0f && tLine[4 * 8 + 3] == 0.0f && tLine[4 * 8 + 5] == 0.0f &&
              tLine[4 * 8 + 0] == 0.0f && tLine[4 * 8 + 7] == 0.0f && tLine[0] == 0.0f,
          "kiyi cizgisi suyun tarafinda tek piksel; karada, uzakta ve veri yok pikselinde yok");
    std::vector<float> tMixed = {1.0f, 0.5f, 0.0f};  // kaba seviye: karisik piksel
    coastlineStrength(tMixed, 1, &tLine);
    CHECK(tLine[0] == 0.0f, "tek tam kara pikseli: cizgi yok");
    std::vector<float> tHalf(9, 0.0f);
    tHalf[4] = 0.5f;
    coastlineStrength(tHalf, 3, &tLine);
    CHECK(near(tLine[4], 1.0, 1e-6) && near(tLine[1], 0.5, 1e-6), "yari kara piksel tam cizgi, komsu su piksel yari");
}

void testTerrainPaletteSettings()
{
    // [relief] [water] [vfr] yaz -> oku ayni; bozuk bant sirasi varsayilana doner
    TerrainPalette tPalette;
    TerrainReliefParams tRelief;
    tRelief.mMultiDirectional = false;
    tRelief.mExaggeration = 3.5;
    tPalette.setRelief(tRelief);
    TerrainWaterParams tWater;
    tWater.mCoastline = false;
    tWater.mBandMeters = 450.0;
    tPalette.setWater(tWater);
    TerrainVfrParams tVfr = defaultVfrParams();
    tVfr.mContourLines = true;
    tVfr.mBandColors[2] = {1, 2, 3};
    tPalette.setVfr(tVfr);
    Settings tSettings;
    tPalette.save(tSettings);
    TerrainPalette tLoaded;
    tLoaded.load(tSettings);
    CHECK(!tLoaded.relief().mMultiDirectional && near(tLoaded.relief().mExaggeration, 3.5, 1e-9) &&
              !tLoaded.water().mCoastline && near(tLoaded.water().mBandMeters, 450.0, 1e-9) &&
              tLoaded.vfr().mContourLines && tLoaded.vfr().mBandColors[2].mB == 3 &&
              near(tLoaded.vfr().mBandFeet[4], 5000.0, 1e-9),
          "yeni ayarlar yazilip okunur");
    tSettings.setString("vfr", "band_3", "500 10 20 30");  // 2. bant 1000 ft'ten kucuk: sira bozuk
    TerrainPalette tBroken;
    tBroken.load(tSettings);
    CHECK(near(tBroken.vfr().mBandFeet[2], 2000.0, 1e-9) &&
              tBroken.vfr().mBandColors[2].mR == defaultVfrParams().mBandColors[2].mR,
          "bozuk bant sirasi: varsayilan bantlar");
    tSettings.setDouble("relief", "exaggeration", 999.0);
    tBroken.load(tSettings);
    CHECK(tBroken.relief().mExaggeration <= 20.0, "aralik disi deger sinirlanir");

    // H-01: yerlesim ezmesi (setColor(TerrainBuiltUp) paleti)
    TerrainPalette tBuiltUpPalette;
    CHECK(!tBuiltUpPalette.hasBuiltUpColor(), "varsayilan: yerlesim ezmesi yok");
    tBuiltUpPalette.setBuiltUpColor({255, 10, 20});
    Settings tBuiltUpSettings;
    tBuiltUpPalette.save(tBuiltUpSettings);
    CHECK(tBuiltUpSettings.getString("built_up", "color", "") == "255 10 20",
          "yerlesim ezmesi [built_up] olarak yazilir");
    TerrainPalette tBuiltUpLoaded;
    tBuiltUpLoaded.load(tBuiltUpSettings);
    CHECK(tBuiltUpLoaded.hasBuiltUpColor() && tBuiltUpLoaded.builtUpColor().mR == 255 &&
              tBuiltUpLoaded.builtUpColor().mG == 10 && tBuiltUpLoaded.builtUpColor().mB == 20,
          "yerlesim ezmesi okunur");
    Settings tPlainSettings;
    TerrainPalette tPlainPalette2;
    tPlainPalette2.save(tPlainSettings);
    CHECK(tPlainSettings.toText({}) != tBuiltUpSettings.toText({}),
          "ezmeli palet farkli anahtarla tani tir (registerPalette ayrisimi)");
    tBuiltUpLoaded.clearBuiltUpColor();
    Settings tClearedSettings;
    tBuiltUpLoaded.save(tClearedSettings);
    CHECK(tClearedSettings.getString("built_up", "color", "").empty(),
          "ezme kalkinca [built_up] yazilmaz");
}

void testTerrainImageRelief()
{
    // Tum karo cimen; DEM batidan doguya 0 -> 3000 m rampa (0 -> ~9800 ft)
    const int tSize = 33;
    auto tRamp = [](int pColumn, int /*pRow*/)
    {
        return 3000.0 * pColumn / 32.0;
    };
    TerrainRaster tRaster;
    tRaster.reset(10, 50);
    const double tLonA[3] = {10.0, 11.0, 10.0};
    const double tLatA[3] = {50.0, 50.0, 51.0};
    const double tLonB[3] = {11.0, 11.0, 10.0};
    const double tLatB[3] = {50.0, 51.0, 51.0};
    tRaster.fillTriangle(tLonA, tLatA, TerrainClass::Grass);
    tRaster.fillTriangle(tLonB, tLatB, TerrainClass::Grass);
    ElevationGrid tGrid;
    tGrid.build(makeDem(tSize, tRamp), nullptr);
    ElevationTiles tTiles;
    tTiles.mTiles[1][1] = &tGrid;

    // 1) VFR bantlari: golge kapali, her piksel ya bir bant rengi ya da sinirda iki bandin karisimi
    Settings tFlat;
    tFlat.setDouble("shading", "strength", 0.0);
    TerrainPalette tPalette;
    tPalette.load(tFlat);
    TerrainImageStyle tVfrStyle;
    tVfrStyle.mStyle = TerrainStyle::Elevation;
    tVfrStyle.mWater = {160, 200, 230};
    std::vector<uint8_t> tImage;
    const int tOut = 128;
    tRaster.buildImage(0, 0, TerrainRaster::kSize, TerrainRaster::kSize, tOut, tPalette, tVfrStyle, &tImage, &tTiles);
    const TerrainVfrParams& tVfr = tPalette.vfr();
    int tPureBand = 0;
    int tMixed = 0;
    int tOther = 0;
    for (int tColumn = 0; tColumn < tOut; ++tColumn)
    {
        const uint8_t* tPixel = tImage.data() + (static_cast<size_t>(64) * tOut + tColumn) * 4;
        bool tIsBand = false;
        for (int tBand = 0; tBand < kVfrBandCount; ++tBand)
        {
            tIsBand = tIsBand || (tPixel[0] == tVfr.mBandColors[tBand].mR && tPixel[1] == tVfr.mBandColors[tBand].mG);
        }
        if (tIsBand)
        {
            ++tPureBand;
            continue;
        }
        bool tIsMix = false;
        for (int tBand = 0; tBand + 1 < kVfrBandCount; ++tBand)
        {
            const int tLowR = std::min(tVfr.mBandColors[tBand].mR, tVfr.mBandColors[tBand + 1].mR);
            const int tHighR = std::max(tVfr.mBandColors[tBand].mR, tVfr.mBandColors[tBand + 1].mR);
            tIsMix = tIsMix || (tPixel[0] >= tLowR && tPixel[0] <= tHighR);
        }
        (tIsMix ? tMixed : tOther) += 1;
    }
    CHECK(tPureBand > tOut - 16 && tOther == 0 && tMixed <= 14,
          "VFR satiri: bant renkleri + sinirlarda en fazla birkac karisik piksel (%d saf, %d karisik, %d diger)",
          tPureBand, tMixed, tOther);

    // 2) Kabartma: dogu yukari egimli rampa batiya bakar -> golgeli goruntu duz goruntuden farkli
    TerrainPalette tShaded;
    std::vector<uint8_t> tWithRelief;
    std::vector<uint8_t> tWithoutRelief;
    tRaster.buildImage(0, 0, TerrainRaster::kSize, TerrainRaster::kSize, 64, tShaded, TerrainImageStyle(),
                       &tWithRelief, &tTiles);
    tRaster.buildImage(0, 0, TerrainRaster::kSize, TerrainRaster::kSize, 64, tShaded, TerrainImageStyle(),
                       &tWithoutRelief);
    const size_t tMiddle = (static_cast<size_t>(32) * 64 + 32) * 4;
    CHECK(tWithoutRelief[tMiddle + 1] == tShaded.colorOf(TerrainClass::Grass).mG &&
              tWithRelief[tMiddle + 1] != tWithoutRelief[tMiddle + 1],
          "rakim verilince kabartma uygulanir, verilmezse sinif rengi");

    // 3) Karo siniri: ayni surekli yuzeyin iki komsu karosu; komsular verilince sinirdaki iki piksel neredeyse ayni
    auto tHill = [](double pX, double pY)
    {
        return 600.0 + 500.0 * std::sin(pX * 2.7 + 0.4) * std::cos(pY * 1.9);
    };
    auto tTileGrid = [&](int pTileX)
    {
        ElevationGrid tTile;
        tTile.build(makeDem(tSize,
                            [&](int pColumn, int pRow)
                            {
                                return std::round(tHill(pTileX + pColumn / 32.0, pRow / 32.0));
                            }),
                    nullptr);
        return tTile;
    };
    const ElevationGrid tWestGrid = tTileGrid(0);
    const ElevationGrid tEastGrid = tTileGrid(1);
    TerrainRaster tEastRaster;
    tEastRaster.reset(11, 50);
    const double tLonC[3] = {11.0, 12.0, 11.0};
    const double tLonD[3] = {12.0, 12.0, 11.0};
    tEastRaster.fillTriangle(tLonC, tLatA, TerrainClass::Grass);
    tEastRaster.fillTriangle(tLonD, tLatB, TerrainClass::Grass);
    ElevationTiles tWestTiles;
    tWestTiles.mTiles[1][1] = &tWestGrid;
    tWestTiles.mTiles[1][2] = &tEastGrid;
    ElevationTiles tEastTiles;
    tEastTiles.mTiles[1][1] = &tEastGrid;
    tEastTiles.mTiles[1][0] = &tWestGrid;
    std::vector<uint8_t> tWestImage;
    std::vector<uint8_t> tEastImage;
    tRaster.buildImage(0, 0, TerrainRaster::kSize, TerrainRaster::kSize, 256, tShaded, TerrainImageStyle(),
                       &tWestImage, &tWestTiles);
    tEastRaster.buildImage(0, 0, TerrainRaster::kSize, TerrainRaster::kSize, 256, tShaded, TerrainImageStyle(),
                           &tEastImage, &tEastTiles);
    int tWorst = 0;
    for (int tRow = 0; tRow < 256; ++tRow)
    {
        const uint8_t* tLeft = tWestImage.data() + (static_cast<size_t>(tRow) * 256 + 255) * 4;
        const uint8_t* tRight = tEastImage.data() + (static_cast<size_t>(tRow) * 256 + 0) * 4;
        for (int tChannel = 0; tChannel < 3; ++tChannel)
        {
            tWorst = std::max(tWorst, std::abs(static_cast<int>(tLeft[tChannel]) - static_cast<int>(tRight[tChannel])));
        }
    }
    CHECK(tWorst <= 3, "karo sinirinda golge dikisi yok (en buyuk kanal farki %d)", tWorst);

    // 4) Su: derinlik tonu sadece Scenery / VFR'de; IFR'da duz su
    auto tSeaLevelGround = [](int /*pColumn*/, int /*pRow*/)
    {
        return 0.0;
    };
    TerrainRaster tSea;
    tSea.reset(10, 50);
    tSea.fillTriangle(tLonA, tLatA, TerrainClass::Water);
    tSea.fillTriangle(tLonB, tLatB, TerrainClass::Water);
    DsfRaster tDeep;
    tDeep.mWidth = 4;
    tDeep.mHeight = 4;
    tDeep.mValues.assign(16, -400.0f);
    ElevationGrid tSeaGrid;
    tSeaGrid.build(makeDem(tSize, tSeaLevelGround), &tDeep);
    ElevationTiles tSeaTiles;
    tSeaTiles.mTiles[1][1] = &tSeaGrid;
    std::vector<uint8_t> tDeepImage;
    tSea.buildImage(0, 0, TerrainRaster::kSize, TerrainRaster::kSize, 64, tShaded, TerrainImageStyle(), &tDeepImage,
                    &tSeaTiles);
    TerrainImageStyle tIfr;
    tIfr.mStyle = TerrainStyle::LandWater;
    tIfr.mLand = {240, 240, 230};
    tIfr.mWater = {200, 220, 230};
    std::vector<uint8_t> tIfrImage;
    tSea.buildImage(0, 0, TerrainRaster::kSize, TerrainRaster::kSize, 64, tShaded, tIfr, &tIfrImage, &tSeaTiles);
    const TerrainColor& tWaterColor = tShaded.colorOf(TerrainClass::Water);
    CHECK(tDeepImage[tMiddle + 2] < tWaterColor.mB - 5 && tIfrImage[tMiddle + 2] == tIfr.mWater.mB,
          "derin deniz koyu (Scenery), IFR'da duz su rengi");
}

void testTerrainBuiltUpColor()
{
    // H-01: setColor(TerrainBuiltUp) ezmesi zemin goruntusune isler. Yerlesim alanlari vektor
    // katmanindan gelir (kAreaBuiltUp); IFR (LandWater) stili yerlesimi karisim rengine boyar.
    TerrainRaster tRaster;
    tRaster.reset(10, 50);
    const double tLonA[3] = {10.0, 11.0, 10.0};
    const double tLatA[3] = {50.0, 50.0, 51.0};
    const double tLonB[3] = {11.0, 11.0, 10.0};
    const double tLatB[3] = {50.0, 51.0, 51.0};
    tRaster.fillTriangle(tLonA, tLatA, TerrainClass::Grass);
    tRaster.fillTriangle(tLonB, tLatB, TerrainClass::Grass);

    TerrainVectors tVectors;
    tVectors.reset(10, 50);
    VectorArea tUrban;
    tUrban.mClass = TerrainClass::Urban;
    tUrban.mRingStarts = {0};
    tUrban.mX = {0.0f, 0.5f, 0.5f, 0.0f};  // karonun bati yarisi
    tUrban.mY = {0.0f, 0.0f, 1.0f, 1.0f};
    tUrban.mBounds = {0.0f, 0.0f, 0.5f, 1.0f};
    tVectors.addArea(tUrban);
    tRaster.fillAreas(tVectors);

    TerrainImageStyle tStyle;
    tStyle.mStyle = TerrainStyle::LandWater;
    tStyle.mLand = {200, 200, 200};
    tStyle.mWater = {160, 200, 230};
    tStyle.mHasBuiltUp = true;
    tStyle.mBuiltUp = {240, 210, 90};  // tema rengi (sectional sari)

    TerrainPalette tBase;
    Settings tFlat;
    tFlat.setDouble("shading", "strength", 0.0);
    tBase.load(tFlat);
    TerrainPalette tOverride = tBase;
    tOverride.setBuiltUpColor({255, 0, 0});  // H-01: harita ezmesi kirmizi

    const int tOut = 64;
    std::vector<uint8_t> tImageBase;
    std::vector<uint8_t> tImageOverride;
    tRaster.buildImage(0, 0, TerrainRaster::kSize, TerrainRaster::kSize, tOut, tBase, tStyle, &tImageBase,
                       nullptr, TerrainRaster::kAreaBuiltUp);
    tRaster.buildImage(0, 0, TerrainRaster::kSize, TerrainRaster::kSize, tOut, tOverride, tStyle,
                       &tImageOverride, nullptr, TerrainRaster::kAreaBuiltUp);

    // Sehir yarisi degisir (kirmiziya kayar), cim yarisi piksel piksel ayni kalir
    int tDiff = 0;
    int tSame = 0;
    int tRedUp = 0;
    for (size_t tPixel = 0; tPixel + 3 < tImageBase.size(); tPixel += 4)
    {
        const bool tChanged = tImageBase[tPixel] != tImageOverride[tPixel] ||
                              tImageBase[tPixel + 1] != tImageOverride[tPixel + 1] ||
                              tImageBase[tPixel + 2] != tImageOverride[tPixel + 2];
        if (tChanged)
        {
            ++tDiff;
            tRedUp += tImageOverride[tPixel] > tImageBase[tPixel] ? 1 : 0;
        }
        else
        {
            ++tSame;
        }
    }
    CHECK(tDiff > tOut * tOut / 4, "yerlesim ezmesi sehir alaninin goruntusunu degistirir (%d piksel)", tDiff);
    CHECK(tSame >= tOut * tOut / 4, "cim alani etkilenmez (%d piksel ayni)", tSame);
    CHECK(tRedUp == tDiff, "degisen pikseller ezme renginin kirmizisina kayar (%d/%d)", tRedUp, tDiff);
}

void testWorldTileMap()
{
    std::vector<uint8_t> tBytes(WorldTileMap::kFileSize, 0);
    tBytes[WorldTileMap::kHeaderSize + WorldTileMap::indexOf(32, 39)] = 255;    // Ankara
    tBytes[WorldTileMap::kHeaderSize + WorldTileMap::indexOf(-123, 47)] = 255;  // Seattle
    tBytes[WorldTileMap::kHeaderSize + WorldTileMap::indexOf(-124, 46)] = 255;
    WorldTileMap tXMap;
    CHECK(!tXMap.loadBytes(std::vector<uint8_t>(100, 255)) && !tXMap.isLoaded(), "yanlis boyut reddedilir");
    CHECK(tXMap.loadBytes(tBytes) && tXMap.hasDsf(32, 39) && !tXMap.hasDsf(33, 39) && !tXMap.hasDsf(32, 40) &&
              tXMap.landTileCount() == 3,
          "boylam oncelikli indeks");
    CHECK(WorldTileMap::indexOf(-180, -90) == 0 && WorldTileMap::indexOf(179, 89) == 360 * 180 - 1 &&
              WorldTileMap::indexOf(-179, -90) == 180,
          "indeks: (boylam + 180) * 180 + (enlem + 90)");
    CHECK(!tXMap.hasDsf(180, 0) && !tXMap.hasDsf(0, 90), "aralik disi karo: yok");

    // Gercek dosya (bu bilgisayarda varsa): bilinen kara ve deniz karolari
    WorldTileMap tReal;
    if (tReal.load("C:/X-Plane 11/Resources/dsf_files"))
    {
        CHECK(tReal.hasDsf(32, 39) && tReal.hasDsf(28, 41) && tReal.hasDsf(-123, 47) && !tReal.hasDsf(34, 43) &&
                  !tReal.hasDsf(-150, 30) && !tReal.hasDsf(-40, 30),
              "gercek dsf_files: Ankara, Istanbul, Seattle kara; Karadeniz ortasi, Pasifik, Atlantik deniz");
    }
    else
    {
        std::printf("note: C:/X-Plane 11/Resources/dsf_files not found, real-file check skipped\n");
    }

    // SceneryIndex: tablo dogrulanir; kurulu olmayan kara = veri yok, tabloda olmayan = deniz
    namespace Fs = std::filesystem;
    const Fs::path tRoot = Fs::temp_directory_path() / "prime_map_plugin_world_test";
    std::error_code tIgnore;
    Fs::remove_all(tRoot, tIgnore);
    Fs::create_directories(tRoot / "Global" / "Earth nav data" / "+40-130");
    std::ofstream(tRoot / "Global" / "Earth nav data" / "+40-130" / "+47-123.dsf") << "global";
    std::ofstream(tRoot / "Global" / "Earth nav data" / "+40-130" / "+46-124.dsf") << "global";
    SceneryIndex tIndex;
    tIndex.buildFromPacks({}, {(tRoot / "Global").string() + "/"});
    tIndex.setWorldMap(tXMap);
    CHECK(tIndex.usesWorldMap() && tIndex.worldMapReport().find("0 mismatches") != std::string::npos,
          "tablo kurulu karolarla dogrulandi: %s", tIndex.worldMapReport().c_str());
    CHECK(tIndex.coverageOf(32, 39) == TileCoverage::NoData, "Ankara: kara ama kurulu degil -> veri yok");
    CHECK(tIndex.coverageOf(-125, 48) == TileCoverage::Ocean && tIndex.coverageOf(34, 43) == TileCoverage::Ocean,
          "tabloda olmayan karo deniz (kurulu bolgede ve disinda)");
    CHECK(tIndex.noBaseCoverage(-123, 47) == TileCoverage::NoData, "sadece overlay'li kara karo: veri yok");

    std::vector<uint8_t> tWrong(WorldTileMap::kFileSize, 0);  // kurulu karolar tabloda yok: yon yanlis
    WorldTileMap tWrongMap;
    tWrongMap.loadBytes(tWrong);
    tIndex.setWorldMap(tWrongMap);
    CHECK(!tIndex.usesWorldMap() && tIndex.worldMapReport().find("REJECTED") != std::string::npos &&
              tIndex.coverageOf(-125, 48) == TileCoverage::Ocean && tIndex.coverageOf(32, 39) == TileCoverage::NoData,
          "dogrulanamayan tablo reddedilir, 10 derece klasor kurali kalir");
    Fs::remove_all(tRoot, tIgnore);
}

void testElevationCache()
{
    // DEM'li test DSF'i gecici bir pakette: karo uretilince rakim dosyasi yazilir, sonra sadece onbellekten okunur
    namespace Fs = std::filesystem;
    const Fs::path tRoot = Fs::temp_directory_path() / "prime_map_plugin_elevation_cache_test";
    std::error_code tIgnore;
    Fs::remove_all(tRoot, tIgnore);
    Fs::create_directories(tRoot / "Global" / "Earth nav data" / "+40+020");
    Fs::create_directories(tRoot / "cache");
    const std::vector<uint8_t> tDsf = makeTestDsf(false, true);
    std::ofstream(tRoot / "Global" / "Earth nav data" / "+40+020" / "+41+029.dsf", std::ios::binary)
        .write(reinterpret_cast<const char*>(tDsf.data()), static_cast<std::streamsize>(tDsf.size()));
    SceneryIndex tIndex;
    tIndex.buildFromPacks({}, {(tRoot / "Global").string() + "/"});
    const std::string tCache = (tRoot / "cache").string() + "/";

    ElevationGrid tMissing;
    CHECK(!TerrainStore::loadElevation(tIndex, 29, 41, tCache, &tMissing), "uretilmeden once onbellekte rakim yok");
    TerrainRaster tRaster;
    ElevationGrid tBuilt;
    std::string tLog;
    TerrainTileState tState = TerrainTileState::Unknown;
    CHECK(TerrainStore::buildRaster(tIndex, 29, 41, tCache, &tRaster, &tLog, &tState, &tBuilt) &&
              tState == TerrainTileState::Ready && tLog.find("built from") != std::string::npos,
          "karo DSF'ten uretildi: %s", tLog.c_str());
    // DEM: yukseklik = 10 * sutun + guneyden satir; kuzey-bati kosesi (sutun 0, guneyden satir 4) = 4
    CHECK(tBuilt.size(0) == 5 && near(tBuilt.post(0, 0, 0), 4.0, 1e-6) && near(tBuilt.post(0, 4, 4), 40.0, 1e-6) &&
              tBuilt.hasSeaLevel() && near(tBuilt.seaLevelCell(1, 1), -50.0, 1e-6),
          "DSF'in DEM'i ve sea_level'i okundu");
    ElevationGrid tCached;
    CHECK(TerrainStore::loadElevation(tIndex, 29, 41, tCache, &tCached) &&
              near(tCached.post(0, 3, 2), tBuilt.post(0, 3, 2), 1e-6),
          "rakim sadece onbellekten okunur (DSF acilmadan)");
    std::string tSecondLog;
    ElevationGrid tSecond;
    CHECK(TerrainStore::buildRaster(tIndex, 29, 41, tCache, &tRaster, &tSecondLog, &tState, &tSecond) &&
              tSecondLog.find("built from") == std::string::npos && tSecond.size(0) == 5,
          "ikinci kez: raster ve rakim onbellekten");
    Fs::remove_all(tRoot, tIgnore);
}

// Asama 2 / 3: DSF vektorleri, alan taramasi, X-Plane doku rengi

// Duzlem: ham kodlama (0) ile degerler (16 veya 32 bit)
void putRawPlane16(std::vector<uint8_t>& pOut, const std::vector<int>& pValues)
{
    putU8(pOut, 0);
    for (const int tValue : pValues)
    {
        putU16(pOut, tValue);
    }
}

void testDsfVectors()
{
    // Karo +41+029. POOL: kare poligon (29.25..29.75 x 41.25..41.75); PO32: 3 noktali yol
    std::vector<uint8_t> tPool;
    putU32(tPool, 4);
    putU8(tPool, 2);
    putRawPlane16(tPool, {16384, 49151, 49151, 16384});  // ~0.25 / ~0.75
    putRawPlane16(tPool, {16384, 16384, 49151, 49151});
    std::vector<uint8_t> tScale;
    for (const float tOffset : {29.0f, 41.0f})
    {
        putF32(tScale, 1.0f);
        putF32(tScale, tOffset);
    }
    std::vector<uint8_t> tPool32;
    putU32(tPool32, 3);
    putU8(tPool32, 2);
    putU8(tPool32, 0);  // ham, 32 bit
    for (const uint32_t tValue : {0u, 2147483647u, 4294967295u})
    {
        putU32(tPool32, tValue);
    }
    putU8(tPool32, 0);
    for (const uint32_t tValue : {2147483647u, 2147483647u, 2147483647u})
    {
        putU32(tPool32, tValue);
    }
    std::vector<uint8_t> tScale32;
    for (const float tOffset : {29.0f, 41.0f})
    {
        putF32(tScale32, 1.0f);
        putF32(tScale32, tOffset);
    }

    std::vector<uint8_t> tCommands;
    putU8(tCommands, 1);  // havuz 0
    putU16(tCommands, 0);
    putU8(tCommands, 3);  // poligon tanimi 0 (orman)
    putU8(tCommands, 0);
    putU8(tCommands, 13);  // poligon araligi 0..4
    putU16(tCommands, 128);
    putU16(tCommands, 0);
    putU16(tCommands, 4);
    putU8(tCommands, 3);  // ag tanimi 0
    putU8(tCommands, 0);
    putU8(tCommands, 6);  // alt tur 10 (ana yol)
    putU8(tCommands, 10);
    putU8(tCommands, 2);  // junction offset 1 -> zincir indeksleri 0,1 -> 1,2
    putU32(tCommands, 1);
    putU8(tCommands, 9);
    putU8(tCommands, 2);
    putU16(tCommands, 0);
    putU16(tCommands, 1);

    std::vector<uint8_t> tDefinitions = atom("TERT", strings({"terrain_Water"}));
    append(tDefinitions, atom("POLY", strings({"lib/g8/coni_tmp_wet.for"})));
    append(tDefinitions, atom("NETW", strings({"lib/g10/roads.net"})));
    std::vector<uint8_t> tGeometry = atom("POOL", tPool);
    append(tGeometry, atom("SCAL", tScale));
    append(tGeometry, atom("PO32", tPool32));
    append(tGeometry, atom("SC32", tScale32));
    std::vector<uint8_t> tFile = {'X', 'P', 'L', 'N', 'E', 'D', 'S', 'F'};
    putU32(tFile, 1);
    append(tFile, atom("HEAD", atom("PROP", strings({"sim/west", "29", "sim/south", "41"}))));
    append(tFile, atom("DEFN", tDefinitions));
    append(tFile, atom("GEOD", tGeometry));
    append(tFile, atom("CMDS", tCommands));
    tFile.resize(tFile.size() + 16, 0);

    DsfReader tReader;
    std::string tError;
    CHECK(tReader.open(tFile, &tError), "vektor DSF'i acilir: %s", tError.c_str());
    std::vector<DsfPolygon> tPolygons;
    std::vector<DsfChain> tChains;
    CHECK(tReader.forEachVector(
              [&](const DsfPolygon& pPolygon)
              {
                  tPolygons.push_back(pPolygon);
              },
              [&](const DsfChain& pChain)
              {
                  tChains.push_back(pChain);
              },
              &tError),
          "vektorler okunur: %s", tError.c_str());
    CHECK(tPolygons.size() == 1 && tPolygons[0].mParameter == 128 && tPolygons[0].mRingLon.size() == 1 &&
              tPolygons[0].mRingLon[0].size() == 4 && near(tPolygons[0].mRingLon[0][1], 29.75, 1e-4) &&
              near(tPolygons[0].mRingLat[0][2], 41.75, 1e-4),
          "poligon 16 bitlik havuzdan, olcekli");
    CHECK(tChains.size() == 1 && tChains[0].mSubtype == 10 && tChains[0].mLon.size() == 2 &&
              near(tChains[0].mLon[0], 29.5, 1e-6) && near(tChains[0].mLon[1], 30.0, 1e-6) &&
              near(tChains[0].mLat[1], 41.5, 1e-6),
          "zincir 32 bitlik havuzdan, junction offset eklenmis (%.6f %.6f)",
          tChains.empty() ? 0.0 : tChains[0].mLon[0], tChains.empty() ? 0.0 : tChains[0].mLon[1]);

    // TerrainVectors: siniflandirma, dislama, onbellek gidis-donus
    TerrainVectors tVectors;
    tVectors.reset(29, 41);
    CHECK(tVectors.addFromDsf(tReader, &tError) && tVectors.areas().size() == 1 && tVectors.lines().size() == 1 &&
              tVectors.areas()[0].mClass == TerrainClass::ForestConifer &&
              tVectors.lines()[0].mKind == VectorLineKind::Primary,
          "orman alani ve ana yol");
    DsfInfo tOverlay;
    tOverlay.mProperties = {{"sim/exclude_net", "29.4/41.4/29.9/41.6"}, {"sim/exclude_for", "29.2/41.2/29.4/41.4"}};
    tVectors.addExclusions(tOverlay);
    tVectors.applyNetworkExclusions();
    CHECK(tVectors.lines().empty(), "dislama alanindaki yol kesildi");
    CHECK(tVectors.isAreaExcluded(TerrainClass::ForestConifer, 0.3, 0.3) &&
              !tVectors.isAreaExcluded(TerrainClass::Urban, 0.3, 0.3) &&
              !tVectors.isAreaExcluded(TerrainClass::ForestConifer, 0.6, 0.6),
          "orman dislamasi sadece ormana ve sadece kendi alaninda");
    CHECK(TerrainVectors::areaClassFor("lib/g10/autogen/urban_low_broken_0.ags") == TerrainClass::Urban &&
              TerrainVectors::areaClassFor("lib/g10/autogen/park_2.ags") == TerrainClass::Grass &&
              TerrainVectors::areaClassFor("lib/g10/autogen/natural.ags") == TerrainClass::None &&
              TerrainVectors::areaClassFor("lib/g8/mixed_tmp_wet.for") == TerrainClass::ForestBroadleaf &&
              TerrainVectors::lineKindFor(120) == static_cast<int>(VectorLineKind::Motorway) &&
              TerrainVectors::lineKindFor(153) == static_cast<int>(VectorLineKind::Railway) &&
              TerrainVectors::lineKindFor(220) == static_cast<int>(VectorLineKind::PowerLine) &&
              TerrainVectors::lineKindFor(1) == -1,
          "tanim adi ve alt tur siniflari");
    namespace Fs = std::filesystem;
    const Fs::path tPath = Fs::temp_directory_path() / "prime_map_plugin_vectors_test.vectors";
    TerrainVectors tLoaded;
    CHECK(tVectors.save(tPath.string(), "k") && tLoaded.load(tPath.string(), "k") && tLoaded.west() == 29 &&
              tLoaded.south() == 41 && tLoaded.areas().size() == 1 &&
              near(tLoaded.areas()[0].mX[1], tVectors.areas()[0].mX[1], 2e-5) &&
              tLoaded.isAreaExcluded(TerrainClass::ForestConifer, 0.3, 0.3) && !tLoaded.load(tPath.string(), "x"),
          "vektor onbellegi gidis-donus");
    std::error_code tIgnore;
    Fs::remove(tPath, tIgnore);

    // Alan taramasi: delikli kare (dis 0.25..0.75, delik 0.4..0.6), 20 x 20 izgara
    VectorArea tArea;
    tArea.mClass = TerrainClass::ForestConifer;
    tArea.mRingStarts = {0, 4};
    tArea.mX = {0.25f, 0.75f, 0.75f, 0.25f, 0.4f, 0.6f, 0.6f, 0.4f};
    tArea.mY = {0.25f, 0.25f, 0.75f, 0.75f, 0.4f, 0.4f, 0.6f, 0.6f};
    tArea.mBounds = {0.25f, 0.25f, 0.75f, 0.75f};
    std::vector<int> tGrid(400, 0);
    fillVectorArea(tArea, 0.0, 1.0, 20.0, 20.0, 20, 20,
                   [&](int pColumn, int pRow)
                   {
                       tGrid[static_cast<size_t>(pRow) * 20 + pColumn] = 1;
                   });
    // Piksel merkezi (c + 0.5) / 20: dis kare sutun 5..14, delik sutun 8..11
    CHECK(tGrid[6 * 20 + 5] == 1 && tGrid[6 * 20 + 14] == 1 && tGrid[6 * 20 + 4] == 0 && tGrid[6 * 20 + 15] == 0 &&
              tGrid[10 * 20 + 9] == 0 && tGrid[10 * 20 + 6] == 1,
          "alan taramasi: kenarlar ve delik (cift-tek)");
}

void testDdsColor()
{
    // 8 x 8 DXT1, tek mip: butun bloklar c0 = saf kirmizi, c1 = saf mavi, indeks 0 ve 1 yari yari
    std::vector<uint8_t> tFile = {'D', 'D', 'S', ' '};
    std::vector<uint8_t> tHeader(124, 0);
    auto tSet = [&](size_t pOffset, uint32_t pValue)
    {
        for (int tByte = 0; tByte < 4; ++tByte)
        {
            tHeader[pOffset - 4 + tByte] = static_cast<uint8_t>((pValue >> (8 * tByte)) & 0xFF);
        }
    };
    tSet(12, 8);
    tSet(16, 8);
    tSet(28, 1);
    tSet(80, 0x4);
    tSet(84, 'D' | ('X' << 8) | ('T' << 16) | ('1' << 24));
    append(tFile, tHeader);
    for (int tBlock = 0; tBlock < 4; ++tBlock)
    {
        putU16(tFile, 0xF800);  // kirmizi (c0 > c1: 4 renk modu)
        putU16(tFile, 0x001F);  // mavi
        putU32(tFile, 0x44444444u);  // her pikselde indeks 0 ve 1 sirayla (0,1,0,1 ...)
    }
    namespace Fs = std::filesystem;
    const Fs::path tPath = Fs::temp_directory_path() / "prime_map_plugin_test.dds";
    std::ofstream(tPath, std::ios::binary).write(reinterpret_cast<const char*>(tFile.data()),
                                                 static_cast<std::streamsize>(tFile.size()));
    TerrainColor tColor;
    CHECK(averageDdsColor(tPath.string(), &tColor) && std::abs(tColor.mR - 128) <= 1 && tColor.mG == 0 &&
              std::abs(tColor.mB - 128) <= 1,
          "DXT1 ortalama rengi (%d %d %d)", tColor.mR, tColor.mG, tColor.mB);
    std::error_code tIgnore;
    Fs::remove(tPath, tIgnore);
}

// Atomik dosya yazimi: ayni hedefe 2 is x 1000 yazim. Hedef her zaman tam icerik gosterir
// (yarim yazim gorunmez), hic gecici dosya kalmaz (1.0.0, NF-REL).
void testAtomicFile()
{
    namespace Fs = std::filesystem;
    const Fs::path tDir = Fs::temp_directory_path() / "prime_map_atomic_test";
    Fs::create_directories(tDir);
    const std::string tTarget = (tDir / "target.bin").string();
    const std::string tPayloadA(64, 'A');
    const std::string tPayloadB(64, 'B');

    auto tWriter = [&](const std::string& pPayload)
    {
        for (int tRound = 0; tRound < 1000; ++tRound)
        {
            const bool tOk = writeFileAtomic(tTarget,
                                             [&pPayload](std::ofstream& pFile)
                                             {
                                                 pFile.write(pPayload.data(),
                                                             static_cast<std::streamsize>(pPayload.size()));
                                             });
            CHECK(tOk, "atomik yazim basarili");
            // Okuyucu: replace'in ucundan (kisa bir an acilamayan ad) birkac denemeyle gecer
            std::string tContent;
            for (int tTry = 0; tTry < 10 && tContent.size() < pPayload.size(); ++tTry)
            {
                std::ifstream tReader(tTarget, std::ios::binary);
                if (tReader)
                {
                    tContent.assign((std::istreambuf_iterator<char>(tReader)), std::istreambuf_iterator<char>());
                }
                if (tContent.size() < pPayload.size())
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
            }
            // Yarim yazim asla gorunmez: hedefte ya A ya B'nin tamami var
            CHECK(tContent == tPayloadA || tContent == tPayloadB, "okuyan ya eski ya tam yenisini gorur (%zu '%c')",
                  tContent.size(), tContent.empty() ? '?' : tContent[0]);
        }
    };
    std::thread tFirst(tWriter, std::cref(tPayloadA));
    std::thread tSecond(tWriter, std::cref(tPayloadB));
    tFirst.join();
    tSecond.join();

    int tTempFiles = 0;
    for (const Fs::path& tEntry : Fs::directory_iterator(tDir))
    {
        if (tEntry.filename().string().find(".tmp") != std::string::npos)
        {
            ++tTempFiles;
        }
    }
    CHECK(tTempFiles == 0, "hic gecici dosya kalmaz");
    std::error_code tIgnore;
    Fs::remove_all(tDir, tIgnore);
}

// Prime XMap API (katman tablosu, panel kurallari, ayar metni)

// Gercek katmanlarin kimlik ve alt katmanlarini taklit eden sahte katman
class ApiFakeLayer : public Layer
{
public:
    ApiFakeLayer(const char* pId, bool pVisible, std::initializer_list<const char*> pSubs)
        : Layer(pId, pId, pVisible)
    {
        for (const char* tSub : pSubs)
        {
            addSubLayer(tSub, tSub);
        }
    }

    void draw(const drawContext& /*pContext*/) override
    {
    }
};

void testApiLayers()
{
    using prime::XMapLayer;
    // 1) Tablo: her enum ogesinin anahtari geri ayni ogeye doner; ust ogeler tutarli
    for (int tIndex = 0; tIndex < static_cast<int>(XMapLayer::Count); ++tIndex)
    {
        const XMapLayer tLayer = static_cast<XMapLayer>(tIndex);
        const std::optional<XMapLayer> tBack = LayerRules::layerFromKey(LayerRules::layerKey(tLayer));
        CHECK(tBack && *tBack == tLayer, "layer key round trip %d (%s)", tIndex, LayerRules::layerKey(tLayer));
        const std::optional<XMapLayer> tParent = LayerRules::parentOf(tLayer);
        switch (LayerRules::kindOf(tLayer))
        {
            case prime::LayerKind::Group:
                CHECK(!tParent, "group has no parent %d", tIndex);
                break;
            case prime::LayerKind::Layer:
                CHECK(!tParent || LayerRules::kindOf(*tParent) == prime::LayerKind::Group, "layer parent %d", tIndex);
                break;
            case prime::LayerKind::SubLayer:
                CHECK(tParent && LayerRules::kindOf(*tParent) == prime::LayerKind::Layer, "sub parent %d", tIndex);
                break;
        }
    }
    CHECK(LayerRules::topLevelLayers().size() == 6, "6 top level items (5 groups + drawings)");
    CHECK(LayerRules::childrenOf(XMapLayer::AirportsGroup).size() == 5, "airports group has 5 layers");
    CHECK(LayerRules::childrenOf(XMapLayer::Runways).size() == 6, "runways has 6 sub layers");
    CHECK(std::string(LayerRules::layerKey(XMapLayer::RunwaysWater)) == "runways.water", "runways.water key");
    CHECK(std::string(LayerRules::layerName(XMapLayer::RunwaysWater)) == "Water runways", "runways.water name");
    CHECK(!LayerRules::layerFromKey("nonsense"), "unknown key");
    CHECK(!LayerRules::isLayerDefaultVisible(XMapLayer::TaxiRoutes, prime::XMapMode::Scenery), "taxi off");
    CHECK(LayerRules::isLayerDefaultVisible(XMapLayer::Terrain, prime::XMapMode::Scenery), "terrain on");
    CHECK(!LayerRules::isLayerDefaultVisible(XMapLayer::Fixes, prime::XMapMode::Scenery), "fixes off (scenery)");
    CHECK(LayerRules::isLayerDefaultVisible(XMapLayer::Fixes, prime::XMapMode::IfrHigh), "fixes on (ifr high)");

    // 0.26.2 -> 1.0.0: varsayilan harita modu Dark; Dark zemin LandWater, Terrain acik
    CHECK(prime::XMapConfig{}.mMode == prime::XMapMode::Dark, "default map mode is dark");
    CHECK(themeFor(XMapMode::Dark).mTerrainStyle == TerrainStyle::LandWater, "dark ground is land/water");
    CHECK(LayerRules::isLayerDefaultVisible(XMapLayer::Terrain, prime::XMapMode::Dark), "terrain on (dark)");

    // 2) kurallar(katman paneliyle ayni)
    LayerManager tLayers;
    tLayers.add(std::make_unique<ApiFakeLayer>("airports", true, std::initializer_list<const char*>{}));
    tLayers.add(std::make_unique<ApiFakeLayer>(
        "runways", true, std::initializer_list<const char*>{"paved", "unpaved", "water", "overruns", "markings",
                                                            "numbers"}));
    tLayers.add(std::make_unique<ApiFakeLayer>("airport_surfaces", true,
                                               std::initializer_list<const char*>{"pavement", "boundary"}));
    tLayers.add(std::make_unique<ApiFakeLayer>("airport_objects", false, std::initializer_list<const char*>{}));
    tLayers.add(std::make_unique<ApiFakeLayer>("taxi_routes", false, std::initializer_list<const char*>{}));
    const XMapTheme& tTheme = themeFor(XMapMode::Scenery);
    tLayers.resetToDefaults(tTheme);

    // Kapali katmanin alt katmani: sadece o acilir, katman "yarim"
    LayerRules::setVisible(tLayers, tTheme, XMapLayer::Runways, false);
    CHECK(LayerRules::state(tLayers, XMapLayer::Runways) == prime::LayerState::Off, "runways off");
    CHECK(!LayerRules::isVisible(tLayers, XMapLayer::RunwaysWater), "sub hidden with parent");
    LayerRules::setVisible(tLayers, tTheme, XMapLayer::RunwaysWater, true);
    CHECK(LayerRules::isVisible(tLayers, XMapLayer::RunwaysWater), "water on");
    CHECK(!LayerRules::isVisible(tLayers, XMapLayer::RunwaysPaved), "paved stays off");
    CHECK(LayerRules::state(tLayers, XMapLayer::Runways) == prime::LayerState::Mixed, "runways mixed");
    // Son alt katman kapaninca katman da kapanir
    LayerRules::toggle(tLayers, tTheme, XMapLayer::RunwaysWater);
    CHECK(LayerRules::state(tLayers, XMapLayer::Runways) == prime::LayerState::Off, "last sub closes layer");
    // Butun alt katmanlari kapali katman acilinca hepsi acilir
    LayerRules::setVisible(tLayers, tTheme, XMapLayer::Runways, true);
    CHECK(LayerRules::state(tLayers, XMapLayer::Runways) == prime::LayerState::On, "layer opens all subs");

    // Grup: kapat -> hepsi kapali; ac -> sadece varsayilan acik olanlar (taxi routes / objects kapali kalir)
    LayerRules::setVisible(tLayers, tTheme, XMapLayer::AirportsGroup, false);
    CHECK(LayerRules::state(tLayers, XMapLayer::AirportsGroup) == prime::LayerState::Off, "group off");
    LayerRules::toggle(tLayers, tTheme, XMapLayer::AirportsGroup);
    CHECK(LayerRules::isVisible(tLayers, XMapLayer::AirportSymbols), "group on: symbols");
    CHECK(!LayerRules::isVisible(tLayers, XMapLayer::TaxiRoutes), "group on: taxi routes stay off");
    CHECK(LayerRules::state(tLayers, XMapLayer::AirportsGroup) == prime::LayerState::Mixed, "group mixed");
    // Ikinci tiklama: varsayilanlar acik -> hepsi kapanir
    LayerRules::toggle(tLayers, tTheme, XMapLayer::AirportsGroup);
    CHECK(LayerRules::state(tLayers, XMapLayer::AirportsGroup) == prime::LayerState::Off, "group toggled off");
    // Sadece verilenler
    const std::array<XMapLayer, 2> tOnly = {XMapLayer::TaxiRoutes, XMapLayer::SurfacesBoundary};
    LayerRules::showOnly(tLayers, tTheme, tOnly.data(), tOnly.data() + tOnly.size());
    CHECK(LayerRules::isVisible(tLayers, XMapLayer::TaxiRoutes), "show only: taxi");
    CHECK(LayerRules::isVisible(tLayers, XMapLayer::SurfacesBoundary), "show only: boundary");
    CHECK(!LayerRules::isVisible(tLayers, XMapLayer::SurfacesPavement), "show only: pavement off");
    CHECK(!LayerRules::isVisible(tLayers, XMapLayer::Runways), "show only: runways off");
    CHECK(LayerRules::groupFromPanelName("Airports") == XMapLayer::AirportsGroup, "panel group name");

    // 3) Ayar metni (JSON): toText / loadText / mergeText ve bolum suzgeci
    Settings tSettings;
    tSettings.setString("view", "mode", "vfr");
    tSettings.setBool("layers.vfr", "runways.water", false);
    tSettings.setInt("window", "left", 12);
    const std::string tText = tSettings.toText({"view", "layers*"});
    CHECK(tText.find("\"window\"") == std::string::npos && tText.find("\"view\"") != std::string::npos,
          "window section filtered out");
    Settings tBack;
    tBack.loadText(tText);
    CHECK(tBack.getString("view", "mode", "") == "vfr", "mode round trip");
    CHECK(!tBack.getBool("layers.vfr", "runways.water", true), "layer round trip");
    tBack.mergeText("{ \"view\": { \"zoom\": \"9.5\" } }");
    Settings tNotJson;
    tNotJson.loadText("[view]\nzoom=9.5\n");
    CHECK(!tNotJson.has("view", "zoom"), "JSON olmayan metin yok sayilir");
    CHECK(tBack.getDouble("view", "zoom", 0.0) == 9.5 && tBack.getString("view", "mode", "") == "vfr", "merge");

    // 4) Cizimler: tek sekil silme
    Drawings tDrawings;
    tDrawings.setMode(Drawings::Mode::Point);
    tDrawings.addVertex({1.0, 2.0});
    tDrawings.addVertex({3.0, 4.0});
    CHECK(tDrawings.remove(Drawings::ShapeKind::Point, 1) && tDrawings.shapeCount() == 1, "remove shape P1");
    CHECK(!tDrawings.remove(Drawings::ShapeKind::Point, 1), "remove missing shape");
    CHECK(Drawings::labelOf(tDrawings.shapes()[0]) == "P2", "remaining label");
}

// harita basina zoom sinirlari, katman zoom araliklari, palet anahtari
void testMultiMapCore()
{
    // 1) XMapView zoom sinirlari: varsayilan 3..16.5; daraltilir / 18'e kadar genisletilir
    XMapView tView;
    tView.setZoom(20.0);
    CHECK(std::abs(tView.zoom() - XMapView::kMaxZoom) < 1e-9, "varsayilan ust sinir 16.5");
    tView.setZoomLimits(6.0, 12.0);
    CHECK(std::abs(tView.zoom() - 12.0) < 1e-9, "sinir daralinca gecerli zoom kirpilir: %.2f", tView.zoom());
    tView.setZoom(1.0);
    CHECK(std::abs(tView.zoom() - 6.0) < 1e-9, "alt sinir uygulanir");
    tView.setZoomLimits(0.0, 30.0);
    CHECK(std::abs(tView.minZoomLimit() - XMapView::kMinZoom) < 1e-9 &&
              std::abs(tView.maxZoomLimit() - XMapView::kHardMaxZoom) < 1e-9,
          "mutlak sinirlar: %.1f..%.1f", tView.minZoomLimit(), tView.maxZoomLimit());
    tView.setZoom(17.5);
    CHECK(std::abs(tView.zoom() - 17.5) < 1e-9, "genisletilmis ust sinirda 17.5'e cikilir");

    // 2) Katman zoom araligi: aralik disinda cizilmez, kutu durumu degismez
    std::string tTrace;
    LayerManager tLayers;
    tLayers.add(std::make_unique<CountingLayer>("a", true, &tTrace));
    tLayers.add(std::make_unique<CountingLayer>("b", true, &tTrace));
    tLayers.add(std::make_unique<PartedLayer>());
    tLayers.at(0)->setZoomRange(8.0, 12.0);
    XMapView tDrawView;
    tDrawView.setViewport({0.0, 100.0, 100.0, 0.0});
    drawContext tContext;
    tContext.mView = &tDrawView;
    tDrawView.setZoom(6.0);
    tLayers.drawAll(tContext);
    CHECK(tTrace == "Db Lb ", "zoom 6: araligi 8..12 olan katman cizilmez: '%s'", tTrace.c_str());
    CHECK(tLayers.at(0)->isVisible() && !tLayers.at(0)->isInZoomRange(), "kutu acik kalir, aralik disi");
    tTrace.clear();
    tDrawView.setZoom(10.0);
    tLayers.drawAll(tContext);
    CHECK(tTrace == "Da Db La Lb ", "zoom 10: aralik icinde cizilir: '%s'", tTrace.c_str());
    tTrace.clear();
    tDrawView.setZoom(13.0);
    tLayers.drawAll(tContext);
    CHECK(tTrace == "Db Lb ", "zoom 13: ust sinirin disinda: '%s'", tTrace.c_str());

    // 3) Alt katman araligi: isSubLayerDrawn zoom'a bakar, isSubLayerVisible bakmaz
    Layer& tParted = *tLayers.at(2);
    tParted.setSubLayerZoomRange(0, 11.0, Layer::kNoMaxZoom);
    tLayers.setCurrentZoom(9.0);
    CHECK(tParted.isSubLayerVisible(0) && !tParted.isSubLayerDrawn(0), "alt katman zoom 9'da cizilmez");
    tLayers.setCurrentZoom(11.5);
    CHECK(tParted.isSubLayerDrawn(0) && !tParted.isSubLayerDrawn(1), "zoom 11.5: acik olan cizilir, kapali cizilmez");
    CHECK(tParted.isSubLayerDrawn(9), "gecersiz indeks cizilir sayilir");

    // 4) Ayar dosyasi: [layer_zoom] sadece sinirli katmanlari tasir, geri okunur, kaldirilan satir silinir
    const uint64_t tSignature = tLayers.visibilitySignature();
    Settings tSettings;
    tLayers.saveZoomRanges(tSettings);
    CHECK(tSettings.has("layer_zoom", "a") && !tSettings.has("layer_zoom", "b") &&
              tSettings.has("layer_zoom", "parted.one") && !tSettings.has("layer_zoom", "parted.two"),
          "sadece sinirli katmanlar yazilir");
    tLayers.resetZoomRanges();
    CHECK(tLayers.visibilitySignature() != tSignature, "aralik degisince imza degisir (resim yeniden cizilir)");
    CHECK(!tLayers.at(0)->hasHostZoomRange() && std::abs(tLayers.at(0)->minZoomEff() - Layer::kNoMinZoom) < 1e-9 &&
              std::abs(tLayers.at(0)->maxZoomEff() - Layer::kNoMaxZoom) < 1e-9,
          "sifirlama: host araligi kalkar, yerlesik esige doner");
    tLayers.loadZoomRanges(tSettings);
    CHECK(std::abs(tLayers.at(0)->minZoomEff() - 8.0) < 1e-9 && std::abs(tLayers.at(0)->maxZoomEff() - 12.0) < 1e-9 &&
              std::abs(tParted.subLayerAt(0).mMinZoom - 11.0) < 1e-9,
          "araliklar geri okunur");
    CHECK(tLayers.visibilitySignature() == tSignature, "geri okununca imza ayni");
    tLayers.at(0)->clearZoomRange();
    tLayers.saveZoomRanges(tSettings);
    CHECK(!tSettings.has("layer_zoom", "a") && tSettings.has("layer_zoom", "parted.one"),
          "siniri kaldirilan katmanin satiri silinir");
    tSettings.setString("layer_zoom", "b", "12 9");  // ters aralik: yok sayilir
    tLayers.loadZoomRanges(tSettings);
    CHECK(!tLayers.at(1)->hasHostZoomRange(), "gecersiz aralik yok sayilir");

    // 4b) Yerlesik esikler (1.0.0, ZOOM-02): tablo + kirpma + "host araligi yerlesikin yerine gecer"
    const LayerRules::LayerZoomDefaults tTerrain = LayerRules::zoomDefaultsOf(prime::XMapLayer::Terrain);
    CHECK(std::abs(tTerrain.mMinZoom - 5.0) < 1e-9 && std::abs(tTerrain.mMaxZoom - 99.0) < 1e-9 &&
              std::abs(tTerrain.mHardMinZoom - 4.0) < 1e-9,
          "tablo: terrain 5.0 / 99 / sert 4.0");
    CHECK(std::abs(LayerRules::zoomDefaultsOf(prime::XMapLayer::AirwaysLow).mMinZoom - 6.5) < 1e-9,
          "tablo: airways_low 6.5");
    CHECK(std::abs(LayerRules::zoomDefaultsOf(prime::XMapLayer::RoadsMotorways).mMinZoom - 7.5) < 1e-9,
          "tablo: roads.motorways 7.5");
    const LayerRules::LayerZoomDefaults tNone = LayerRules::zoomDefaultsOf(prime::XMapLayer::Grid);
    CHECK(tNone.mMinZoom == 0.0 && tNone.mMaxZoom == 99.0 && tNone.mHardMinZoom == 0.0, "tabloda olmayan: sinirsiz");
    // Yerlesik esik davranisi: tablodaki degerler dogrudan katmana uygulanarak denenir
    class BuiltInLayer : public Layer
    {
    public:
        BuiltInLayer()
            : Layer("a", "A", true)
        {
            mMotorways = addSubLayer("motorways", "Motorways");
            mLocal = addSubLayer("local", "Local roads");
        }
        void draw(const drawContext&) override
        {
        }
        int mMotorways = 0;
        int mLocal = 0;
    };
    BuiltInLayer tBuiltIn;
    tBuiltIn.setBuiltInZoomRange(5.0, 99.0, 4.0);
    tBuiltIn.setCurrentZoom(4.5);
    CHECK(!tBuiltIn.isInZoomRange(), "yerlesik esik 5.0: zoom 4.5'te cizilmez");
    tBuiltIn.setZoomRange(4.0, 99.0);  // host yerlesik esigi asagi ceker (iki yon)
    CHECK(tBuiltIn.isInZoomRange(), "host 4.0 verdi: yerlesik esigin yerine gecer");
    tBuiltIn.setZoomRange(3.0, 99.0);  // sert sinirin alti: kirpilir
    CHECK(std::abs(tBuiltIn.minZoomEff() - 4.0) < 1e-9, "3.0 istegi sert sinira kirpilir (etkin 4.0)");
    tBuiltIn.clearZoomRange();
    CHECK(!tBuiltIn.hasHostZoomRange() && std::abs(tBuiltIn.minZoomEff() - 5.0) < 1e-9,
          "clear: yerlesik esige doner");
    // Alt katman: tablodaki yerlesik esik + host ezmesi
    const int tMotorways = tBuiltIn.mMotorways;
    const int tLocal = tBuiltIn.mLocal;
    tBuiltIn.setSubLayerBuiltInZoomRange(tMotorways, 7.5, 99.0, 6.5);
    tBuiltIn.setSubLayerBuiltInZoomRange(tLocal, 12.0, 99.0, 11.0);
    tBuiltIn.setCurrentZoom(8.0);
    CHECK(tBuiltIn.isSubLayerDrawn(tMotorways) && !tBuiltIn.isSubLayerDrawn(tLocal),
          "zoom 8: motorways yerlesik esigi 7.5'te cizilir, local 12'de cizilmez");
    tBuiltIn.setSubLayerZoomRange(tMotorways, 5.0, 99.0);
    CHECK(std::abs(tBuiltIn.subMinZoomEff(tMotorways) - 6.5) < 1e-9,
          "alt katmanda 5.0 istegi sert sinira kirpilir (etkin 6.5)");
    tBuiltIn.clearSubLayerZoomRange(tMotorways);
    CHECK(std::abs(tBuiltIn.subMinZoomEff(tMotorways) - 7.5) < 1e-9, "alt katman clear: yerlesik 7.5'e doner");
    CHECK(std::abs(tBuiltIn.subHardMinZoom(tMotorways) - 6.5) < 1e-9, "alt katman sert siniri tablodan");

    // 5) Settings::clearSection
    Settings tClear;
    tClear.setInt("one", "x", 1);
    tClear.setInt("two", "y", 2);
    tClear.clearSection("one");
    CHECK(!tClear.has("one", "x") && tClear.has("two", "y"), "bolum silme");

    // 6) Zemin anahtari: palet numarasi farkli = farkli doku (haritalar arasi karismaz)
    TerrainImageKey tFirst = {-123, 47, 2, 0, 0, 0};
    TerrainImageKey tSecond = tFirst;
    tSecond.mPalette = 1;
    CHECK(!(tFirst == tSecond) && TerrainImageKeyHash()(tFirst) != TerrainImageKeyHash()(tSecond),
          "palet anahtara ve ozete girer");
    tSecond.mPalette = 0;
    CHECK(tFirst == tSecond, "ayni palet = ayni doku (paylasilir)");

    // 7) Depo: palet ekleme (Start'tan once de), bellek olcegi ve sinir cagrilari guvenli
    TerrainStore tStore;
    TerrainPalette tPalette;
    CHECK(tStore.addPalette(tPalette) == 0 && tStore.addPalette(tPalette) == 1, "palet numaralari sirayla");
    tStore.setMemoryScale(0.2);  // 1'in altina inmez
    tStore.setMemoryScale(2.5);
    tStore.setCacheLimit(123u << 20);
}

int main()
{
    testApiLayers();
    testMultiMapCore();
    testScreenMath();
    testMercator();
    testMapView();
    testGeo();
    testMapTheme();
    testDrawings();
    testMapFormat();
    testMesh();
    testSettings();
    testLayerManager();
    testSubLayers();
    testNavDatabase();
    testLabelPlacer();
    testMapViewBounds();
    testAptLine();
    testAptDatScanner();
    testAptDatDetail();
    testAptIndexCache();
    testAirportShapes();
    testAirportGeometryStore();
    testStoreCancel();
    testLocalizerMatch();
    testDrawingsFile();
    testAirways();
    testLabelPriority();
    testDsfReader();
    testDsfCommandCount();
    testTerrain();
    testLandBlend();
    testTerrainWarp();
    testSceneryIndex();
    testElevationCodec();
    testElevationGrid();
    testElevationSampler();
    testTerrainRelief();
    testTerrainPaletteSettings();
    testTerrainImageRelief();
    testTerrainBuiltUpColor();
    testWorldTileMap();
    testElevationCache();
    testDsfVectors();
    testDdsColor();
    testAtomicFile();
    std::printf("%d checks passed, %d failed\n", gPassCount, gFailCount);
    return gFailCount == 0 ? 0 : 1;
}
