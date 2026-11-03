// tests.cpp
// X-Plane OLMADAN calisan testler: projeksiyon, koordinat matematigi,
// cografi hesaplar, cizim modeli, ayar dosyasi, katman yoneticisi.
//
// Derleme: cmake -DHARITA_TESTS=ON ... ; calistirma: harita_tests(.exe)
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
#include "data/NavDatabase.h"

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
            std::printf("HATA %s:%d: ", __FILE__, __LINE__);    \
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
    const char* tPath = "harita_test_ayarlar.ini";
    {
        FILE* tFile = std::fopen(tPath, "w");
        CHECK(tFile != nullptr, "test dosyasi acilmali");
        if (tFile)
        {
            std::fputs("# yorum\n[pencere]\n sol = -1200 \nust=900\n\n[harita]\nzoom=7.25\ntakip=0\n"
                       "bozuk satir\n; diger yorum\n",
                       tFile);
            std::fclose(tFile);
        }
    }

    Settings tSettings;
    CHECK(tSettings.Load(tPath), "dosya okunmali");
    CHECK(tSettings.GetInt("pencere", "sol", 0) == -1200, "bosluklu deger");
    CHECK(tSettings.GetInt("pencere", "ust", 0) == 900, "tamsayi");
    CHECK(Near(tSettings.GetDouble("harita", "zoom", 0.0), 7.25, 1e-12), "ondalik nokta ile");
    CHECK(!tSettings.GetBool("harita", "takip", true), "bool 0");
    CHECK(tSettings.GetInt("pencere", "yok", 42) == 42, "olmayan anahtar varsayilan");
    CHECK(Near(tSettings.GetDouble("yok", "yok", 1.5), 1.5, 1e-12), "olmayan bolum varsayilan");

    tSettings.SetDouble("harita", "merkez_enlem", 41.123456789);
    tSettings.SetBool("katmanlar", "izgara", true);
    CHECK(tSettings.Save(), "kaydedilmeli");

    Settings tReloaded;
    CHECK(tReloaded.Load(tPath), "yeniden okunmali");
    CHECK(Near(tReloaded.GetDouble("harita", "merkez_enlem", 0.0), 41.123456789, 1e-8), "ondalik geri okunur");
    CHECK(tReloaded.GetBool("katmanlar", "izgara", false), "bool geri okunur");
    CHECK(tReloaded.GetInt("pencere", "sol", 0) == -1200, "eski deger korunur");
    std::remove(tPath);

    Settings tMissing;
    CHECK(!tMissing.Load("boyle_bir_dosya_yok.ini"), "olmayan dosya false");
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
    CHECK(!tSettings.GetBool("katmanlar", "a", true) && !tSettings.GetBool("katmanlar", "b", true) &&
              tSettings.GetBool("katmanlar", "c", false),
          "gorunurluk kaydi");

    tSettings.SetBool("katmanlar", "b", true);
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
    std::printf("%d test gecti, %d hata\n", gPassCount, gFailCount);
    return gFailCount == 0 ? 0 : 1;
}
