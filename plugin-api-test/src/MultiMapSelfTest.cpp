// MultiMapSelfTest.cpp
#include "MultiMapSelfTest.h"

#include <cmath>
#include <filesystem>
#include <format>
#include <system_error>

#include "XPLMProcessing.h"
#include "XPLMUtilities.h"

namespace
{

using prime::MapLayer;

const char* kFlagFile = "selftest.flag";
const float kDrawWait = 3.0f;     // 2. asamadan once haritalar bu kadar cizilsin (s)
const float kCleanupWait = 2.0f;  // 3. asamadan once (s)

bool Near(double pA, double pB, double pTolerance)
{
    return std::fabs(pA - pB) <= pTolerance;
}

bool FileExists(const std::string& pPath)
{
    std::error_code tError;
    return std::filesystem::exists(pPath, tError);
}

void Write(const std::string& pLine)
{
    XPLMDebugString(("[plugin-api-test] SELFTEST " + pLine + "\n").c_str());
}

// taban x (1 + artis% x (n - 1) / 100): kutuphanenin sinir formulu (PrimeMap.h LibrarySettings)
int GrownLimit(int pBase, int pGrowthPercent, int pOpenMaps)
{
    return static_cast<int>(std::lround(pBase * (1.0 + pGrowthPercent / 100.0 * (pOpenMaps - 1))));
}

}  // namespace

MultiMapSelfTest::~MultiMapSelfTest()
{
    if (mRegistered)
    {
        XPLMUnregisterFlightLoopCallback(FlightLoopCallback, this);
    }
}

bool MultiMapSelfTest::StartIfRequested(const std::string& pPluginDirectory)
{
    if (!FileExists(pPluginDirectory + kFlagFile))
    {
        return false;
    }
    Write("started (flag file found: " + pPluginDirectory + kFlagFile + ")");
    mPhase = 1;
    mPhaseStart = XPLMGetElapsedTime();
    RunImmediate();
    XPLMRegisterFlightLoopCallback(FlightLoopCallback, -1.0f, this);
    mRegistered = true;
    return true;
}

float MultiMapSelfTest::FlightLoopCallback(float /*pSinceLastCall*/, float /*pSinceLastLoop*/, int /*pCounter*/,
                                           void* pRefcon)
{
    return static_cast<MultiMapSelfTest*>(pRefcon)->Step();
}

float MultiMapSelfTest::Step()
{
    const float tNow = XPLMGetElapsedTime();
    if (mPhase == 1 && tNow - mPhaseStart > kDrawWait)
    {
        RunAfterDraw();
        mPhase = 2;
        mPhaseStart = tNow;
    }
    else if (mPhase == 2 && tNow - mPhaseStart > kCleanupWait)
    {
        RunCleanup();
        mPhase = 3;
        return 0.0f;  // bitti: bir daha cagirma
    }
    return -1.0f;
}

void MultiMapSelfTest::Check(bool pCondition, const std::string& pName, const std::string& pDetail)
{
    (pCondition ? mPassed : mFailed)++;
    Write(std::format("{} {}{}", pCondition ? "PASS" : "FAIL", pName, pDetail.empty() ? "" : "  (" + pDetail + ")"));
}

void MultiMapSelfTest::RunImmediate()
{
    prime::MapLibrary& tLibrary = prime::MapLibrary::Get();

    // --- Kutuphane: surum, klasorler, merkezi ayarlar
    Check(prime::VersionString() == "0.23.0", "VersionString", prime::VersionString());
    Check(!tLibrary.GetDataDirectory().empty() && !tLibrary.GetCacheDirectory().empty() &&
              !tLibrary.GetMapsDirectory().empty(),
          "data / cache / maps directories", tLibrary.GetDataDirectory());
    Check(FileExists(tLibrary.GetSettingsFilePath()), "prime_map.ini exists", tLibrary.GetSettingsFilePath());
    mOriginalSettings = tLibrary.GetSettings();
    prime::LibrarySettings tSettings = mOriginalSettings;
    tSettings.mTerrainCacheMb = 1000;
    tSettings.mCacheGrowthPercentPerMap = 50;
    tSettings.mTerrainCacheMaxMb = 0;
    tSettings.mTextureMemoryMb = 400;
    tSettings.mMemoryGrowthPercentPerMap = 50;
    tSettings.mTextureMemoryMaxMb = 0;
    Check(tLibrary.SetSettings(tSettings), "MapLibrary::SetSettings");
    Check(tLibrary.GetSettings().mCacheGrowthPercentPerMap == 50 && tLibrary.GetSettings().mTerrainCacheMb == 1000,
          "GetSettings returns the new values");
    mMapsBefore = tLibrary.GetOpenMapCount();

    // --- Fabrika: nesne pencere acilmadan olusur, butun ayarlar onceden verilir
    mFirst = tLibrary.CreateMap();
    Check(mFirst != nullptr, "CreateMap() returns a map object");
    if (!mFirst)
    {
        return;
    }
    Check(!mFirst->IsOpen() && !mFirst->IsVisible(), "new map is not open");
    Check(mFirst->GetId() > 0, "GetId", std::format("{}", mFirst->GetId()));
    Check(tLibrary.GetOpenMapCount() == mMapsBefore, "unopened map is not counted as open");
    Check(mFirst->SetName("selftest_a") && mFirst->GetName() == "selftest_a", "SetName / GetName");
    Check(mFirst->GetSettingsDirectory() == tLibrary.GetMapsDirectory() + "selftest_a/", "GetSettingsDirectory",
          mFirst->GetSettingsDirectory());
    mFirst->SetPersistence(prime::Persistence::SaveOnly);  // kod her acilista kazansin, dosya yine yazilsin
    Check(mFirst->GetPersistence() == prime::Persistence::SaveOnly, "SetPersistence / GetPersistence");
    mFirst->SetStyle(prime::WindowStyle::Embedded);
    mFirst->SetGeometry({900, 720, 420, 300});
    mFirst->SetMode(prime::MapMode::Vfr);
    mFirst->SetCenter(47.45, -122.30);
    mFirst->SetZoomLimits(5.0, 14.0);
    mFirst->SetZoom(10.0);
    mFirst->SetLayerVisible(MapLayer::Grid, false);
    mFirst->SetLayerZoomRange(MapLayer::AirwaysGroup, 8.0, 99.0);
    mFirst->SetZoomStep(1.0);
    mFirst->SetMaxRenderFps(10);
    mFirst->SetVisible(true);
    const prime::Geometry tBefore = mFirst->GetGeometry();
    Check(tBefore.mLeft == 900 && tBefore.mTop == 720 && tBefore.mWidth == 420 && tBefore.mHeight == 300,
          "geometry set before Open");
    Check(mFirst->GetMode() == prime::MapMode::Vfr, "mode set before Open");
    Check(Near(mFirst->GetZoom(), 10.0, 1e-9) && Near(mFirst->GetCenter().mLat, 47.45, 1e-6),
          "view set before Open");
    Check(!mFirst->IsLayerVisible(MapLayer::Grid), "layer set before Open");
    Check(mFirst->Open() && mFirst->IsOpen(), "Map::Open");
    Check(mFirst->IsVisible(), "opened visible");
    const prime::Geometry tAfter = mFirst->GetGeometry();
    Check(tAfter.mLeft == 900 && tAfter.mTop == 720 && tAfter.mWidth == 420 && tAfter.mHeight == 300,
          "geometry kept by Open", std::format("{} {} {} {}", tAfter.mLeft, tAfter.mTop, tAfter.mWidth, tAfter.mHeight));
    Check(mFirst->GetMode() == prime::MapMode::Vfr && Near(mFirst->GetZoom(), 10.0, 1e-9) &&
              !mFirst->IsLayerVisible(MapLayer::Grid),
          "pre-open settings are in effect after Open");
    mFirst->SetZoom(16.0);
    Check(Near(mFirst->GetZoom(), 14.0, 1e-9), "zoom limit of this map (max 14)", std::format("{}", mFirst->GetZoom()));
    mFirst->SetZoom(10.0);
    mFirst->ZoomIn();
    Check(Near(mFirst->GetZoom(), 11.0, 1e-9), "ZoomIn uses this map's zoom step (1.0)");
    double tMinZoom = 0.0;
    double tMaxZoom = 0.0;
    mFirst->GetLayerZoomRange(MapLayer::AirwaysLow, &tMinZoom, &tMaxZoom);
    Check(Near(tMinZoom, 8.0, 1e-9), "group zoom range reaches its layers", std::format("{} {}", tMinZoom, tMaxZoom));
    Check(!mFirst->SetLayerZoomRange(MapLayer::Vor, 12.0, 9.0), "inverted zoom range is refused");
    Check(!mFirst->SetName("other"), "SetName refused while the map is open");
    Check(FileExists(mFirst->GetSettingsDirectory() + "map_settings.ini") &&
              FileExists(mFirst->GetSettingsDirectory() + "terrain_colors.ini"),
          "settings files created by Open");

    // --- Ikinci harita: bagimsiz
    mSecond = tLibrary.CreateMap();
    Check(mSecond != nullptr && mSecond->GetId() != mFirst->GetId(), "second map has its own id");
    if (!mSecond)
    {
        return;
    }
    Check(!mSecond->SetName("selftest_a"), "a name used by another map is refused");
    Check(mSecond->SetName("selftest_b"), "second map named");
    mSecond->SetPersistence(prime::Persistence::SaveOnly);
    mSecond->SetStyle(prime::WindowStyle::Embedded);
    mSecond->SetGeometry({900, 400, 420, 300});
    mSecond->SetMode(prime::MapMode::Dark);
    mSecond->SetCenter(49.0, 2.55);
    mSecond->SetZoom(8.0);
    Check(mSecond->Open(), "second Map::Open");
    Check(tLibrary.GetOpenMapCount() == mMapsBefore + 2, "GetOpenMapCount",
          std::format("{}", tLibrary.GetOpenMapCount()));
    Check(tLibrary.FindMap("selftest_b") == mSecond.get() && tLibrary.FindMap(mFirst->GetId()) == mFirst.get() &&
              tLibrary.FindMap("no_such_map") == nullptr,
          "FindMap by name / id");

    // Gorunum
    Check(Near(mFirst->GetCenter().mLat, 47.45, 1e-6) && Near(mSecond->GetCenter().mLat, 49.0, 1e-6),
          "independent centers");
    mFirst->SetZoom(12.0);
    Check(Near(mSecond->GetZoom(), 8.0, 1e-9), "zooming map 1 does not change map 2");
    Check(mSecond->GoToAirport("KSEA") || !tLibrary.GetStatus().mAirportsReady, "GoToAirport on map 2");
    Check(Near(mFirst->GetZoom(), 12.0, 1e-9) && Near(mFirst->GetCenter().mLon, -122.30, 1e-6),
          "GoToAirport on map 2 does not move map 1");
    mSecond->SetCenter(49.0, 2.55);
    mSecond->SetZoom(17.0);
    Check(Near(mSecond->GetZoom(), prime::Map::MaxZoom(), 1e-9), "map 2 keeps the default zoom limit (16.5)");
    mSecond->SetZoom(8.0);
    // Mod ve katmanlar
    Check(mFirst->GetMode() == prime::MapMode::Vfr && mSecond->GetMode() == prime::MapMode::Dark, "independent modes");
    mSecond->SetMode(prime::MapMode::Scenery);
    Check(mFirst->GetMode() == prime::MapMode::Vfr, "changing the mode of map 2 keeps map 1");
    Check(!mFirst->IsLayerVisible(MapLayer::Grid) && mSecond->IsLayerVisible(MapLayer::Grid), "independent layers");
    mSecond->SetLayerVisible(MapLayer::Roads, false);
    Check(mFirst->IsLayerVisible(MapLayer::Roads) == prime::IsLayerDefaultVisible(MapLayer::Roads, prime::MapMode::Vfr),
          "hiding a layer on map 2 keeps map 1");
    // Cizimler
    mFirst->SetDrawTool(prime::DrawTool::Point);
    Check(mFirst->AddDrawingPoint(47.5, -122.3), "drawing on map 1");
    Check(mFirst->GetShapes().size() == 1 && mSecond->GetShapes().empty(), "independent drawings");
    Check(mSecond->GetDrawTool() == prime::DrawTool::Pan, "independent draw tools");
    mFirst->SetDrawTool(prime::DrawTool::Pan);
    // Ucak kaynagi
    mFirst->SetAircraftSource(prime::DataSource::Host);
    prime::AircraftState tAircraft;
    tAircraft.mValid = true;
    tAircraft.mPosition = {40.0, 30.0};
    mFirst->SetAircraft(tAircraft);
    Check(mSecond->GetAircraftSource() == prime::DataSource::XPlane, "independent aircraft sources");
    // Zemin ayarlari
    const prime::TerrainSettings tSecondTerrain = mSecond->GetTerrainSettings();
    prime::TerrainSettings tFirstTerrain = mFirst->GetTerrainSettings();
    tFirstTerrain.mShadingStrength = 0.95;
    tFirstTerrain.mVfrContourLines = true;
    mFirst->SetTerrainSettings(tFirstTerrain);
    Check(Near(mFirst->GetTerrainSettings().mShadingStrength, 0.95, 1e-9) &&
              Near(mSecond->GetTerrainSettings().mShadingStrength, tSecondTerrain.mShadingStrength, 1e-9),
          "independent terrain settings");

    // --- Ortak onbellek: sinirlar acik harita sayisiyla buyur
    const prime::Status tStatus = tLibrary.GetStatus();
    const int tOpen = mMapsBefore + 2;
    Check(tStatus.mOpenMaps == tOpen, "Status::mOpenMaps", std::format("{}", tStatus.mOpenMaps));
    Check(tStatus.mTerrainCacheLimitMb == GrownLimit(1000, 50, tOpen), "disk cache limit grows with open maps",
          std::format("{} MB for {} maps", tStatus.mTerrainCacheLimitMb, tOpen));
    Check(tStatus.mTextureLimitMb == GrownLimit(400, 50, tOpen), "texture budget grows with open maps",
          std::format("{} MB", tStatus.mTextureLimitMb));
    Check(mFirst->SaveSettings(), "Map::SaveSettings");
    Check(FileExists(mFirst->GetSettingsDirectory() + "map_drawings.txt"), "drawings file written");
}

void MultiMapSelfTest::RunAfterDraw()
{
    if (!mFirst || !mSecond)
    {
        return;
    }
    prime::MapLibrary& tLibrary = prime::MapLibrary::Get();
    Write(std::format("status after drawing: map1 '{}' pending {}, map2 '{}' pending {}", mFirst->GetStatusText(),
                      mFirst->GetTerrainPendingImages(), mSecond->GetStatusText(),
                      mSecond->GetTerrainPendingImages()));
    const prime::Geometry tGeometry = mFirst->GetGeometry();
    mFirst->QueryAtScreen(tGeometry.mLeft + tGeometry.mWidth / 2, tGeometry.mTop - tGeometry.mHeight / 2);
    Check(true, "QueryAtScreen on an open map (no crash)");

    // --- Kapat: nesne ve ayarlari kalir
    const int tOpenBefore = tLibrary.GetOpenMapCount();
    mFirst->Close();
    Check(!mFirst->IsOpen() && !mFirst->IsVisible(), "Close");
    Check(tLibrary.GetOpenMapCount() == tOpenBefore - 1, "open count drops after Close");
    Check(tLibrary.GetStatus().mTerrainCacheLimitMb == GrownLimit(1000, 50, tOpenBefore - 1),
          "disk cache limit shrinks after Close", std::format("{} MB", tLibrary.GetStatus().mTerrainCacheLimitMb));
    Check(mFirst->GetMode() == prime::MapMode::Vfr && mFirst->GetShapes().size() == 1,
          "closed map keeps its settings and drawings");
    Check(mSecond->IsOpen(), "closing map 1 keeps map 2 open");

    // --- Oncelik kurali: LoadAndSave'de Open, dosyadaki degeri kodla verilenin yerine koyar
    mFirst->SetMode(prime::MapMode::IfrLow);  // kapaliyken kodla
    mFirst->SetPersistence(prime::Persistence::LoadAndSave);
    Check(mFirst->Open(), "reopen");
    Check(mFirst->GetMode() == prime::MapMode::Vfr, "LoadAndSave: the file (VFR) wins over the pre-open value",
          std::format("mode {}", static_cast<int>(mFirst->GetMode())));
    Check(!mFirst->IsLayerVisible(MapLayer::Grid), "layers restored from the file");
    Check(mFirst->GetShapes().size() == 1, "drawings restored from the file");
    Check(Near(mFirst->GetTerrainSettings().mShadingStrength, 0.95, 1e-6), "terrain settings restored from the file");
    Check(mFirst->GetAircraftSource() == prime::DataSource::Host, "aircraft source restored from the file");
    double tMinZoom = 0.0;
    double tMaxZoom = 0.0;
    mFirst->GetLayerZoomRange(MapLayer::AirwaysHigh, &tMinZoom, &tMaxZoom);
    Check(Near(tMinZoom, 8.0, 1e-6), "layer zoom range restored from the file");
    Check(Near(mFirst->GetMaxZoomLimit(), 14.0, 1e-6), "zoom limits restored from the file");
    mFirst->SetMode(prime::MapMode::IfrHigh);  // acikken: her zaman uygulanir
    Check(mFirst->GetMode() == prime::MapMode::IfrHigh, "a setting made after Open always applies");
}

void MultiMapSelfTest::RunCleanup()
{
    prime::MapLibrary& tLibrary = prime::MapLibrary::Get();
    mFirst.reset();
    mSecond.reset();
    Check(tLibrary.GetOpenMapCount() == mMapsBefore, "maps released: open count back",
          std::format("{}", tLibrary.GetOpenMapCount()));
    Check(tLibrary.FindMap("selftest_a") == nullptr, "released map is no longer found");
    tLibrary.SetSettings(mOriginalSettings);  // prime_map.ini eski haline
    Write(std::format("DONE: {} passed, {} failed", mPassed, mFailed));
}
