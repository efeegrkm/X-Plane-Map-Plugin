// Bir yoneticinin haritalari arasinda paylasilan veri ve her karede ilerleyen ortak isler.
#ifndef PRIME_MAP_SRC_ENGINE_XMAPDATA_H
#define PRIME_MAP_SRC_ENGINE_XMAPDATA_H

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/XMapTypes.h"
#include "Settings.h"
#include "data/AircraftData.h"
#include "data/AirportDetailStore.h"
#include "data/AirportGeometryStore.h"
#include "data/AirwayDatabase.h"
#include "data/NavDatabase.h"
#include "data/NavLoader.h"
#include "data/TerrainClasses.h"
#include "data/TerrainStore.h"
#include "layers/TerrainHub.h"
#include "XMapView.h"

namespace prime
{
class XMap;
}

struct XMapDataConfig
{
    std::string mDataDirectory;
    std::string mCacheDirectory;
    prime::XMapManagerSettings mDefaults;
};

struct XMapDataStatus
{
    bool mNavReady = false;
    bool mAirportsReady = false;
    bool mAirwaysReady = false;
    bool mTerrainIndexReady = false;
    int mTerrainPendingImages = 0;
    bool mPrecacheRunning = false;
    int mPrecacheDone = 0;
    int mPrecacheTotal = 0;
    std::string mText;
    int mOpenMaps = 0;
    int mTerrainCacheLimitMb = 0;
    int mTextureLimitMb = 0;
};

class XMapData
{
public:
    explicit XMapData(const XMapDataConfig& pConfig);
    ~XMapData();
    XMapData(const XMapData&) = delete;
    XMapData& operator=(const XMapData&) = delete;
    void start();
    void stop();
    bool isRunning() const { return mRunning; }

    void addInstance(prime::XMap* pInstance);
    void removeInstance(prime::XMap* pInstance);
    void openXMapsChanged();
    int openXMapCount() const;

    TerrainHub& hub();
    TerrainStore& terrain();
    NavDatabase& navs();
    AirwayDatabase& airways();
    AirportGeometryStore& airportGeometry();
    AirportDetailStore& airportDetails();
    const AirportGeometryStore& airports() const;
    const XMapDataStatus& status() const;

    int registerPalette(TerrainPalette* pPalette);

    const prime::XMapManagerSettings& managerSettings() const;
    void setManagerSettings(const prime::XMapManagerSettings& pSettings);
    std::string settingsPath() const;
    const std::string& dataDirectory() const;
    const std::string& cacheDirectory() const;
    std::string mapsDirectory() const;
    bool startPrecache(const GeoBounds& pBounds);
    void cancelPrecache();
    bool isBusy() const;
    const std::string& sharedStatusText() const;
    const std::string& terrainWorkerStatus() const;
    bool isTerrainIdle() const;
    void showNotice(const std::string& pText);

private:
    static float flightLoopCallback(float pSinceLastCall, float pSinceLastLoop, int pCounter, void* pRefcon);
    float onFlightLoop();
    void stepLoading();
    void updateStatus();
    void updatePrefetch(float pNow);
    void loadLibrarySettings();
    bool saveLibrarySettings() const;
    void applyLimits();
    static void logReport(const std::string& pReport);

    XMapDataConfig mConfig;
    prime::XMapManagerSettings mSettings;
    NavDatabase mNavDatabase;
    NavLoader mNavLoader{mNavDatabase};
    std::unique_ptr<AirportGeometryStore> mAirportGeometry;
    std::unique_ptr<AirportDetailStore> mAirportDetails;
    bool mLoadStarted = false;
    AirwayDatabase mAirways;
    TerrainPalette mDefaultPalette;
    TerrainStore mTerrainStore;
    TerrainHub mHub{mTerrainStore};
    std::string mXPlaneDirectory;
    bool mDerivedLoaded = false;
    bool mDerivedOk = false;
    std::array<TerrainColor, kTerrainClassCount> mDerivedColors = {};
    std::array<int, kTerrainClassCount> mDerivedCounts = {};
    std::map<std::string, int> mPaletteIds;
    std::vector<prime::XMap*> mInstances;
    AircraftData mAircraft;
    bool mMaplessPrefetchLogged = false;
    float mLastPrefetch = -100.0f;
    bool mPrecacheRunning = false;
    std::string mNotice;
    float mNoticeUntil = 0.0f;
    bool mFlightLoopRegistered = false;
    bool mRunning = false;
    bool mBusy = false;
    bool mTerrainIdle = true;
    std::string mSharedText;
    std::string mTerrainWorkerText;
    XMapDataStatus mStatus;
};
#endif
