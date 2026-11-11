// SelfTest.cpp
#include "SelfTest.h"

#include <cmath>
#include <format>

#include "XPLMUtilities.h"

#include "MapApiCalls.h"

namespace
{

using PrimeMap::MapLayer;

const float kDataTimeout = 300.0f;  // havalimani / nav verisi en cok bu kadar beklenir (s)
const float kDrawWait = 2.0f;       // sorgudan once harita bu kadar cizilsin (s)

bool Near(double pA, double pB, double pTolerance)
{
    return std::fabs(pA - pB) <= pTolerance;
}

void Write(const std::string& pLine)
{
    XPLMDebugString(("[map-api-test] " + pLine + "\n").c_str());
}

}  // namespace

SelfTest::SelfTest(MapApiCalls& pCalls)
    : mCalls(pCalls)
{
}

void SelfTest::Start(float pNow)
{
    Write("self test started");
    mPhase = 1;
    mPhaseStart = pNow;
    RunImmediate();
}

bool SelfTest::IsDone() const
{
    return mPhase == 4;
}

void SelfTest::Step(float pNow)
{
    if (mPhase == 1)
    {
        const PrimeMap::Status tStatus = mCalls.GetStatus();
        const bool tReady = tStatus.mAirportsReady && tStatus.mNavReady;
        if (tReady || pNow - mPhaseStart > kDataTimeout)
        {
            Check(tReady, "data ready (GetStatus: airports + nav)", tStatus.mText);
            RunWithData();
            mPhase = 2;
            mPhaseStart = pNow;
        }
    }
    else if (mPhase == 2 && pNow - mPhaseStart > kDrawWait)
    {
        RunAfterDraw();
        Finish();
    }
}

void SelfTest::Check(bool pCondition, const std::string& pName, const std::string& pDetail)
{
    (pCondition ? mPassed : mFailed)++;
    Write(std::format("{} {}{}", pCondition ? "PASS" : "FAIL", pName, pDetail.empty() ? "" : "  (" + pDetail + ")"));
}

void SelfTest::RunImmediate()
{
    // --- Kutuphane ve tek harita kurali
    Check(mCalls.IsInitialized(), "IsInitialized");
    Check(mCalls.VersionString() == "1.0.0", "VersionString", mCalls.VersionString());
    Check(mCalls.IsOpen(), "Map::Open (embedded)");
    Check(!mCalls.OpenSecondMap(), "Map::Open second map returns nullptr");

    // --- Pencere
    const PrimeMap::Geometry tGeometry = {150, 750, 640, 420};
    mCalls.SetGeometry(tGeometry);
    const PrimeMap::Geometry tBack = mCalls.GetGeometry();
    Check(tBack.mLeft == 150 && tBack.mTop == 750 && tBack.mWidth == 640 && tBack.mHeight == 420,
          "SetGeometry / GetGeometry",
          std::format("{} {} {} {}", tBack.mLeft, tBack.mTop, tBack.mWidth, tBack.mHeight));
    mCalls.SetVisible(false);
    Check(!mCalls.IsVisible(), "SetVisible(false) / IsVisible");
    mCalls.SetVisible(true);
    Check(mCalls.IsVisible(), "SetVisible(true) / IsVisible");
    mCalls.BringToFront();
    mCalls.SetPoppedOut(true);
    Check(!mCalls.IsPoppedOut(), "SetPoppedOut ignored for Embedded / IsPoppedOut");
    mCalls.SetUiVisible(true);
    Check(mCalls.IsUiVisible(), "SetUiVisible(true) / IsUiVisible");
    mCalls.SetUiVisible(false);
    Check(!mCalls.IsUiVisible(), "SetUiVisible(false)");
    mCalls.SetMouseInput(true);
    Check(mCalls.IsMouseInputEnabled(), "SetMouseInput(true) / IsMouseInputEnabled");
    mCalls.SetMouseInput(false);
    Check(!mCalls.IsMouseInputEnabled(), "SetMouseInput(false)");

    // --- Gorunum
    mCalls.SetCenter(47.45, -122.30);
    const PrimeMap::LatLon tCenter = mCalls.GetCenter();
    Check(Near(tCenter.mLat, 47.45, 1e-6) && Near(tCenter.mLon, -122.30, 1e-6), "SetCenter / GetCenter");
    Check(!mCalls.IsFollowingAircraft(), "SetCenter stops following");
    mCalls.SetZoom(10.0);
    Check(Near(mCalls.GetZoom(), 10.0, 1e-9), "SetZoom / GetZoom");
    mCalls.SetZoom(100.0);
    Check(Near(mCalls.GetZoom(), mCalls.MaxZoom(), 1e-9), "SetZoom clamps to MaxZoom", std::format("{}", mCalls.MaxZoom()));
    mCalls.SetZoom(-5.0);
    Check(Near(mCalls.GetZoom(), mCalls.MinZoom(), 1e-9), "SetZoom clamps to MinZoom", std::format("{}", mCalls.MinZoom()));
    mCalls.SetZoom(10.0);
    mCalls.ZoomIn();
    Check(Near(mCalls.GetZoom(), 10.5, 1e-9), "ZoomIn (default step 0.5)");
    mCalls.ZoomOut();
    Check(Near(mCalls.GetZoom(), 10.0, 1e-9), "ZoomOut");
    mCalls.SetZoomStep(1.0);
    mCalls.ZoomIn();
    Check(Near(mCalls.GetZoom(), 11.0, 1e-9), "SetZoomStep(1) + ZoomIn");
    mCalls.SetZoomStep(0.5);
    mCalls.SetZoom(10.0);

    PrimeMap::LatLon tBefore = mCalls.GetCenter();
    mCalls.PanLeft();
    Check(mCalls.GetCenter().mLon < tBefore.mLon, "PanLeft moves view west");
    tBefore = mCalls.GetCenter();
    mCalls.PanRight();
    Check(mCalls.GetCenter().mLon > tBefore.mLon, "PanRight moves view east");
    tBefore = mCalls.GetCenter();
    mCalls.PanUp();
    Check(mCalls.GetCenter().mLat > tBefore.mLat, "PanUp moves view north");
    tBefore = mCalls.GetCenter();
    mCalls.PanDown();
    Check(mCalls.GetCenter().mLat < tBefore.mLat, "PanDown moves view south");
    const PrimeMap::GeoBounds tView = mCalls.GetVisibleBounds();
    const double tSpan = tView.mEast - tView.mWest;
    mCalls.SetPanStep(0.5);
    tBefore = mCalls.GetCenter();
    mCalls.PanRight();
    Check(Near(mCalls.GetCenter().mLon - tBefore.mLon, tSpan * 0.5, tSpan * 0.02), "SetPanStep(0.5) + PanRight",
          std::format("moved {:.4f} of span {:.4f}", mCalls.GetCenter().mLon - tBefore.mLon, tSpan));
    mCalls.SetPanStep(0.25);
    tBefore = mCalls.GetCenter();
    mCalls.PanBy(100.0, 0.0);
    Check(mCalls.GetCenter().mLon < tBefore.mLon, "PanBy(+100, 0): content moves right, view west");
    Check(tView.mNorth > tView.mSouth && tView.mEast > tView.mWest, "GetVisibleBounds",
          std::format("{:.3f} {:.3f} {:.3f} {:.3f}", tView.mSouth, tView.mWest, tView.mNorth, tView.mEast));
    Check(mCalls.GetMetersPerBoxel() > 0.0, "GetMetersPerBoxel", std::format("{:.2f}", mCalls.GetMetersPerBoxel()));
    int tX = 0;
    int tY = 0;
    mCalls.SetCenter(47.45, -122.30);
    const bool tOnScreen = mCalls.GeoToScreen(47.45, -122.30, &tX, &tY);
    const PrimeMap::LatLon tGeo = mCalls.ScreenToGeo(tX, tY);
    Check(tOnScreen && Near(tGeo.mLat, 47.45, 0.01) && Near(tGeo.mLon, -122.30, 0.01),
          "GeoToScreen / ScreenToGeo round trip", std::format("screen {} {}", tX, tY));
    Check(tX == 150 + 320 && tY == 750 - 210, "GeoToScreen of the center is the window center");
    Check(!mCalls.GeoToScreen(-47.0, 60.0, &tX, &tY), "GeoToScreen outside the window returns false");

    // --- Mod
    const PrimeMap::MapMode tModes[] = {PrimeMap::MapMode::Vfr, PrimeMap::MapMode::IfrLow, PrimeMap::MapMode::IfrHigh,
                                        PrimeMap::MapMode::Dark, PrimeMap::MapMode::Scenery};
    for (PrimeMap::MapMode tMode : tModes)
    {
        mCalls.SetMode(tMode);
        Check(mCalls.GetMode() == tMode, std::format("SetMode / GetMode {}", static_cast<int>(tMode)));
    }
    mCalls.NextMode();
    Check(mCalls.GetMode() == PrimeMap::MapMode::Vfr, "NextMode Scenery -> Vfr");
    mCalls.SetMode(PrimeMap::MapMode::Scenery);

    // --- Katman yardimcilari
    Check(mCalls.TopLevelLayers().size() == 6, "TopLevelLayers (5 groups + drawings)");
    Check(mCalls.ChildrenOf(MapLayer::Runways).size() == 6, "ChildrenOf(Runways)");
    Check(mCalls.ParentOf(MapLayer::RunwaysWater) == MapLayer::Runways, "ParentOf(RunwaysWater)");
    Check(!mCalls.ParentOf(MapLayer::AirportsGroup), "ParentOf(group) = none");
    Check(mCalls.KindOf(MapLayer::NavaidsGroup) == PrimeMap::LayerKind::Group, "KindOf");
    Check(mCalls.LayerName(MapLayer::RunwaysWater) == "Water runways", "LayerName");
    Check(mCalls.LayerKey(MapLayer::RunwaysWater) == "runways.water", "LayerKey");
    Check(mCalls.LayerFromKey("airport_objects.signs") == MapLayer::ObjectsSigns, "LayerFromKey");
    Check(!mCalls.IsLayerDefaultVisible(MapLayer::TaxiRoutes, PrimeMap::MapMode::Scenery), "IsLayerDefaultVisible");

    // --- Katmanlar (panel kurallari)
    mCalls.ResetLayersToDefaults();
    Check(mCalls.IsLayerVisible(MapLayer::Runways), "ResetLayersToDefaults / IsLayerVisible");
    mCalls.SetLayerVisible(MapLayer::Runways, false);
    Check(!mCalls.IsLayerVisible(MapLayer::RunwaysPaved), "SetLayerVisible(layer, false) hides sub layers");
    mCalls.SetLayerVisible(MapLayer::RunwaysWater, true);
    Check(mCalls.GetLayerState(MapLayer::Runways) == PrimeMap::LayerState::Mixed &&
              !mCalls.IsLayerVisible(MapLayer::RunwaysPaved),
          "sub layer of a closed layer opens alone (GetLayerState Mixed)");
    mCalls.ToggleLayer(MapLayer::AirwaysGroup);
    Check(mCalls.GetLayerState(MapLayer::AirwaysGroup) == PrimeMap::LayerState::Off, "ToggleLayer(group) off");
    mCalls.ToggleLayer(MapLayer::AirwaysGroup);
    Check(mCalls.GetLayerState(MapLayer::AirwaysGroup) == PrimeMap::LayerState::On, "ToggleLayer(group) on");
    mCalls.SetLayersVisible({MapLayer::Vor, MapLayer::Ndb}, false);
    Check(!mCalls.IsLayerVisible(MapLayer::Vor) && !mCalls.IsLayerVisible(MapLayer::Ndb), "SetLayersVisible");
    mCalls.ShowOnlyLayers({MapLayer::Terrain, MapLayer::AirportSymbols});
    Check(mCalls.IsLayerVisible(MapLayer::Terrain) && !mCalls.IsLayerVisible(MapLayer::Grid), "ShowOnlyLayers");
    const uint64_t tRevision = mCalls.GetSettingsRevision();
    mCalls.ResetLayersToDefaults();
    Check(mCalls.GetSettingsRevision() > tRevision, "GetSettingsRevision increases on change");

    // --- Cizimler
    mCalls.SetDrawTool(PrimeMap::DrawTool::Pan);
    Check(!mCalls.AddDrawingPoint(47.0, -122.0), "AddDrawingPoint refused with Pan");
    const uint64_t tDrawingsRevision = mCalls.GetDrawingsRevision();
    mCalls.SetDrawTool(PrimeMap::DrawTool::Point);
    Check(mCalls.GetDrawTool() == PrimeMap::DrawTool::Point, "SetDrawTool / GetDrawTool");
    Check(mCalls.AddDrawingPoint(47.45, -122.30), "AddDrawingPoint (point)");
    mCalls.SetDrawTool(PrimeMap::DrawTool::Route);
    mCalls.AddDrawingPoint(47.40, -122.40);
    mCalls.AddDrawingPoint(47.50, -122.20);
    Check(mCalls.HasUnfinishedDrawing(), "HasUnfinishedDrawing");
    Check(mCalls.FinishDrawing(), "FinishDrawing (route)");
    Check(!mCalls.FinishDrawing(), "FinishDrawing with nothing open returns false");
    mCalls.SetDrawTool(PrimeMap::DrawTool::Region);
    mCalls.AddDrawingPoint(47.3, -122.5);
    mCalls.AddDrawingPoint(47.3, -122.1);
    mCalls.AddDrawingPoint(47.6, -122.3);
    mCalls.FinishDrawing();
    std::vector<PrimeMap::Shape> tShapes = mCalls.GetShapes();
    Check(tShapes.size() == 3 && tShapes[0].mLabel == "P1" && tShapes[1].mLabel == "R1" && tShapes[2].mLabel == "A1",
          "GetShapes (P1, R1, A1)", std::format("{} shapes", tShapes.size()));
    Check(mCalls.GetDrawingsRevision() > tDrawingsRevision, "GetDrawingsRevision increases");
    const std::string tExported = mCalls.ExportDrawings();
    Check(mCalls.RemoveShape(tShapes[0].mId) && mCalls.GetShapes().size() == 2, "RemoveShape");
    Check(mCalls.UndoDrawing() && mCalls.GetShapes().size() == 1, "UndoDrawing");
    mCalls.ClearDrawings();
    Check(mCalls.GetShapes().empty(), "ClearDrawings");
    Check(mCalls.ImportDrawings(tExported) && mCalls.GetShapes().size() == 3, "ExportDrawings / ImportDrawings");
    mCalls.SetDrawTool(PrimeMap::DrawTool::Pan);

    // --- Ayarlar
    mCalls.SetMode(PrimeMap::MapMode::IfrHigh);
    const std::string tSettings = mCalls.ExportSettings();
    Check(tSettings.find("[layers.ifr_high]") != std::string::npos && tSettings.find("mode=ifr_high") != std::string::npos,
          "ExportSettings");
    mCalls.SetMode(PrimeMap::MapMode::Scenery);
    Check(mCalls.ImportSettings(tSettings) && mCalls.GetMode() == PrimeMap::MapMode::IfrHigh, "ImportSettings");
    mCalls.SetMode(PrimeMap::MapMode::Scenery);
    PrimeMap::TerrainSettings tTerrain = mCalls.GetTerrainSettings();
    Check(tTerrain.mVfrBandsFeet.size() == 8 && !tTerrain.mClassColors.empty(), "GetTerrainSettings");
    const double tShading = tTerrain.mShadingStrength;
    tTerrain.mShadingStrength = 0.42;
    mCalls.SetTerrainSettings(tTerrain);
    Check(Near(mCalls.GetTerrainSettings().mShadingStrength, 0.42, 1e-6), "SetTerrainSettings");
    tTerrain.mShadingStrength = tShading;
    mCalls.SetTerrainSettings(tTerrain);
    const std::string tTerrainText = mCalls.ExportTerrainSettings();
    Check(tTerrainText.find("[relief]") != std::string::npos, "ExportTerrainSettings");
    Check(mCalls.ImportTerrainSettings(tTerrainText), "ImportTerrainSettings");

    // --- Teshis
    mCalls.SetDiagnostics(true);
    Check(mCalls.IsDiagnosticsOn(), "SetDiagnostics / IsDiagnosticsOn");
    mCalls.SetDiagnostics(false);
}

void SelfTest::RunWithData()
{
    // --- Arama ve havalimanina gitme
    const std::vector<PrimeMap::AirportMatch> tMatches = mCalls.FindAirports("KSEA", 5);
    Check(!tMatches.empty() && tMatches[0].mIdent == "KSEA", "FindAirports(KSEA)",
          tMatches.empty() ? "none" : tMatches[0].mName);
    Check(mCalls.GoToAirport("KSEA"), "GoToAirport(KSEA)");
    Check(Near(mCalls.GetCenter().mLat, 47.45, 0.05) && Near(mCalls.GetCenter().mLon, -122.31, 0.05),
          "GoToAirport moves the map", std::format("{:.4f} {:.4f} zoom {:.2f}", mCalls.GetCenter().mLat,
                                                   mCalls.GetCenter().mLon, mCalls.GetZoom()));
    Check(!mCalls.GoToAirport("QQQQ9"), "GoToAirport unknown ident returns false");

    // --- FitBounds
    PrimeMap::GeoBounds tArea;
    tArea.mSouth = 47.0;
    tArea.mWest = -123.0;
    tArea.mNorth = 48.0;
    tArea.mEast = -122.0;
    mCalls.FitBounds(tArea);
    const PrimeMap::GeoBounds tView = mCalls.GetVisibleBounds();
    Check(tView.mSouth <= 47.0 && tView.mNorth >= 48.0 && tView.mWest <= -123.0 && tView.mEast >= -122.0,
          "FitBounds: area fully visible",
          std::format("{:.3f} {:.3f} {:.3f} {:.3f}", tView.mSouth, tView.mWest, tView.mNorth, tView.mEast));
    Check(tView.mNorth - tView.mSouth < 2.5, "FitBounds: not zoomed out too far");

    // --- Ucak (host kaynakli)
    mCalls.SetAircraftSource(PrimeMap::DataSource::Host);
    Check(mCalls.GetAircraftSource() == PrimeMap::DataSource::Host, "SetAircraftSource / GetAircraftSource");
    PrimeMap::AircraftState tAircraft;
    tAircraft.mValid = true;
    tAircraft.mPosition = {47.60, -122.33};
    tAircraft.mHeadingDeg = 90.0;
    tAircraft.mGroundSpeedKt = 120.0;
    tAircraft.mAltitudeFt = 3000.0;
    mCalls.SetAircraft(tAircraft);
    Check(Near(mCalls.GetAircraft().mPosition.mLat, 47.60, 1e-9) && mCalls.GetAircraft().mValid,
          "SetAircraft / GetAircraft");
    mCalls.CenterOnAircraft();
    Check(Near(mCalls.GetCenter().mLat, 47.60, 1e-6), "CenterOnAircraft");
    mCalls.SetFollowAircraft(true);
    Check(mCalls.IsFollowingAircraft(), "SetFollowAircraft / IsFollowingAircraft");
    mCalls.PanBy(50.0, 0.0);
    Check(!mCalls.IsFollowingAircraft(), "PanBy stops following");
    mCalls.ClearTrack();
    Check(true, "ClearTrack (called)");

    // --- Rota (host kaynakli)
    mCalls.SetRouteSource(PrimeMap::DataSource::Host);
    Check(mCalls.GetRouteSource() == PrimeMap::DataSource::Host, "SetRouteSource / GetRouteSource");
    mCalls.SetRoute({{"KSEA", {47.449, -122.309}, 0}, {"SEA", {47.435, -122.310}, 3000}, {"KPAE", {47.906, -122.282}, 0}},
                    1);
    Check(true, "SetRoute (3 waypoints; visible on the map)");

    // Sorgu icin: KSEA'ya don, katmanlar varsayilan, harita cizilsin
    mCalls.ResetLayersToDefaults();
    mCalls.GoToAirport("KSEA");
}

void SelfTest::RunAfterDraw()
{
    // --- Sorgu (son cizilen kareye gore)
    int tX = 0;
    int tY = 0;
    mCalls.GeoToScreen(47.4490, -122.3093, &tX, &tY);
    std::string tFound;
    for (int tDy = -40; tDy <= 40 && tFound.empty(); tDy += 4)
    {
        for (int tDx = -40; tDx <= 40 && tFound.empty(); tDx += 4)
        {
            const std::vector<PrimeMap::MapObject> tObjects = mCalls.QueryAtScreen(tX + tDx, tY + tDy);
            if (!tObjects.empty())
            {
                tFound = std::format("{}: {}", mCalls.LayerKey(tObjects[0].mLayer), tObjects[0].mText);
            }
        }
    }
    Check(!tFound.empty(), "QueryAtScreen near KSEA", tFound);
    const std::vector<PrimeMap::MapObject> tAtGeo = mCalls.QueryAt(47.4490, -122.3093);
    Check(true, "QueryAt (called)", std::format("{} objects", tAtGeo.size()));

    // --- On onbellek
    PrimeMap::GeoBounds tOcean;
    tOcean.mSouth = -40.0;
    tOcean.mWest = -140.0;
    tOcean.mNorth = -39.5;
    tOcean.mEast = -139.5;
    Check(!mCalls.PrecacheArea(tOcean), "PrecacheArea without scenery returns false");
    const bool tStarted = mCalls.PrecacheVisibleArea();
    Check(true, "PrecacheVisibleArea (called)", tStarted ? "started" : "not started: " + mCalls.GetStatus().mText);
    mCalls.CancelPrecache();

    // --- UI ve fare acik birak (elle denemek icin), Status
    const PrimeMap::Status tStatus = mCalls.GetStatus();
    Check(tStatus.mNavReady && tStatus.mAirportsReady, "GetStatus after load",
          std::format("airways {} terrain index {}", tStatus.mAirwaysReady, tStatus.mTerrainIndexReady));

    // --- Kapat / yeniden ac (tek harita kurali serbest kalir)
    mCalls.CloseMap();
    Check(!mCalls.IsOpen(), "CloseMap");
    const bool tReopened = mCalls.OpenMap({150, 750, 640, 420}, PrimeMap::WindowStyle::Embedded, true, true);
    Check(tReopened, "Map::Open again after close (UI and mouse on)");
    mCalls.GoToAirport("KSEA");
}

void SelfTest::Finish()
{
    Write(std::format("self test finished: {} passed, {} failed", mPassed, mFailed));
    mPhase = 4;
}
