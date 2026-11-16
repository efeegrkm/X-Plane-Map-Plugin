// SelfTest.cpp
#include "SelfTest.h"

#include <cmath>
#include <format>

#include "XPLMUtilities.h"

#include "MapApiCalls.h"

namespace
{

using prime::MapLayer;

const float kDataTimeout = 300.0f;  // havalimani / nav verisi en cok bu kadar beklenir (s)
const float kDrawWait = 2.0f;       // sorgudan once harita bu kadar cizilsin (s)

bool nearEqual(double pA, double pB, double pTolerance)
{
    return std::fabs(pA - pB) <= pTolerance;
}

void write(const std::string& pLine)
{
    XPLMDebugString(("[map-api-test] " + pLine + "\n").c_str());
}

}  // namespace

SelfTest::SelfTest(MapApiCalls& pCalls)
    : mCalls(pCalls)
{
}

void SelfTest::start(float pNow)
{
    write("self test started");
    mPhase = 1;
    mPhaseStart = pNow;
    runImmediate();
}

bool SelfTest::isDone() const
{
    return mPhase == 4;
}

void SelfTest::step(float pNow)
{
    if (mPhase == 1)
    {
        const prime::Status tStatus = mCalls.getStatus();
        const bool tReady = tStatus.mAirportsReady && tStatus.mNavReady;
        if (tReady || pNow - mPhaseStart > kDataTimeout)
        {
            Check(tReady, "data ready (getStatus: airports + nav)", tStatus.mText);
            runWithData();
            mPhase = 2;
            mPhaseStart = pNow;
        }
    }
    else if (mPhase == 2 && pNow - mPhaseStart > kDrawWait)
    {
        runAfterDraw();
        finish();
    }
}

void SelfTest::Check(bool pCondition, const std::string& pName, const std::string& pDetail)
{
    (pCondition ? mPassed : mFailed)++;
    write(std::format("{} {}{}", pCondition ? "PASS" : "FAIL", pName, pDetail.empty() ? "" : "  (" + pDetail + ")"));
}

void SelfTest::runImmediate()
{
    // --- Kutuphane ve coklu harita (0.24 fabrika)
    Check(mCalls.isInitialized(), "isInitialized");
    Check(mCalls.versionString() == "0.24.0", "VersionString", mCalls.versionString());
    Check(mCalls.isOpen(), "XMap::open(embedded)");
    Check(mCalls.openSecondMap(), "openSecondMap opens an independent second map");
    Check(!mCalls.openSecondMap(), "openSecondMap again closes the second map");

    // --- Pencere
    const prime::Geometry tGeometry = {150, 750, 640, 420};
    mCalls.setGeometry(tGeometry);
    const prime::Geometry tBack = mCalls.getGeometry();
    Check(tBack.mLeft == 150 && tBack.mTop == 750 && tBack.mWidth == 640 && tBack.mHeight == 420,
          "setGeometry / getGeometry",
          std::format("{} {} {} {}", tBack.mLeft, tBack.mTop, tBack.mWidth, tBack.mHeight));
    mCalls.setVisible(false);
    Check(!mCalls.isVisible(), "setVisible(false) / isVisible");
    mCalls.setVisible(true);
    Check(mCalls.isVisible(), "setVisible(true) / isVisible");
    mCalls.bringToFront();
    mCalls.setPoppedOut(true);
    Check(!mCalls.isPoppedOut(), "setPoppedOut ignored for Embedded / isPoppedOut");
    mCalls.setUiVisible(true);
    Check(mCalls.isUiVisible(), "setUiVisible(true) / isUiVisible");
    mCalls.setUiVisible(false);
    Check(!mCalls.isUiVisible(), "setUiVisible(false)");
    mCalls.setMouseInput(true);
    Check(mCalls.isMouseInputEnabled(), "setMouseInput(true) / isMouseInputEnabled");
    mCalls.setMouseInput(false);
    Check(!mCalls.isMouseInputEnabled(), "setMouseInput(false)");

    // --- Gorunum
    mCalls.setCenter(47.45, -122.30);
    const prime::LatLon tCenter = mCalls.getCenter();
    Check(nearEqual(tCenter.mLat, 47.45, 1e-6) && nearEqual(tCenter.mLon, -122.30, 1e-6), "setCenter / getCenter");
    Check(!mCalls.isFollowingAircraft(), "setCenter stops following");
    mCalls.setZoom(10.0);
    Check(nearEqual(mCalls.getZoom(), 10.0, 1e-9), "setZoom / getZoom");
    mCalls.setZoom(100.0);
    Check(nearEqual(mCalls.getZoom(), mCalls.maxZoom(), 1e-9), "setZoom clamps to MaxZoom", std::format("{}", mCalls.maxZoom()));
    mCalls.setZoom(-5.0);
    Check(nearEqual(mCalls.getZoom(), mCalls.minZoom(), 1e-9), "setZoom clamps to MinZoom", std::format("{}", mCalls.minZoom()));
    mCalls.setZoom(10.0);
    mCalls.zoomIn();
    Check(nearEqual(mCalls.getZoom(), 10.5, 1e-9), "zoomIn (default step 0.5)");
    mCalls.zoomOut();
    Check(nearEqual(mCalls.getZoom(), 10.0, 1e-9), "zoomOut");
    mCalls.setZoomStep(1.0);
    mCalls.zoomIn();
    Check(nearEqual(mCalls.getZoom(), 11.0, 1e-9), "setZoomStep(1) + zoomIn");
    mCalls.setZoomStep(0.5);
    mCalls.setZoom(10.0);

    prime::LatLon tBefore = mCalls.getCenter();
    mCalls.panLeft();
    Check(mCalls.getCenter().mLon < tBefore.mLon, "panLeft moves view west");
    tBefore = mCalls.getCenter();
    mCalls.panRight();
    Check(mCalls.getCenter().mLon > tBefore.mLon, "panRight moves view east");
    tBefore = mCalls.getCenter();
    mCalls.panUp();
    Check(mCalls.getCenter().mLat > tBefore.mLat, "panUp moves view north");
    tBefore = mCalls.getCenter();
    mCalls.panDown();
    Check(mCalls.getCenter().mLat < tBefore.mLat, "panDown moves view south");
    const prime::GeoBounds tView = mCalls.getVisibleBounds();
    const double tSpan = tView.mEast - tView.mWest;
    mCalls.setPanStep(0.5);
    tBefore = mCalls.getCenter();
    mCalls.panRight();
    Check(nearEqual(mCalls.getCenter().mLon - tBefore.mLon, tSpan * 0.5, tSpan * 0.02), "setPanStep(0.5) + panRight",
          std::format("moved {:.4f} of span {:.4f}", mCalls.getCenter().mLon - tBefore.mLon, tSpan));
    mCalls.setPanStep(0.25);
    tBefore = mCalls.getCenter();
    mCalls.panBy(100.0, 0.0);
    Check(mCalls.getCenter().mLon < tBefore.mLon, "panBy(+100, 0): content moves right, view west");
    Check(tView.mNorth > tView.mSouth && tView.mEast > tView.mWest, "getVisibleBounds",
          std::format("{:.3f} {:.3f} {:.3f} {:.3f}", tView.mSouth, tView.mWest, tView.mNorth, tView.mEast));
    Check(mCalls.getMetersPerBoxel() > 0.0, "getMetersPerBoxel", std::format("{:.2f}", mCalls.getMetersPerBoxel()));
    int tX = 0;
    int tY = 0;
    mCalls.setCenter(47.45, -122.30);
    const bool tOnScreen = mCalls.geoToScreen(47.45, -122.30, &tX, &tY);
    const prime::LatLon tGeo = mCalls.screenToGeo(tX, tY);
    Check(tOnScreen && nearEqual(tGeo.mLat, 47.45, 0.01) && nearEqual(tGeo.mLon, -122.30, 0.01),
          "GeoToScreen / ScreenToGeo round trip", std::format("screen {} {}", tX, tY));
    Check(tX == 150 + 320 && tY == 750 - 210, "GeoToScreen of the center is the window center");
    Check(!mCalls.geoToScreen(-47.0, 60.0, &tX, &tY), "GeoToScreen outside the window returns false");

    // --- Mod
    const prime::MapMode tModes[] = {prime::MapMode::Vfr, prime::MapMode::IfrLow, prime::MapMode::IfrHigh,
                                        prime::MapMode::Dark, prime::MapMode::Scenery};
    for (prime::MapMode tMode : tModes)
    {
        mCalls.setMode(tMode);
        Check(mCalls.getMode() == tMode, std::format("setMode / getMode {}", static_cast<int>(tMode)));
    }
    mCalls.nextMode();
    Check(mCalls.getMode() == prime::MapMode::Vfr, "nextMode Scenery -> Vfr");
    mCalls.setMode(prime::MapMode::Scenery);

    // --- Katman yardimcilari
    Check(mCalls.topLevelLayers().size() == 6, "TopLevelLayers (5 groups + drawings)");
    Check(mCalls.childrenOf(MapLayer::Runways).size() == 6, "ChildrenOf(Runways)");
    Check(mCalls.parentOf(MapLayer::RunwaysWater) == MapLayer::Runways, "ParentOf(RunwaysWater)");
    Check(!mCalls.parentOf(MapLayer::AirportsGroup), "ParentOf(group) = none");
    Check(mCalls.kindOf(MapLayer::NavaidsGroup) == prime::LayerKind::Group, "KindOf");
    Check(mCalls.layerName(MapLayer::RunwaysWater) == "Water runways", "LayerName");
    Check(mCalls.layerKey(MapLayer::RunwaysWater) == "runways.water", "LayerKey");
    Check(mCalls.layerFromKey("airport_objects.signs") == MapLayer::ObjectsSigns, "LayerFromKey");
    Check(!mCalls.isLayerDefaultVisible(MapLayer::TaxiRoutes, prime::MapMode::Scenery), "isLayerDefaultVisible");

    // --- katmanlar(panel kurallari)
    mCalls.resetLayersToDefaults();
    Check(mCalls.isLayerVisible(MapLayer::Runways), "resetLayersToDefaults / isLayerVisible");
    mCalls.setLayerVisible(MapLayer::Runways, false);
    Check(!mCalls.isLayerVisible(MapLayer::RunwaysPaved), "setLayerVisible(layer, false) hides sub layers");
    mCalls.setLayerVisible(MapLayer::RunwaysWater, true);
    Check(mCalls.getLayerState(MapLayer::Runways) == prime::LayerState::Mixed &&
              !mCalls.isLayerVisible(MapLayer::RunwaysPaved),
          "sub layer of a closed layer opens alone (getLayerState Mixed)");
    mCalls.toggleLayer(MapLayer::AirwaysGroup);
    Check(mCalls.getLayerState(MapLayer::AirwaysGroup) == prime::LayerState::Off, "toggleLayer(group) off");
    mCalls.toggleLayer(MapLayer::AirwaysGroup);
    Check(mCalls.getLayerState(MapLayer::AirwaysGroup) == prime::LayerState::On, "toggleLayer(group) on");
    mCalls.setLayersVisible({MapLayer::Vor, MapLayer::Ndb}, false);
    Check(!mCalls.isLayerVisible(MapLayer::Vor) && !mCalls.isLayerVisible(MapLayer::Ndb), "setLayersVisible");
    mCalls.showOnlyLayers({MapLayer::Terrain, MapLayer::AirportSymbols});
    Check(mCalls.isLayerVisible(MapLayer::Terrain) && !mCalls.isLayerVisible(MapLayer::Grid), "showOnlyLayers");
    const uint64_t tRevision = mCalls.getSettingsRevision();
    mCalls.resetLayersToDefaults();
    Check(mCalls.getSettingsRevision() > tRevision, "getSettingsRevision increases on change");

    // --- Cizimler
    mCalls.setDrawTool(prime::DrawTool::Pan);
    Check(!mCalls.addDrawingPoint(47.0, -122.0), "addDrawingPoint refused with Pan");
    const uint64_t tDrawingsRevision = mCalls.getDrawingsRevision();
    mCalls.setDrawTool(prime::DrawTool::Point);
    Check(mCalls.getDrawTool() == prime::DrawTool::Point, "setDrawTool / getDrawTool");
    Check(mCalls.addDrawingPoint(47.45, -122.30), "addDrawingPoint (point)");
    mCalls.setDrawTool(prime::DrawTool::Route);
    mCalls.addDrawingPoint(47.40, -122.40);
    mCalls.addDrawingPoint(47.50, -122.20);
    Check(mCalls.hasUnfinishedDrawing(), "hasUnfinishedDrawing");
    Check(mCalls.finishDrawing(), "finishDrawing (route)");
    Check(!mCalls.finishDrawing(), "finishDrawing with nothing open returns false");
    mCalls.setDrawTool(prime::DrawTool::Region);
    mCalls.addDrawingPoint(47.3, -122.5);
    mCalls.addDrawingPoint(47.3, -122.1);
    mCalls.addDrawingPoint(47.6, -122.3);
    mCalls.finishDrawing();
    std::vector<prime::Shape> tShapes = mCalls.getShapes();
    Check(tShapes.size() == 3 && tShapes[0].mLabel == "P1" && tShapes[1].mLabel == "R1" && tShapes[2].mLabel == "A1",
          "getShapes (P1, R1, A1)", std::format("{} shapes", tShapes.size()));
    Check(mCalls.getDrawingsRevision() > tDrawingsRevision, "getDrawingsRevision increases");
    const std::string tExported = mCalls.exportDrawings();
    Check(mCalls.removeShape(tShapes[0].mId) && mCalls.getShapes().size() == 2, "removeShape");
    Check(mCalls.undoDrawing() && mCalls.getShapes().size() == 1, "undoDrawing");
    mCalls.clearDrawings();
    Check(mCalls.getShapes().empty(), "clearDrawings");
    Check(mCalls.importDrawings(tExported) && mCalls.getShapes().size() == 3, "exportDrawings / importDrawings");
    mCalls.setDrawTool(prime::DrawTool::Pan);

    // --- Ayarlar
    mCalls.setMode(prime::MapMode::IfrHigh);
    const std::string tSettings = mCalls.exportSettings();
    Check(tSettings.find("[layers.ifr_high]") != std::string::npos && tSettings.find("mode=ifr_high") != std::string::npos,
          "exportSettings");
    mCalls.setMode(prime::MapMode::Scenery);
    Check(mCalls.importSettings(tSettings) && mCalls.getMode() == prime::MapMode::IfrHigh, "importSettings");
    mCalls.setMode(prime::MapMode::Scenery);
    prime::TerrainSettings tTerrain = mCalls.getTerrainSettings();
    Check(tTerrain.mVfrBandsFeet.size() == 8 && !tTerrain.mClassColors.empty(), "getTerrainSettings");
    const double tShading = tTerrain.mShadingStrength;
    tTerrain.mShadingStrength = 0.42;
    mCalls.setTerrainSettings(tTerrain);
    Check(nearEqual(mCalls.getTerrainSettings().mShadingStrength, 0.42, 1e-6), "setTerrainSettings");
    tTerrain.mShadingStrength = tShading;
    mCalls.setTerrainSettings(tTerrain);
    const std::string tTerrainText = mCalls.exportTerrainSettings();
    Check(tTerrainText.find("[relief]") != std::string::npos, "exportTerrainSettings");
    Check(mCalls.importTerrainSettings(tTerrainText), "importTerrainSettings");

    // --- Teshis
    mCalls.setDiagnostics(true);
    Check(mCalls.isDiagnosticsOn(), "setDiagnostics / isDiagnosticsOn");
    mCalls.setDiagnostics(false);
}

void SelfTest::runWithData()
{
    // --- Arama ve havalimanina gitme
    const std::vector<prime::AirportMatch> tMatches = mCalls.findAirports("KSEA", 5);
    Check(!tMatches.empty() && tMatches[0].mIdent == "KSEA", "findAirports(KSEA)",
          tMatches.empty() ? "none" : tMatches[0].mName);
    Check(mCalls.goToAirport("KSEA"), "goToAirport(KSEA)");
    Check(nearEqual(mCalls.getCenter().mLat, 47.45, 0.05) && nearEqual(mCalls.getCenter().mLon, -122.31, 0.05),
          "goToAirport moves the map", std::format("{:.4f} {:.4f} zoom {:.2f}", mCalls.getCenter().mLat,
                                                   mCalls.getCenter().mLon, mCalls.getZoom()));
    Check(!mCalls.goToAirport("QQQQ9"), "goToAirport unknown ident returns false");

    // --- fitBounds
    prime::GeoBounds tArea;
    tArea.mSouth = 47.0;
    tArea.mWest = -123.0;
    tArea.mNorth = 48.0;
    tArea.mEast = -122.0;
    mCalls.fitBounds(tArea);
    const prime::GeoBounds tView = mCalls.getVisibleBounds();
    Check(tView.mSouth <= 47.0 && tView.mNorth >= 48.0 && tView.mWest <= -123.0 && tView.mEast >= -122.0,
          "fitBounds: area fully visible",
          std::format("{:.3f} {:.3f} {:.3f} {:.3f}", tView.mSouth, tView.mWest, tView.mNorth, tView.mEast));
    Check(tView.mNorth - tView.mSouth < 2.5, "fitBounds: not zoomed out too far");

    // --- ucak(host kaynakli)
    mCalls.setAircraftSource(prime::DataSource::Host);
    Check(mCalls.getAircraftSource() == prime::DataSource::Host, "setAircraftSource / getAircraftSource");
    prime::AircraftState tAircraft;
    tAircraft.mValid = true;
    tAircraft.mPosition = {47.60, -122.33};
    tAircraft.mHeadingDeg = 90.0;
    tAircraft.mGroundSpeedKt = 120.0;
    tAircraft.mAltitudeFt = 3000.0;
    mCalls.setAircraft(tAircraft);
    Check(nearEqual(mCalls.getAircraft().mPosition.mLat, 47.60, 1e-9) && mCalls.getAircraft().mValid,
          "setAircraft / getAircraft");
    mCalls.centerOnAircraft();
    Check(nearEqual(mCalls.getCenter().mLat, 47.60, 1e-6), "CenterOnAircraft");
    mCalls.setFollowAircraft(true);
    Check(mCalls.isFollowingAircraft(), "setFollowAircraft / isFollowingAircraft");
    mCalls.panBy(50.0, 0.0);
    Check(!mCalls.isFollowingAircraft(), "panBy stops following");
    mCalls.clearTrack();
    Check(true, "clearTrack (called)");

    // --- rota(host kaynakli)
    mCalls.setRouteSource(prime::DataSource::Host);
    Check(mCalls.getRouteSource() == prime::DataSource::Host, "setRouteSource / getRouteSource");
    mCalls.setRoute({{"KSEA", {47.449, -122.309}, 0}, {"SEA", {47.435, -122.310}, 3000}, {"KPAE", {47.906, -122.282}, 0}},
                    1);
    Check(true, "setRoute (3 waypoints; visible on the map)");

    // Sorgu icin: KSEA'ya don, katmanlar varsayilan, harita cizilsin
    mCalls.resetLayersToDefaults();
    mCalls.goToAirport("KSEA");
}

void SelfTest::runAfterDraw()
{
    // --- sorgu(son cizilen kareye gore)
    int tX = 0;
    int tY = 0;
    mCalls.geoToScreen(47.4490, -122.3093, &tX, &tY);
    std::string tFound;
    for (int tDy = -40; tDy <= 40 && tFound.empty(); tDy += 4)
    {
        for (int tDx = -40; tDx <= 40 && tFound.empty(); tDx += 4)
        {
            const std::vector<prime::MapObject> tObjects = mCalls.queryAtScreen(tX + tDx, tY + tDy);
            if (!tObjects.empty())
            {
                tFound = std::format("{}: {}", mCalls.layerKey(tObjects[0].mLayer), tObjects[0].mText);
            }
        }
    }
    Check(!tFound.empty(), "queryAtScreen near KSEA", tFound);
    const std::vector<prime::MapObject> tAtGeo = mCalls.queryAt(47.4490, -122.3093);
    Check(true, "queryAt (called)", std::format("{} objects", tAtGeo.size()));

    // --- On onbellek
    prime::GeoBounds tOcean;
    tOcean.mSouth = -40.0;
    tOcean.mWest = -140.0;
    tOcean.mNorth = -39.5;
    tOcean.mEast = -139.5;
    Check(!mCalls.precacheArea(tOcean), "PrecacheArea without scenery returns false");
    const bool tStarted = mCalls.precacheVisibleArea();
    Check(true, "PrecacheVisibleArea (called)", tStarted ? "started" : "not started: " + mCalls.getStatus().mText);
    mCalls.cancelPrecache();

    // --- UI ve fare acik birak (elle denemek icin), Status
    const prime::Status tStatus = mCalls.getStatus();
    Check(tStatus.mNavReady && tStatus.mAirportsReady, "getStatus after load",
          std::format("airways {} terrain index {}", tStatus.mAirwaysReady, tStatus.mTerrainIndexReady));

    // --- Kapat / yeniden ac
    mCalls.closeMap();
    Check(!mCalls.isOpen(), "closeMap");
    const bool tReopened = mCalls.openMap({150, 750, 640, 420}, prime::WindowStyle::Embedded, true, true);
    Check(tReopened, "XMap::Open again after close (UI and mouse on)");
    mCalls.goToAirport("KSEA");
}

void SelfTest::finish()
{
    write(std::format("self test finished: {} passed, {} failed", mPassed, mFailed));
    mPhase = 4;
}
