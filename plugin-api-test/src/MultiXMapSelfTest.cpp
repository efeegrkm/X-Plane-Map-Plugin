// MultiXMapSelfTest.cpp
#include "MultiXMapSelfTest.h"

#include <cmath>
#include <filesystem>
#include <format>
#include <system_error>

#include "XPLMProcessing.h"
#include "XPLMUtilities.h"

namespace
{

using prime::XMapLayer;

const char* kFlagFile = "selftest.flag";
const float kDrawWait = 3.0f;     // 2. asamadan once haritalar bu kadar cizilsin (s)
const float kCleanupWait = 2.0f;  // 3. asamadan once (s)

bool nearEqual(double pA, double pB, double pTolerance)
{
    return std::fabs(pA - pB) <= pTolerance;
}

bool fileExists(const std::string& pPath)
{
    std::error_code tError;
    return std::filesystem::exists(pPath, tError);
}

void write(const std::string& pLine)
{
    XPLMDebugString(("[plugin-api-test] SELFTEST " + pLine + "\n").c_str());
}

// taban x (1 + artis% x (n - 1) / 100): kutuphanenin sinir formulu (PrimeMap.h XMapManagerSettings)
int grownLimit(int pBase, int pGrowthPercent, int pOpenMaps)
{
    return static_cast<int>(std::lround(pBase * (1.0 + pGrowthPercent / 100.0 * (pOpenMaps - 1))));
}

}  // namespace

MultiXMapSelfTest::~MultiXMapSelfTest()
{
    if (mRegistered)
    {
        XPLMUnregisterFlightLoopCallback(flightLoopCallback, this);
    }
}

bool MultiXMapSelfTest::startIfRequested(const std::string& pPluginDirectory, prime::XMapManager* pManager)
{
    mManager = pManager;
    if (!mManager || !fileExists(pPluginDirectory + kFlagFile))
    {
        return false;
    }
    write("started (flag file found: " + pPluginDirectory + kFlagFile + ")");
    mPhase = 1;
    mPhaseStart = XPLMGetElapsedTime();
    runImmediate();
    XPLMRegisterFlightLoopCallback(flightLoopCallback, -1.0f, this);
    mRegistered = true;
    return true;
}

float MultiXMapSelfTest::flightLoopCallback(float /*pSinceLastCall*/, float /*pSinceLastLoop*/, int /*pCounter*/,
                                           void* pRefcon)
{
    return static_cast<MultiXMapSelfTest*>(pRefcon)->step();
}

float MultiXMapSelfTest::step()
{
    const float tNow = XPLMGetElapsedTime();
    if (mPhase == 1 && tNow - mPhaseStart > kDrawWait)
    {
        runAfterDraw();
        mPhase = 2;
        mPhaseStart = tNow;
    }
    else if (mPhase == 2 && tNow - mPhaseStart > kCleanupWait)
    {
        runCleanup();
        mPhase = 3;
        return 0.0f;  // bitti: bir daha cagirma
    }
    return -1.0f;
}

void MultiXMapSelfTest::Check(bool pCondition, const std::string& pName, const std::string& pDetail)
{
    (pCondition ? mPassed : mFailed)++;
    write(std::format("{} {}{}", pCondition ? "PASS" : "FAIL", pName, pDetail.empty() ? "" : "  (" + pDetail + ")"));
}

void MultiXMapSelfTest::runImmediate()
{
    prime::XMapManager& tLibrary = *mManager;

    // --- Kutuphane: surum, klasorler, merkezi ayarlar
    Check(prime::versionString() == "0.26.0", "VersionString", prime::versionString());
    Check(!tLibrary.getDataDirectory().empty() && !tLibrary.getCacheDirectory().empty() &&
              !tLibrary.getXMapsDirectory().empty(),
          "data / cache / maps directories", tLibrary.getDataDirectory());
    Check(fileExists(tLibrary.getSettingsFilePath()), "prime_map.json exists", tLibrary.getSettingsFilePath());
    mOriginalSettings = tLibrary.getSettings();
    prime::XMapManagerSettings tSettings = mOriginalSettings;
    tSettings.mTerrainCacheMb = 1000;
    tSettings.mCacheGrowthPercentPerXMap = 50;
    tSettings.mTerrainCacheMaxMb = 0;
    tSettings.mTextureMemoryMb = 400;
    tSettings.mMemoryGrowthPercentPerXMap = 50;
    tSettings.mTextureMemoryMaxMb = 0;
    Check(tLibrary.setSettings(tSettings), "XMapManager::setSettings");
    Check(tLibrary.getSettings().mCacheGrowthPercentPerXMap == 50 && tLibrary.getSettings().mTerrainCacheMb == 1000,
          "getSettings returns the new values");
    mMapsBefore = tLibrary.getOpenXMapCount();

    // --- Fabrika: nesne pencere acilmadan olusur, butun ayarlar onceden verilir
    mFirst = tLibrary.addXMap();
    Check(mFirst != nullptr, "addXMap() returns a map object");
    if (!mFirst)
    {
        return;
    }
    Check(!mFirst->isInitialized() && !mFirst->isVisible(), "new map is not open");
    Check(mFirst->getId() > 0, "getId", std::format("{}", mFirst->getId()));
    Check(tLibrary.getOpenXMapCount() == mMapsBefore, "unopened map is not counted as open");
    Check(mFirst->setName("selftest_a") && mFirst->getName() == "selftest_a", "setName / getName");
    Check(mFirst->getSettingsDirectory() == tLibrary.getXMapsDirectory() + "selftest_a/", "getSettingsDirectory",
          mFirst->getSettingsDirectory());
    mFirst->setPersistence(prime::Persistence::SaveOnly);  // kod her acilista kazansin, dosya yine yazilsin
    Check(mFirst->getPersistence() == prime::Persistence::SaveOnly, "setPersistence / getPersistence");
    mFirst->setStyle(prime::WindowStyle::Embedded);
    mFirst->setGeometry({900, 720, 420, 300});
    mFirst->setMode(prime::XMapMode::Vfr);
    mFirst->setCenter(47.45, -122.30);
    mFirst->setZoomLimits(5.0, 14.0);
    mFirst->setZoom(10.0);
    mFirst->setIsLayerVisible(XMapLayer::Grid, false);
    mFirst->setLayerZoomRange(XMapLayer::AirwaysGroup, 8.0, 99.0);
    mFirst->setZoomStep(1.0);
    mFirst->setMaxRenderFps(10);
    mFirst->setVisible(true);
    const prime::Geometry tBefore = mFirst->getGeometry();
    Check(tBefore.mLeft == 900 && tBefore.mTop == 720 && tBefore.mWidth == 420 && tBefore.mHeight == 300,
          "geometry set before Open");
    Check(mFirst->getMode() == prime::XMapMode::Vfr, "mode set before Open");
    Check(nearEqual(mFirst->getZoom(), 10.0, 1e-9) && nearEqual(mFirst->getCenter().mLat, 47.45, 1e-6),
          "view set before Open");
    Check(!mFirst->isLayerVisible(XMapLayer::Grid), "layer set before Open");
    Check(mFirst->initialize() && mFirst->isInitialized(), "XMap::initialize");
    Check(mFirst->isVisible(), "opened visible");
    const prime::Geometry tAfter = mFirst->getGeometry();
    Check(tAfter.mLeft == 900 && tAfter.mTop == 720 && tAfter.mWidth == 420 && tAfter.mHeight == 300,
          "geometry kept by Open", std::format("{} {} {} {}", tAfter.mLeft, tAfter.mTop, tAfter.mWidth, tAfter.mHeight));
    Check(mFirst->getMode() == prime::XMapMode::Vfr && nearEqual(mFirst->getZoom(), 10.0, 1e-9) &&
              !mFirst->isLayerVisible(XMapLayer::Grid),
          "pre-open settings are in effect after Open");
    mFirst->setZoom(16.0);
    Check(nearEqual(mFirst->getZoom(), 14.0, 1e-9), "zoom limit of this map (max 14)", std::format("{}", mFirst->getZoom()));
    mFirst->setZoom(10.0);
    mFirst->zoomIn();
    Check(nearEqual(mFirst->getZoom(), 11.0, 1e-9), "zoomIn uses this map's zoom step (1.0)");
    double tMinZoom = 0.0;
    double tMaxZoom = 0.0;
    mFirst->getLayerZoomRange(XMapLayer::AirwaysLow, &tMinZoom, &tMaxZoom);
    Check(nearEqual(tMinZoom, 8.0, 1e-9), "group zoom range reaches its layers", std::format("{} {}", tMinZoom, tMaxZoom));
    Check(!mFirst->setLayerZoomRange(XMapLayer::Vor, 12.0, 9.0), "inverted zoom range is refused");
    Check(!mFirst->setName("other"), "setName refused while the map is open");
    Check(fileExists(mFirst->getSettingsDirectory() + "map_settings.json") &&
              fileExists(mFirst->getSettingsDirectory() + "terrain_colors.json"),
          "settings files created by Open");

    // --- Ikinci harita: bagimsiz
    mSecond = tLibrary.addXMap();
    Check(mSecond != nullptr && mSecond->getId() != mFirst->getId(), "second map has its own id");
    if (!mSecond)
    {
        return;
    }
    Check(!mSecond->setName("selftest_a"), "a name used by another map is refused");
    Check(mSecond->setName("selftest_b"), "second map named");
    mSecond->setPersistence(prime::Persistence::SaveOnly);
    mSecond->setStyle(prime::WindowStyle::Embedded);
    mSecond->setGeometry({900, 400, 420, 300});
    mSecond->setMode(prime::XMapMode::Dark);
    mSecond->setCenter(49.0, 2.55);
    mSecond->setZoom(8.0);
    Check(mSecond->initialize(), "second XMap::initialize");
    Check(tLibrary.getOpenXMapCount() == mMapsBefore + 2, "getOpenXMapCount",
          std::format("{}", tLibrary.getOpenXMapCount()));
    Check(tLibrary.findXMap("selftest_b") == mSecond && tLibrary.findXMap(mFirst->getId()) == mFirst &&
              tLibrary.findXMap("no_such_map") == nullptr,
          "findXMap by name / id");

    // Gorunum
    Check(nearEqual(mFirst->getCenter().mLat, 47.45, 1e-6) && nearEqual(mSecond->getCenter().mLat, 49.0, 1e-6),
          "independent centers");
    mFirst->setZoom(12.0);
    Check(nearEqual(mSecond->getZoom(), 8.0, 1e-9), "zooming map 1 does not change map 2");
    Check(mSecond->goToAirport("KSEA") || !tLibrary.getStatus().mAirportsReady, "goToAirport on map 2");
    Check(nearEqual(mFirst->getZoom(), 12.0, 1e-9) && nearEqual(mFirst->getCenter().mLon, -122.30, 1e-6),
          "goToAirport on map 2 does not move map 1");
    mSecond->setCenter(49.0, 2.55);
    mSecond->setZoom(17.0);
    Check(nearEqual(mSecond->getZoom(), prime::XMap::maxZoom(), 1e-9), "map 2 keeps the default zoom limit (16.5)");
    mSecond->setZoom(8.0);
    // Mod ve katmanlar
    Check(mFirst->getMode() == prime::XMapMode::Vfr && mSecond->getMode() == prime::XMapMode::Dark, "independent modes");
    mSecond->setMode(prime::XMapMode::Scenery);
    Check(mFirst->getMode() == prime::XMapMode::Vfr, "changing the mode of map 2 keeps map 1");
    Check(!mFirst->isLayerVisible(XMapLayer::Grid) && mSecond->isLayerVisible(XMapLayer::Grid), "independent layers");
    mSecond->setIsLayerVisible(XMapLayer::Roads, false);
    Check(mFirst->isLayerVisible(XMapLayer::Roads) == tLibrary.isLayerDefaultVisible(XMapLayer::Roads, prime::XMapMode::Vfr),
          "hiding a layer on map 2 keeps map 1");
    // Cizimler
    mFirst->setDrawTool(prime::DrawTool::Point);
    Check(mFirst->addDrawingPoint(47.5, -122.3), "drawing on map 1");
    Check(mFirst->getShapes().size() == 1 && mSecond->getShapes().empty(), "independent drawings");
    Check(mSecond->getDrawTool() == prime::DrawTool::Pan, "independent draw tools");
    mFirst->setDrawTool(prime::DrawTool::Pan);
    // Ucak kaynagi
    mFirst->setAircraftSource(prime::DataSource::Host);
    prime::AircraftState tAircraft;
    tAircraft.mValid = true;
    tAircraft.mPosition = {40.0, 30.0};
    mFirst->setAircraft(tAircraft);
    Check(mSecond->getAircraftSource() == prime::DataSource::XPlane, "independent aircraft sources");
    // Zemin ayarlari
    const prime::TerrainSettings tSecondTerrain = mSecond->getTerrainSettings();
    prime::TerrainSettings tFirstTerrain = mFirst->getTerrainSettings();
    tFirstTerrain.mShadingStrength = 0.95;
    tFirstTerrain.mVfrContourLines = true;
    mFirst->setTerrainSettings(tFirstTerrain);
    Check(nearEqual(mFirst->getTerrainSettings().mShadingStrength, 0.95, 1e-9) &&
              nearEqual(mSecond->getTerrainSettings().mShadingStrength, tSecondTerrain.mShadingStrength, 1e-9),
          "independent terrain settings");

    // --- Ortak onbellek: sinirlar acik harita sayisiyla buyur
    const prime::Status tStatus = tLibrary.getStatus();
    const int tOpen = mMapsBefore + 2;
    Check(tStatus.mOpenMaps == tOpen, "Status::mOpenMaps", std::format("{}", tStatus.mOpenMaps));
    Check(tStatus.mTerrainCacheLimitMb == grownLimit(1000, 50, tOpen), "disk cache limit grows with open maps",
          std::format("{} MB for {} maps", tStatus.mTerrainCacheLimitMb, tOpen));
    Check(tStatus.mTextureLimitMb == grownLimit(400, 50, tOpen), "texture budget grows with open maps",
          std::format("{} MB", tStatus.mTextureLimitMb));
    Check(mFirst->saveSettings(), "XMap::saveSettings");
    Check(fileExists(mFirst->getSettingsDirectory() + "map_drawings.txt"), "drawings file written");
}

void MultiXMapSelfTest::runAfterDraw()
{
    if (!mFirst || !mSecond)
    {
        return;
    }
    prime::XMapManager& tLibrary = *mManager;
    write(std::format("status after drawing: map1 '{}' pending {}, map2 '{}' pending {}", mFirst->getStatusText(),
                      mFirst->getTerrainPendingImages(), mSecond->getStatusText(),
                      mSecond->getTerrainPendingImages()));
    const prime::Geometry tGeometry = mFirst->getGeometry();
    mFirst->queryAtScreen(tGeometry.mLeft + tGeometry.mWidth / 2, tGeometry.mTop - tGeometry.mHeight / 2);
    Check(true, "queryAtScreen on an open map (no crash)");

    // --- Kapat: nesne ve ayarlari kalir
    const int tOpenBefore = tLibrary.getOpenXMapCount();
    mFirst->close();
    Check(!mFirst->isInitialized() && !mFirst->isVisible(), "Close");
    Check(tLibrary.getOpenXMapCount() == tOpenBefore - 1, "open count drops after Close");
    Check(tLibrary.getStatus().mTerrainCacheLimitMb == grownLimit(1000, 50, tOpenBefore - 1),
          "disk cache limit shrinks after Close", std::format("{} MB", tLibrary.getStatus().mTerrainCacheLimitMb));
    Check(mFirst->getMode() == prime::XMapMode::Vfr && mFirst->getShapes().size() == 1,
          "closed map keeps its settings and drawings");
    Check(mSecond->isInitialized(), "closing map 1 keeps map 2 open");

    // --- Oncelik kurali: LoadAndSave'de Open, dosyadaki degeri kodla verilenin yerine koyar
    mFirst->setMode(prime::XMapMode::IfrLow);  // kapaliyken kodla
    mFirst->setPersistence(prime::Persistence::LoadAndSave);
    Check(mFirst->initialize(), "reopen");
    Check(mFirst->getMode() == prime::XMapMode::Vfr, "LoadAndSave: the file (VFR) wins over the pre-open value",
          std::format("mode {}", static_cast<int>(mFirst->getMode())));
    Check(!mFirst->isLayerVisible(XMapLayer::Grid), "layers restored from the file");
    Check(mFirst->getShapes().size() == 1, "drawings restored from the file");
    Check(nearEqual(mFirst->getTerrainSettings().mShadingStrength, 0.95, 1e-6), "terrain settings restored from the file");
    Check(mFirst->getAircraftSource() == prime::DataSource::Host, "aircraft source restored from the file");
    double tMinZoom = 0.0;
    double tMaxZoom = 0.0;
    mFirst->getLayerZoomRange(XMapLayer::AirwaysHigh, &tMinZoom, &tMaxZoom);
    Check(nearEqual(tMinZoom, 8.0, 1e-6), "layer zoom range restored from the file");
    Check(nearEqual(mFirst->getMaxZoomLimit(), 14.0, 1e-6), "zoom limits restored from the file");
    mFirst->setMode(prime::XMapMode::IfrHigh);  // acikken: her zaman uygulanir
    Check(mFirst->getMode() == prime::XMapMode::IfrHigh, "a setting made after Open always applies");
}

void MultiXMapSelfTest::runCleanup()
{
    prime::XMapManager& tLibrary = *mManager;
    if (mFirst)
    {
        mManager->removeXMap(mFirst);
        mFirst = nullptr;
    }
    if (mSecond)
    {
        mManager->removeXMap(mSecond);
        mSecond = nullptr;
    }
    Check(tLibrary.getOpenXMapCount() == mMapsBefore, "maps released: open count back",
          std::format("{}", tLibrary.getOpenXMapCount()));
    Check(tLibrary.findXMap("selftest_a") == nullptr, "released map is no longer found");
    tLibrary.setSettings(mOriginalSettings);  // prime_map.json eski haline
    write(std::format("DONE: {} passed, {} failed", mPassed, mFailed));
}
