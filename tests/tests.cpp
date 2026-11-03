// tests.cpp
// X-Plane OLMADAN calisan testler: projeksiyon, koordinat matematigi,
// cografi hesaplar, cizim modeli, ayar dosyasi, katman yoneticisi.
//
// Derleme: cmake -DMAP_TESTS=ON ... ; calistirma: map_tests(.exe)
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "Drawings.h"
#include "Geo.h"
#include "LabelPlacer.h"
#include "LayerManager.h"
#include "MapFormat.h"
#include "MapView.h"
#include "ScreenMath.h"
#include "Settings.h"
#include "data/AirportGeometryStore.h"
#include "data/AptDatReader.h"
#include "data/LocalizerMatch.h"
#include "data/NavDatabase.h"

#include <chrono>
#include <thread>

// Log.cpp (XPLMDebugString kullanir) yerine bos bir taklit
void Log(const char* /*pFormat*/, ...)
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

void TestMapFormat()
{
    char tText[64];
    FormatLatLon({41.26201, -28.74205}, tText, sizeof(tText));
    CHECK(std::strcmp(tText, "41.26201 N  28.74205 W") == 0, "lat/lon bicimi: '%s'", tText);

    FormatGridLabel(41.0, true, 1.0, tText, sizeof(tText));
    CHECK(std::strcmp(tText, "41N") == 0, "izgara etiketi derece: '%s'", tText);
    FormatGridLabel(41.5, true, 0.5, tText, sizeof(tText));
    CHECK(std::strcmp(tText, "41 30N") == 0, "izgara etiketi dakika: '%s'", tText);
    FormatGridLabel(28.0 + 5.5 / 60.0, false, 0.5 / 60.0, tText, sizeof(tText));
    CHECK(std::strcmp(tText, "28 05.5E") == 0, "izgara etiketi yarim dakika: '%s'", tText);
    FormatGridLabel(-(12.0 + 20.0 / 60.0), false, 10.0 / 60.0, tText, sizeof(tText));
    CHECK(std::strcmp(tText, "12 20W") == 0, "izgara etiketi bati: '%s'", tText);
    FormatGridLabel(29.0 + 59.9999999 / 60.0, true, 1.0 / 60.0, tText, sizeof(tText));
    CHECK(std::strcmp(tText, "30 00N") == 0, "yuvarlama tasmasi: '%s'", tText);

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

    Settings tSettings;
    tLayers.At(0)->SetVisible(false);
    tLayers.SaveVisibility(tSettings);
    CHECK(!tSettings.GetBool("layers", "a", true) && !tSettings.GetBool("layers", "b", true) &&
              tSettings.GetBool("layers", "c", false),
          "gorunurluk kaydi");

    tSettings.SetBool("layers", "b", true);
    tLayers.At(0)->SetVisible(true);
    tLayers.LoadVisibility(tSettings);
    CHECK(!tLayers.At(0)->IsVisible() && tLayers.At(1)->IsVisible() && tLayers.At(2)->IsVisible(),
          "gorunurluk okuma");

    Settings tEmpty;
    tLayers.LoadVisibility(tEmpty);
    CHECK(!tLayers.At(0)->IsVisible() && tLayers.At(1)->IsVisible(), "ayar yoksa mevcut durum korunur");
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

void FeedLines(AptDatParser& pParser, const char* pText)
{
    const char* tLineStart = pText;
    for (const char* tCursor = pText;; ++tCursor)
    {
        if (*tCursor == '\n' || *tCursor == '\0')
        {
            pParser.ParseLine(tLineStart, static_cast<size_t>(tCursor - tLineStart));
            if (*tCursor == '\0')
            {
                break;
            }
            tLineStart = tCursor + 1;
        }
    }
    pParser.Finish();
}

void TestAptDatReader()
{
    // Gercek apt.dat satiri (Global Airports, LTAC)
    const char* tLtacRunway = "100 59.00 1 0 0.25 1 2 0 03R 40.1140623 32.9831635 0 65 3 2 1 0 "
                              "21L 40.1413103 33.0091255 0 60 3 2 1 0";
    Runway tRunway;
    CHECK(AptDatParser::ParseRunway(tLtacRunway, std::strlen(tLtacRunway), &tRunway), "pist satiri okunmali");
    CHECK(Near(tRunway.mWidthM, 59.0, 1e-9) && tRunway.mSurface == RunwaySurface::Paved, "genislik ve yuzey");
    CHECK(tRunway.mEnds[0].mName == "03R" && tRunway.mEnds[1].mName == "21L", "pist numaralari");
    CHECK(Near(tRunway.mEnds[0].mPosition.mLat, 40.1140623, 1e-9) &&
              Near(tRunway.mEnds[1].mPosition.mLon, 33.0091255, 1e-9),
          "pist uclari");
    CHECK(Near(tRunway.mEnds[0].mOverrunM, 65.0, 1e-9) && Near(tRunway.mEnds[1].mOverrunM, 60.0, 1e-9),
          "overrun");
    const double tLength = DistanceNm(tRunway.mEnds[0].mPosition, tRunway.mEnds[1].mPosition) * 1852.0;
    CHECK(tLength > 3700 && tLength < 3800, "LTAC 03R/21L boyu %.0f m (gercek 3750 m)", tLength);

    const char* tGrass = "100 30 3 0 0.25 0 0 0 09 41.0 29.0 150.5 0 1 0 0 0 27 41.0 29.02 0 0 1 0 0 0\r";
    CHECK(AptDatParser::ParseRunway(tGrass, std::strlen(tGrass), &tRunway) &&
              tRunway.mSurface == RunwaySurface::Unpaved && Near(tRunway.mEnds[0].mDisplacedThresholdM, 150.5, 1e-9),
          "cim pist, kayik esik, CRLF");
    const char* tBroken = "100 30 3 0 0.25 0 0 0 09 41.0";
    CHECK(!AptDatParser::ParseRunway(tBroken, std::strlen(tBroken), &tRunway), "eksik satir reddedilir");

    std::string tIcao;
    std::string tName;
    const char* tHeader = "1   3125 0 0 LTAC Esenboga Intl \r";
    CHECK(AptDatParser::ParseAirportHeader(tHeader, std::strlen(tHeader), &tIcao, &tName) && tIcao == "LTAC" &&
              tName == "Esenboga Intl",
          "baslik: '%s' '%s'", tIcao.c_str(), tName.c_str());
    const char* tNegative = "1 -12 0 0 EHAM Schiphol";
    CHECK(AptDatParser::ParseAirportHeader(tNegative, std::strlen(tNegative), &tIcao, &tName) && tIcao == "EHAM",
          "deniz seviyesi alti rakim");

    // Iki "dosya": once gelen kazanir, pistsizler (heliport) alinmaz
    std::unordered_set<std::string> tSeen;
    std::vector<AirportGeometry> tAirports;
    {
        AptDatParser tFirst(tSeen, tAirports);
        FeedLines(tFirst, "I\n1130 Version\n\n"
                          "1 3125 0 0 LTAC Esenboga\n"
                          "1302 city Ankara\n"
                          "100 59.00 1 0 0.25 1 2 0 03R 40.11 32.98 0 65 3 2 1 0 21L 40.14 33.00 0 60 3 2 1 0\n"
                          "110 1 0.25 0.00 Taxiway\n"
                          "111 40.12 32.99\n"
                          "17 2000 0 0 XHEL Heliport\n"
                          "102 H1 40.0 33.0 0 10 10 1 0 0 0.25 0\n"
                          "99\n");
        CHECK(tFirst.RunwayCount() == 1, "ilk dosya 1 pist");
    }
    {
        AptDatParser tSecond(tSeen, tAirports);
        FeedLines(tSecond, "1 3125 0 0 LTAC Eski Kayit\n"
                           "100 45 1 0 0.25 0 0 0 03 40.0 32.0 0 0 0 0 0 0 21 40.1 32.1 0 0 0 0 0 0\n"
                           "1 100 0 0 LTFM Istanbul\n"
                           "100 60 2 0 0.25 0 0 0 16L 41.30 28.72 0 0 0 0 0 0 34R 41.25 28.73 0 0 0 0 0 0\n"
                           "100 60 2 0 0.25 0 0 0 16R 41.30 28.74 0 0 0 0 0 0 34L 41.25 28.75 0 0 0 0 0 0");
    }
    CHECK(tAirports.size() == 2, "iki havalimani (heliport yok, eski LTAC atlandi): %d",
          static_cast<int>(tAirports.size()));
    CHECK(tAirports.size() == 2 && tAirports[0].mIcao == "LTAC" && tAirports[0].mName == "Esenboga" &&
              tAirports[0].mRunways.size() == 1 && tAirports[1].mIcao == "LTFM" &&
              tAirports[1].mRunways.size() == 2,
          "ilk gelen kazanir, son satirda 99 olmasa da havalimani kapanir");

    // scenery_packs.ini sirasi
    const std::string tIni = "I\r\n1000 Version\r\nSCENERY\r\n\r\n"
                             "SCENERY_PACK Custom Scenery/LTFM Istanbul/\r\n"
                             "SCENERY_PACK_DISABLED Custom Scenery/Kapali Paket/\r\n"
                             "SCENERY_PACK *GLOBAL_AIRPORTS*\r\n"
                             "SCENERY_PACK D:/Manzara/Ortho\r\n";
    const std::vector<std::string> tFiles = AptDatReader::AptFilesFromSceneryPacks(tIni, "C:\\X-Plane 11\\");
    CHECK(tFiles.size() == 3, "3 etkin paket: %d", static_cast<int>(tFiles.size()));
    CHECK(tFiles.size() == 3 && tFiles[0] == "C:\\X-Plane 11\\Custom Scenery/LTFM Istanbul/Earth nav data/apt.dat" &&
              tFiles[1] == "C:\\X-Plane 11\\Custom Scenery/Global Airports/Earth nav data/apt.dat" &&
              tFiles[2] == "D:/Manzara/Ortho/Earth nav data/apt.dat",
          "paket yollari (goreceli, *GLOBAL_AIRPORTS*, mutlak)");
}

// Motor baglama senaryosu: arayuzu uygulayan sahte kaynak
class FakeAirportSource : public IAirportGeometrySource
{
public:
    const char* Name() const override
    {
        return "sahte kaynak";
    }

    bool LoadAll(const std::string& pXPlaneDirectory, const std::atomic<bool>& /*pCancel*/,
                 std::vector<AirportGeometry>& pOutAirports, std::string& pOutReport) override
    {
        AirportGeometry tAirport;
        tAirport.mIcao = "LTAC";
        Runway tRunway;
        tRunway.mWidthM = 60.0;
        tRunway.mEnds[0].mPosition = {40.11, 32.98};
        tRunway.mEnds[1].mPosition = {40.14, 33.01};
        tAirport.mRunways.push_back(tRunway);
        pOutAirports.push_back(tAirport);
        tAirport.mIcao = "KSEA";
        tAirport.mRunways[0].mEnds[0].mPosition = {47.46, -122.31};
        tAirport.mRunways[0].mEnds[1].mPosition = {47.43, -122.31};
        pOutAirports.push_back(tAirport);
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
    CHECK(tStore.Find("KSEA") && tStore.Find("KSEA")->mIcao == "KSEA" && !tStore.Find("YOK1"), "ICAO ile bulma");

    GeoBounds tAnkara;
    tAnkara.mSouth = 39.5;
    tAnkara.mNorth = 40.5;
    tAnkara.mWest = 32.5;
    tAnkara.mEast = 33.5;
    std::vector<int> tResult;
    tStore.Query(tAnkara, tResult);
    CHECK(tResult.size() == 1 && tStore.At(tResult[0]).mIcao == "LTAC", "Ankara sorgusu");
}

void TestStoreCancel()
{
    // Yukleme bitmeden yok edilen depo takilmadan kapanmali (X-Plane kapanisi)
    AirportGeometryStore tStore(std::make_unique<AptDatReader>());
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
                 std::vector<AirportGeometry>& pOutAirports, std::string& /*pOutReport*/) override
    {
        AirportGeometry tAirport;
        tAirport.mIcao = "LTAC";
        Runway tRunway;
        tRunway.mWidthM = 59.0;
        tRunway.mEnds[0].mName = "03R";
        tRunway.mEnds[0].mPosition = {40.1140623, 32.9831635};
        tRunway.mEnds[1].mName = "21L";
        tRunway.mEnds[1].mPosition = {40.1413103, 33.0091255};
        tAirport.mRunways.push_back(tRunway);
        pOutAirports.push_back(tAirport);
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
    TestDrawings();
    TestMapFormat();
    TestSettings();
    TestLayerManager();
    TestNavDatabase();
    TestLabelPlacer();
    TestMapViewBounds();
    TestAptDatReader();
    TestAirportGeometryStore();
    TestStoreCancel();
    TestLocalizerMatch();
    std::printf("%d checks passed, %d failed\n", gPassCount, gFailCount);
    return gFailCount == 0 ? 0 : 1;
}
