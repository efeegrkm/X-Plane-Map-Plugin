// PrimeMap.cpp
// Genel API (PrimeMap.h) -> ic siniflar koprusu:
//
//   prime::MapLibrary (Init / Shutdown / GetStatus / ayarlar)  ->  MapData (ortak veri, onbellek, flight loop)
//   prime::MapLibrary::CreateMap / prime::Map                  ->  MapInstance (haritanin her seyi) + MapWindow
//   katman fonksiyonlari                                       ->  engine/LayerTable (panelle ayni kurallar)
//
// Kutuphanenin durumu (MapData + yasayan haritalar + host sayisi) MapLibrary'nin Impl'inde
// yasar: prime_map.dll basina TEK ornek (MapLibrary::Get()). Ayni X-Plane surecindeki tum
// hostlar ayni DLL'i kullandigi icin durumu paylasir: Init bir kez yapilir, coklu host
// referans sayaciyla yonetilir. Butun fonksiyonlar X-Plane ana thread'inden cagrilmalidir.
//
// 0.23: her Map kendi MapInstance'ina sahiptir (katmanlar, cizimler, ucak kaynagi, zemin
// ayarlari, ayar dosyalari harita basina). MapInstance pencere acilmadan da tam calisir;
// bu yuzden asagidaki fonksiyonlarin cogu "pencere acik mi" diye bakmaz.
#include "PrimeMap.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <format>
#include <sstream>
#include <vector>

#include "XPLMPlugin.h"
#include "XPLMUtilities.h"

#include "Drawings.h"
#include "LayerManager.h"
#include "Log.h"
#include "MapTheme.h"
#include "MapView.h"
#include "MapWindow.h"
#include "Settings.h"
#include "data/AirportGeometry.h"
#include "data/AirportSearch.h"
#include "engine/LayerTable.h"
#include "engine/MapData.h"
#include "engine/MapInstance.h"
#include "layers/FmsLayer.h"
#include "layers/TrackLayer.h"

namespace
{

// Shape kimligi: tur * kShapeIdBase + etiket numarasi (oturum icinde sabit)
const int kShapeIdBase = 1000000;

std::string HostPluginDirectory()
{
    // .../Resources/plugins/<host>/64/win.xpl -> .../Resources/plugins/<host>/
    std::array<char, 2048> tXplPath = {};
    XPLMGetPluginInfo(XPLMGetMyID(), nullptr, tXplPath.data(), nullptr, nullptr);
    std::string tPath = tXplPath.data();
    for (int tLevel = 0; tLevel < 2; ++tLevel)  // dosya adini ve "64" klasorunu at
    {
        const size_t tCut = tPath.find_last_of("/\\");
        if (tCut != std::string::npos)
        {
            tPath.erase(tCut);
        }
    }
    return tPath + XPLMGetDirectorySeparator();
}

// Klasor yolu sonunda ayirici ile
std::string WithSeparator(std::string pPath)
{
    if (!pPath.empty() && pPath.back() != '/' && pPath.back() != '\\')
    {
        pPath += '/';
    }
    return pPath;
}

std::string Upper(std::string pText)
{
    for (char& tChar : pText)
    {
        tChar = static_cast<char>(std::toupper(static_cast<unsigned char>(tChar)));
    }
    return pText;
}

prime::Rgb ParseRgb(std::istringstream& pStream)
{
    prime::Rgb tColor;
    pStream >> tColor.mR >> tColor.mG >> tColor.mB;
    return tColor;
}

RectI ToRect(const prime::Geometry& pGeometry)
{
    RectI tRect;
    tRect.mLeft = pGeometry.mLeft;
    tRect.mTop = pGeometry.mTop;
    tRect.mRight = pGeometry.mLeft + pGeometry.mWidth;
    tRect.mBottom = pGeometry.mTop - pGeometry.mHeight;
    return tRect;
}

}  // namespace

namespace prime
{

// ============================================================================
// Map::Impl (MapLibrary onu kullandigi icin once tanimli)
// ============================================================================

struct Map::Impl
{
    // Haritanin her seyi. Shutdown haritayi birakirsa nullptr olur: Map nesnesi yasar ama bostur
    // (butun fonksiyonlar bir sey yapmaz, false / bos doner).
    std::unique_ptr<MapInstance> mInstance;

    MapInstance* Instance() const
    {
        return mInstance.get();
    }

    // Pencere nesnesi (XPLM penceresi acik olmasa da ayar tutar); harita birakildiysa nullptr
    MapWindow* Window() const
    {
        return mInstance ? &mInstance->Window() : nullptr;
    }
};

// ============================================================================
// MapLibrary: kutuphanenin kendisi (surecte tek ornek)
// ============================================================================

struct MapLibrary::Impl
{
    std::unique_ptr<MapData> mData;  // ortak veri ve arka plan isleri
    std::vector<Map*> mMaps;         // yasayan harita nesneleri (olusturulma sirasiyla)
    int mHostCount = 0;              // Init/Shutdown dengesi (coklu host)
    int mNextMapId = 1;
};

MapLibrary& MapLibrary::Get()
{
    static MapLibrary tLibrary;
    return tLibrary;
}

MapLibrary::MapLibrary()
    : mImpl(std::make_unique<Impl>())
{
}

MapLibrary::~MapLibrary() = default;

std::string VersionString()
{
    return std::format("{}.{}.{}", kVersionMajor, kVersionMinor, kVersionPatch);
}

bool MapLibrary::Init(const InitConfig& pConfig)
{
    if (mImpl->mData)
    {
        // SHARED DLL: ayni X-Plane surecinde birden cok host eklenti olabilir
        // (resmi eklenti + test/uygulama eklentileri). Kutuphane durumu surecte
        // TEK'tir; ilk host'un yapilandirmasi (log oneki, klasorler) gecerli kalir.
        // Sonraki hostlarin Init'i ek is yapmaz, true doner.
        ++mImpl->mHostCount;
        Log("Init: library already running (hosts: {}); first host's config is in effect (data: {})",
            mImpl->mHostCount, mImpl->mData->DataDirectory());
        return true;
    }
    SetLogPrefix(pConfig.mLogPrefix);
    SetLogCallback(pConfig.mLogCallback);
    // Dosya yollari isletim sisteminin bicimiyle (C:\...); Windows'ta host'un yollarini degistirmez
    XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);

    MapDataConfig tConfig;
    tConfig.mDataDirectory = WithSeparator(pConfig.mDataDirectory.empty() ? HostPluginDirectory()
                                                                          : pConfig.mDataDirectory);
    tConfig.mCacheDirectory = WithSeparator(pConfig.mCacheDirectory.empty() ? tConfig.mDataDirectory + "cache/"
                                                                            : pConfig.mCacheDirectory);
    tConfig.mDefaults.mTerrainCacheMb = std::max(0, pConfig.mTerrainCacheMb);
    tConfig.mDefaults.mPrefetchTiles = std::max(0, pConfig.mPrefetchTiles);
    mImpl->mData = std::make_unique<MapData>(tConfig);
    mImpl->mData->Start();
    mImpl->mHostCount = 1;
    Log("Prime Map API {} ready (data: {}, cache: {})", VersionString(), tConfig.mDataDirectory,
        tConfig.mCacheDirectory);
    return true;
}

void MapLibrary::Shutdown()
{
    if (!mImpl->mData)
    {
        return;
    }
    // Referans sayaci: bir host cikiyor; diger hostlar kutuphaneyi kullaniyorsa
    // calisma surer, tam kapanma yalnizca son host cikarken olur.
    --mImpl->mHostCount;
    if (mImpl->mHostCount > 0)
    {
        Log("Shutdown: kept for other hosts (hosts: {})", mImpl->mHostCount);
        return;
    }
    mImpl->mHostCount = 0;
    for (Map* tMap : mImpl->mMaps)
    {
        // Host haritayi birakmayi unuttu: burada kapat (ayarlari yazilir). Map nesnesi host'ta
        // yasamaya devam eder ama bostur; yikicisi sorunsuz calisir.
        Log("WARNING: MapLibrary::Shutdown with a live map (id {}); closing it", tMap->GetId());
        tMap->mImpl->mInstance.reset();
    }
    mImpl->mMaps.clear();
    mImpl->mData->Stop();
    mImpl->mData.reset();
    Log("Prime Map API stopped");
}

bool MapLibrary::IsInitialized() const
{
    return mImpl->mData != nullptr;
}

Status MapLibrary::GetStatus() const
{
    Status tStatus;
    if (!mImpl->mData)
    {
        return tStatus;
    }
    const MapDataStatus& tData = mImpl->mData->Status();
    tStatus.mNavReady = tData.mNavReady;
    tStatus.mAirportsReady = tData.mAirportsReady;
    tStatus.mAirwaysReady = tData.mAirwaysReady;
    tStatus.mTerrainIndexReady = tData.mTerrainIndexReady;
    tStatus.mTerrainPendingImages = tData.mTerrainPendingImages;
    tStatus.mPrecacheRunning = tData.mPrecacheRunning;
    tStatus.mPrecacheDone = tData.mPrecacheDone;
    tStatus.mPrecacheTotal = tData.mPrecacheTotal;
    tStatus.mText = tData.mText;
    tStatus.mOpenMaps = tData.mOpenMaps;
    tStatus.mTerrainCacheLimitMb = tData.mTerrainCacheLimitMb;
    tStatus.mTextureLimitMb = tData.mTextureLimitMb;
    for (const Map* tMap : mImpl->mMaps)
    {
        if (tMap->IsDiagnosticsOn())
        {
            tStatus.mPerformanceText = tMap->mImpl->Window()->PerformanceText();
            break;
        }
    }
    return tStatus;
}

std::unique_ptr<Map> MapLibrary::CreateMap()
{
    if (!mImpl->mData)
    {
        Log("WARNING: CreateMap before MapLibrary::Init");
        return nullptr;
    }
    std::unique_ptr<Map> tMap(new Map());
    tMap->mImpl->mInstance = std::make_unique<MapInstance>(*mImpl->mData, mImpl->mNextMapId++);
    mImpl->mMaps.push_back(tMap.get());
    return tMap;
}

std::unique_ptr<Map> MapLibrary::CreateMap(const OpenConfig& pConfig)
{
    // 0.22 yolu: olustur + ayarla + hemen ac. Ayar dosyalarini host yonetir (Persistence::Off).
    std::unique_ptr<Map> tMap = CreateMap();
    if (!tMap)
    {
        return nullptr;
    }
    tMap->SetPersistence(Persistence::Off);
    tMap->SetStyle(pConfig.mStyle);
    tMap->SetGeometry(pConfig.mGeometry);
    tMap->SetMode(pConfig.mMode);
    tMap->SetUiVisible(pConfig.mUiVisible);
    tMap->SetMouseInput(pConfig.mMouseInput);
    tMap->SetVisible(pConfig.mVisible);
    tMap->SetImageCache(pConfig.mImageCache);
    tMap->SetMaxRenderFps(pConfig.mMaxRenderFps);
    tMap->SetXPlaneFont(pConfig.mXPlaneFont);
    tMap->Open();
    return tMap;
}

std::vector<Map*> MapLibrary::GetMaps() const
{
    return mImpl->mMaps;
}

Map* MapLibrary::FindMap(int pId) const
{
    for (Map* tMap : mImpl->mMaps)
    {
        if (tMap->GetId() == pId)
        {
            return tMap;
        }
    }
    return nullptr;
}

Map* MapLibrary::FindMap(const std::string& pName) const
{
    for (Map* tMap : mImpl->mMaps)
    {
        if (!pName.empty() && tMap->GetName() == pName)
        {
            return tMap;
        }
    }
    return nullptr;
}

int MapLibrary::GetOpenMapCount() const
{
    return mImpl->mData ? mImpl->mData->OpenMapCount() : 0;
}

LibrarySettings MapLibrary::GetSettings() const
{
    return mImpl->mData ? mImpl->mData->LibrarySettings() : LibrarySettings();
}

bool MapLibrary::SetSettings(const LibrarySettings& pSettings)
{
    if (!mImpl->mData)
    {
        Log("WARNING: SetSettings before MapLibrary::Init");
        return false;
    }
    mImpl->mData->SetLibrarySettings(pSettings);
    return true;
}

std::string MapLibrary::GetDataDirectory() const
{
    return mImpl->mData ? mImpl->mData->DataDirectory() : std::string();
}

std::string MapLibrary::GetSettingsFilePath() const
{
    return mImpl->mData ? mImpl->mData->SettingsPath() : std::string();
}

std::string MapLibrary::GetMapsDirectory() const
{
    return mImpl->mData ? mImpl->mData->MapsDirectory() : std::string();
}

std::string MapLibrary::GetCacheDirectory() const
{
    return mImpl->mData ? mImpl->mData->CacheDirectory() : std::string();
}

// ============================================================================
// Map
// ============================================================================

Map::Map()
    : mImpl(std::make_unique<Impl>())
{
}

std::unique_ptr<Map> Map::Open(const OpenConfig& pConfig)
{
    return MapLibrary::Get().CreateMap(pConfig);
}

Map::~Map()
{
    // Kutuphanenin harita listesinden dus (Map, MapLibrary'nin dostu); sonra harita kapanir
    // (MapInstance yikicisi: ayarlar yazilir, pencere kapanir)
    auto& tMaps = MapLibrary::Get().mImpl->mMaps;
    tMaps.erase(std::remove(tMaps.begin(), tMaps.end(), this), tMaps.end());
    mImpl->mInstance.reset();
}

// --- Kimlik, ad, ayar dosyalari --------------------------------------------------------

int Map::GetId() const
{
    return mImpl->Instance() ? mImpl->Instance()->Id() : 0;
}

bool Map::SetName(const std::string& pName)
{
    MapInstance* tInstance = mImpl->Instance();
    if (!tInstance)
    {
        return false;
    }
    if (tInstance->IsOpen())
    {
        Log("WARNING: SetName: map {} is open; close it first (the name selects the settings folder)",
            tInstance->Id());
        return false;
    }
    // Ayni klasoru iki harita kullanmasin (birbirinin dosyasini ezerlerdi)
    const std::string tFolder = MapInstance::SafeFolderName(pName);
    for (const Map* tOther : MapLibrary::Get().mImpl->mMaps)
    {
        if (tOther != this && tOther->mImpl->Instance() && !tFolder.empty() &&
            tOther->mImpl->Instance()->FolderName() == tFolder)
        {
            Log("WARNING: SetName: '{}' is already used by map {}", pName, tOther->GetId());
            return false;
        }
    }
    tInstance->SetName(pName);
    return true;
}

std::string Map::GetName() const
{
    return mImpl->Instance() ? mImpl->Instance()->Name() : std::string();
}

std::string Map::GetSettingsDirectory() const
{
    return mImpl->Instance() ? mImpl->Instance()->Directory() : std::string();
}

void Map::SetPersistence(Persistence pPersistence)
{
    if (MapInstance* tInstance = mImpl->Instance())
    {
        tInstance->SetPersistence(static_cast<MapPersistence>(pPersistence));
    }
}

Persistence Map::GetPersistence() const
{
    return mImpl->Instance() ? static_cast<Persistence>(mImpl->Instance()->Persistence()) : Persistence::Off;
}

bool Map::SaveSettings()
{
    return mImpl->Instance() && mImpl->Instance()->SaveNow();
}

// --- Acma / kapama -----------------------------------------------------------------------

bool Map::Open()
{
    return mImpl->Instance() && mImpl->Instance()->Open();
}

void Map::Close()
{
    if (MapInstance* tInstance = mImpl->Instance())
    {
        tInstance->Close();
    }
}

bool Map::IsOpen() const
{
    return mImpl->Instance() && mImpl->Instance()->IsOpen();
}

// --- Pencere ---------------------------------------------------------------------

void Map::SetStyle(WindowStyle pStyle)
{
    if (MapWindow* tWindow = mImpl->Window())
    {
        tWindow->SetEmbedded(pStyle == WindowStyle::Embedded);  // acik pencerede uyari loglar, degistirmez
    }
}

WindowStyle Map::GetStyle() const
{
    MapWindow* tWindow = mImpl->Window();
    return (tWindow && !tWindow->IsEmbedded()) ? WindowStyle::Standalone : WindowStyle::Embedded;
}

void Map::SetTitle(const std::string& pTitle)
{
    if (MapWindow* tWindow = mImpl->Window())
    {
        tWindow->SetTitle(pTitle);
    }
}

std::string Map::GetTitle() const
{
    MapWindow* tWindow = mImpl->Window();
    return tWindow ? tWindow->Title() : std::string();
}

void Map::SetGeometry(const Geometry& pGeometry)
{
    if (MapWindow* tWindow = mImpl->Window())
    {
        tWindow->SetGeometry(ToRect(pGeometry));
    }
}

Geometry Map::GetGeometry() const
{
    Geometry tGeometry;
    if (MapWindow* tWindow = mImpl->Window())
    {
        const RectI tRect = tWindow->GetGeometry();
        tGeometry.mLeft = tRect.mLeft;
        tGeometry.mTop = tRect.mTop;
        tGeometry.mWidth = tRect.Width();
        tGeometry.mHeight = tRect.Height();
    }
    return tGeometry;
}

void Map::SetVisible(bool pVisible)
{
    if (MapWindow* tWindow = mImpl->Window())
    {
        tWindow->SetVisible(pVisible);
    }
}

bool Map::IsVisible() const
{
    MapWindow* tWindow = mImpl->Window();
    return tWindow && tWindow->IsVisible();
}

void Map::BringToFront()
{
    if (MapWindow* tWindow = mImpl->Window())
    {
        tWindow->BringToFront();
    }
}

void Map::SetPoppedOut(bool pPoppedOut)
{
    if (MapWindow* tWindow = mImpl->Window())
    {
        tWindow->SetPoppedOut(pPoppedOut);
    }
}

bool Map::IsPoppedOut() const
{
    MapWindow* tWindow = mImpl->Window();
    return tWindow && tWindow->IsPoppedOut();
}

void Map::SetUiVisible(bool pVisible)
{
    if (MapWindow* tWindow = mImpl->Window())
    {
        tWindow->SetUiVisible(pVisible);
    }
}

bool Map::IsUiVisible() const
{
    MapWindow* tWindow = mImpl->Window();
    return tWindow && tWindow->IsUiVisible();
}

void Map::SetMouseInput(bool pEnabled)
{
    if (MapWindow* tWindow = mImpl->Window())
    {
        tWindow->SetMouseInput(pEnabled);
    }
}

bool Map::IsMouseInputEnabled() const
{
    MapWindow* tWindow = mImpl->Window();
    return tWindow && tWindow->IsMouseInputEnabled();
}

// --- Gorunum ----------------------------------------------------------------------

void Map::SetCenter(double pLat, double pLon)
{
    if (MapWindow* tWindow = mImpl->Window())
    {
        tWindow->SetFollowAircraft(false);  // takip acik kalirsa harita hemen ucaga doner
        tWindow->View().SetCenter({pLat, pLon});
    }
}

LatLon Map::GetCenter() const
{
    LatLon tCenter;
    if (MapWindow* tWindow = mImpl->Window())
    {
        const ::LatLon tInner = tWindow->View().Center();
        tCenter.mLat = tInner.mLat;
        tCenter.mLon = tInner.mLon;
    }
    return tCenter;
}

void Map::SetZoom(double pZoom)
{
    if (MapWindow* tWindow = mImpl->Window())
    {
        tWindow->View().SetZoom(pZoom);
    }
}

double Map::GetZoom() const
{
    MapWindow* tWindow = mImpl->Window();
    return tWindow ? tWindow->View().Zoom() : 0.0;
}

double Map::MinZoom()
{
    return MapView::kMinZoom;
}

double Map::MaxZoom()
{
    return MapView::kMaxZoom;
}

double Map::HardMaxZoom()
{
    return MapView::kHardMaxZoom;
}

bool Map::SetZoomLimits(double pMinZoom, double pMaxZoom)
{
    MapWindow* tWindow = mImpl->Window();
    if (!tWindow)
    {
        return false;
    }
    if (!(pMinZoom <= pMaxZoom))
    {
        Log("WARNING: SetZoomLimits: min {} is greater than max {}", pMinZoom, pMaxZoom);
        return false;
    }
    tWindow->SetZoomLimits(pMinZoom, pMaxZoom);
    return true;
}

double Map::GetMinZoomLimit() const
{
    MapWindow* tWindow = mImpl->Window();
    return tWindow ? tWindow->View().MinZoomLimit() : MapView::kMinZoom;
}

double Map::GetMaxZoomLimit() const
{
    MapWindow* tWindow = mImpl->Window();
    return tWindow ? tWindow->View().MaxZoomLimit() : MapView::kMaxZoom;
}

void Map::ZoomIn()
{
    SetZoom(GetZoom() + GetZoomStep());
}

void Map::ZoomOut()
{
    SetZoom(GetZoom() - GetZoomStep());
}

void Map::SetZoomStep(double pStep)
{
    MapWindow* tWindow = mImpl->Window();
    if (tWindow && pStep > 0.0 && pStep <= 5.0)
    {
        tWindow->SetZoomStep(pStep);
    }
    else
    {
        Log("WARNING: SetZoomStep: {} out of range (0, 5]", pStep);
    }
}

double Map::GetZoomStep() const
{
    MapWindow* tWindow = mImpl->Window();
    return tWindow ? tWindow->ZoomStep() : 0.5;
}

void Map::PanLeft()
{
    // Sola bakmak = harita icerigi saga kayar
    PanBy(GetGeometry().mWidth * GetPanStep(), 0.0);
}

void Map::PanRight()
{
    PanBy(-GetGeometry().mWidth * GetPanStep(), 0.0);
}

void Map::PanUp()
{
    // Kuzeye bakmak = icerik asagi kayar (y yukari artar)
    PanBy(0.0, -GetGeometry().mHeight * GetPanStep());
}

void Map::PanDown()
{
    PanBy(0.0, GetGeometry().mHeight * GetPanStep());
}

void Map::SetPanStep(double pFractionOfView)
{
    MapWindow* tWindow = mImpl->Window();
    if (tWindow && pFractionOfView > 0.0 && pFractionOfView <= 1.0)
    {
        tWindow->SetPanStep(pFractionOfView);
    }
    else
    {
        Log("WARNING: SetPanStep: {} out of range (0, 1]", pFractionOfView);
    }
}

double Map::GetPanStep() const
{
    MapWindow* tWindow = mImpl->Window();
    return tWindow ? tWindow->PanStep() : 0.25;
}

void Map::PanBy(double pDeltaX, double pDeltaY)
{
    if (MapWindow* tWindow = mImpl->Window())
    {
        tWindow->SetFollowAircraft(false);  // elle kaydirma gibi: takip biter
        tWindow->View().PanByBoxels(pDeltaX, pDeltaY);
    }
}

void Map::FitBounds(const GeoBounds& pBounds)
{
    MapWindow* tWindow = mImpl->Window();
    if (!tWindow || pBounds.mNorth <= pBounds.mSouth || pBounds.mEast <= pBounds.mWest)
    {
        Log("WARNING: FitBounds: empty or invalid area");
        return;
    }
    const Geometry tGeometry = GetGeometry();
    if (tGeometry.mWidth <= 0 || tGeometry.mHeight <= 0)
    {
        Log("WARNING: FitBounds: the map has no size yet (call SetGeometry first)");
        return;
    }
    const double tX0 = Mercator::LonToX(pBounds.mWest);
    const double tX1 = Mercator::LonToX(pBounds.mWest) + (pBounds.mEast - pBounds.mWest) / 360.0;
    const double tY0 = Mercator::LatToY(pBounds.mSouth);
    const double tY1 = Mercator::LatToY(pBounds.mNorth);
    const double tWidth = std::max(tX1 - tX0, 1e-12);
    const double tHeight = std::max(tY1 - tY0, 1e-12);
    // Dunya kenari 256 * 2^zoom boxel: alanin iki yonde de sigacagi en buyuk zoom; kenarlarda %5 pay
    // (alan pencere kenarina yapismasin, yuvarlama ile disarida kalmasin)
    const double kMargin = 1.05;
    const double tScale = std::min(tGeometry.mWidth / (MapView::kTileSize * tWidth * kMargin),
                                   tGeometry.mHeight / (MapView::kTileSize * tHeight * kMargin));
    tWindow->SetFollowAircraft(false);
    tWindow->View().SetCenterMercator(std::fmod(tX0 + tWidth * 0.5, 1.0), (tY0 + tY1) * 0.5);
    tWindow->View().SetZoom(std::log2(std::max(tScale, 1e-9)));
}

GeoBounds Map::GetVisibleBounds() const
{
    GeoBounds tBounds;
    if (MapWindow* tWindow = mImpl->Window())
    {
        const ::GeoBounds tInner = tWindow->View().VisibleBounds();
        tBounds.mSouth = tInner.mSouth;
        tBounds.mWest = tInner.mWest;
        tBounds.mNorth = tInner.mNorth;
        tBounds.mEast = tInner.mEast;
    }
    return tBounds;
}

double Map::GetMetersPerBoxel() const
{
    MapWindow* tWindow = mImpl->Window();
    return tWindow ? tWindow->View().MetersPerBoxel() : 0.0;
}

void Map::SetFollowAircraft(bool pFollow)
{
    if (MapWindow* tWindow = mImpl->Window())
    {
        tWindow->SetFollowAircraft(pFollow);
    }
}

bool Map::IsFollowingAircraft() const
{
    MapWindow* tWindow = mImpl->Window();
    return tWindow && tWindow->IsFollowingAircraft();
}

void Map::CenterOnAircraft()
{
    if (MapWindow* tWindow = mImpl->Window())
    {
        tWindow->CenterOnAircraft();
    }
}

bool Map::GoToAirport(const std::string& pIdent)
{
    MapWindow* tWindow = mImpl->Window();
    if (!tWindow)
    {
        return false;
    }
    const AirportGeometryStore& tAirports = mImpl->Instance()->Data().Airports();
    const std::string tWanted = Upper(pIdent);
    for (int tIndex : ::FindAirports(tAirports, pIdent, 8))
    {
        if (Upper(tAirports.At(tIndex).mId) == tWanted)
        {
            tWindow->GoToAirport(tIndex);
            return true;
        }
    }
    Log("go to airport: '{}' not found", pIdent);
    return false;
}

std::vector<AirportMatch> Map::FindAirports(const std::string& pQuery, int pMaxResults) const
{
    std::vector<AirportMatch> tMatches;
    if (!mImpl->Instance())
    {
        return tMatches;
    }
    const AirportGeometryStore& tAirports = mImpl->Instance()->Data().Airports();
    for (int tIndex : ::FindAirports(tAirports, pQuery, std::max(0, pMaxResults)))
    {
        const AirportGeometry& tAirport = tAirports.At(tIndex);
        const ::LatLon tCenter = AirportCenter(tAirport);
        AirportMatch tMatch;
        tMatch.mIdent = tAirport.mId;
        tMatch.mName = tAirport.mName;
        tMatch.mPosition.mLat = tCenter.mLat;
        tMatch.mPosition.mLon = tCenter.mLon;
        tMatches.push_back(tMatch);
    }
    return tMatches;
}

LatLon Map::ScreenToGeo(int pX, int pY) const
{
    LatLon tPoint;
    if (MapWindow* tWindow = mImpl->Window())
    {
        const ::LatLon tInner = tWindow->View().BoxelToLatLon(pX, pY);
        tPoint.mLat = tInner.mLat;
        tPoint.mLon = tInner.mLon;
    }
    return tPoint;
}

bool Map::GeoToScreen(double pLat, double pLon, int* pX, int* pY) const
{
    MapWindow* tWindow = mImpl->Window();
    if (!tWindow || !pX || !pY)
    {
        return false;
    }
    double tX = 0.0;
    double tY = 0.0;
    tWindow->View().LatLonToBoxel({pLat, pLon}, &tX, &tY);
    *pX = static_cast<int>(std::lround(tX));
    *pY = static_cast<int>(std::lround(tY));
    const RectD& tArea = tWindow->View().Viewport();
    return tX >= tArea.mLeft && tX <= tArea.mRight && tY >= tArea.mBottom && tY <= tArea.mTop;
}

// --- Mod ----------------------------------------------------------------------

void Map::SetMode(MapMode pMode)
{
    if (MapWindow* tWindow = mImpl->Window())
    {
        tWindow->SetMapMode(static_cast<::MapMode>(pMode));
    }
}

MapMode Map::GetMode() const
{
    MapWindow* tWindow = mImpl->Window();
    return tWindow ? static_cast<MapMode>(tWindow->GetMapMode()) : MapMode::Scenery;
}

void Map::NextMode()
{
    if (MapWindow* tWindow = mImpl->Window())
    {
        tWindow->NextMapMode();
    }
}

// --- Katmanlar -----------------------------------------------------------------

void Map::SetLayerVisible(MapLayer pLayer, bool pVisible)
{
    MapWindow* tWindow = mImpl->Window();
    if (tWindow &&
        LayerRules::SetVisible(mImpl->Instance()->Layers(), ThemeFor(tWindow->GetMapMode()), pLayer, pVisible))
    {
        tWindow->SaveSettings();
    }
}

bool Map::IsLayerVisible(MapLayer pLayer) const
{
    return mImpl->Instance() && LayerRules::IsVisible(mImpl->Instance()->Layers(), pLayer);
}

void Map::ToggleLayer(MapLayer pLayer)
{
    MapWindow* tWindow = mImpl->Window();
    if (tWindow && LayerRules::Toggle(mImpl->Instance()->Layers(), ThemeFor(tWindow->GetMapMode()), pLayer))
    {
        tWindow->SaveSettings();
    }
}

void Map::SetLayersVisible(std::initializer_list<MapLayer> pLayers, bool pVisible)
{
    for (MapLayer tLayer : pLayers)
    {
        SetLayerVisible(tLayer, pVisible);
    }
}

void Map::ShowOnlyLayers(std::initializer_list<MapLayer> pLayers)
{
    if (MapWindow* tWindow = mImpl->Window())
    {
        LayerRules::ShowOnly(mImpl->Instance()->Layers(), ThemeFor(tWindow->GetMapMode()), pLayers.begin(),
                             pLayers.end());
        tWindow->SaveSettings();
    }
}

void Map::ResetLayersToDefaults()
{
    if (MapWindow* tWindow = mImpl->Window())
    {
        tWindow->ResetLayers();
    }
}

LayerState Map::GetLayerState(MapLayer pLayer) const
{
    return mImpl->Instance() ? LayerRules::State(mImpl->Instance()->Layers(), pLayer) : LayerState::Off;
}

bool Map::SetLayerZoomRange(MapLayer pLayer, double pMinZoom, double pMaxZoom)
{
    MapWindow* tWindow = mImpl->Window();
    if (!tWindow)
    {
        return false;
    }
    if (!(pMinZoom <= pMaxZoom))
    {
        Log("WARNING: SetLayerZoomRange: min {} is greater than max {}", pMinZoom, pMaxZoom);
        return false;
    }
    LayerManager& tLayers = mImpl->Instance()->Layers();
    bool tApplied = false;
    if (KindOf(pLayer) == LayerKind::Group)
    {
        for (MapLayer tChild : ChildrenOf(pLayer))
        {
            if (Layer* tLayer = LayerRules::FindLayer(tLayers, tChild))
            {
                tLayer->SetZoomRange(pMinZoom, pMaxZoom);
                tApplied = true;
            }
        }
    }
    else if (Layer* tLayer = LayerRules::FindLayer(tLayers, pLayer))
    {
        const int tSub = LayerRules::SubLayerIndex(tLayers, pLayer);
        if (tSub >= 0)
        {
            tLayer->SetSubLayerZoomRange(tSub, pMinZoom, pMaxZoom);
        }
        else
        {
            tLayer->SetZoomRange(pMinZoom, pMaxZoom);
        }
        tApplied = true;
    }
    if (tApplied)
    {
        tWindow->SaveSettings();
    }
    return tApplied;
}

void Map::GetLayerZoomRange(MapLayer pLayer, double* pMinZoom, double* pMaxZoom) const
{
    double tMin = Layer::kNoMinZoom;
    double tMax = Layer::kNoMaxZoom;
    if (mImpl->Instance())
    {
        const LayerManager& tLayers = mImpl->Instance()->Layers();
        MapLayer tItem = pLayer;
        if (KindOf(pLayer) == LayerKind::Group)
        {
            const std::vector<MapLayer> tChildren = ChildrenOf(pLayer);
            tItem = tChildren.empty() ? pLayer : tChildren.front();
        }
        if (const Layer* tLayer = LayerRules::FindLayer(tLayers, tItem))
        {
            const int tSub = LayerRules::SubLayerIndex(tLayers, tItem);
            tMin = tSub >= 0 ? tLayer->SubLayerAt(tSub).mMinZoom : tLayer->MinZoom();
            tMax = tSub >= 0 ? tLayer->SubLayerAt(tSub).mMaxZoom : tLayer->MaxZoom();
        }
    }
    if (pMinZoom)
    {
        *pMinZoom = tMin;
    }
    if (pMaxZoom)
    {
        *pMaxZoom = tMax;
    }
}

void Map::ClearLayerZoomRange(MapLayer pLayer)
{
    SetLayerZoomRange(pLayer, Layer::kNoMinZoom, Layer::kNoMaxZoom);
}

void Map::ClearAllLayerZoomRanges()
{
    if (MapWindow* tWindow = mImpl->Window())
    {
        mImpl->Instance()->Layers().ResetZoomRanges();
        tWindow->SaveSettings();
    }
}

// --- Cizimler -------------------------------------------------------------------

void Map::SetDrawTool(DrawTool pTool)
{
    if (MapWindow* tWindow = mImpl->Window())
    {
        mImpl->Instance()->UserDrawings().SetMode(static_cast<Drawings::Mode>(pTool));  // yarim cizim biter
        tWindow->DrawingsChanged();
    }
}

DrawTool Map::GetDrawTool() const
{
    return mImpl->Instance() ? static_cast<DrawTool>(mImpl->Instance()->UserDrawings().GetMode()) : DrawTool::Pan;
}

bool Map::AddDrawingPoint(double pLat, double pLon)
{
    MapWindow* tWindow = mImpl->Window();
    if (!tWindow || mImpl->Instance()->UserDrawings().GetMode() == Drawings::Mode::Pan)
    {
        return false;
    }
    mImpl->Instance()->UserDrawings().AddVertex({pLat, pLon});
    tWindow->DrawingsChanged();
    return true;
}

bool Map::FinishDrawing()
{
    MapWindow* tWindow = mImpl->Window();
    if (!tWindow || !mImpl->Instance()->UserDrawings().Current())
    {
        return false;
    }
    mImpl->Instance()->UserDrawings().Finish();
    tWindow->DrawingsChanged();
    return true;
}

bool Map::UndoDrawing()
{
    MapWindow* tWindow = mImpl->Window();
    if (!tWindow)
    {
        return false;
    }
    Drawings& tDrawings = mImpl->Instance()->UserDrawings();
    if (!tDrawings.Current() && tDrawings.ShapeCount() == 0)
    {
        return false;
    }
    tDrawings.Undo();
    tWindow->DrawingsChanged();
    return true;
}

void Map::ClearDrawings()
{
    if (MapWindow* tWindow = mImpl->Window())
    {
        mImpl->Instance()->UserDrawings().Clear();
        tWindow->DrawingsChanged();
    }
}

bool Map::HasUnfinishedDrawing() const
{
    return mImpl->Instance() && mImpl->Instance()->UserDrawings().Current() != nullptr;
}

std::vector<Shape> Map::GetShapes() const
{
    std::vector<Shape> tShapes;
    if (!mImpl->Instance())
    {
        return tShapes;
    }
    for (const Drawings::Shape& tInner : mImpl->Instance()->UserDrawings().Shapes())
    {
        Shape tShape;
        tShape.mKind = static_cast<ShapeKind>(tInner.mKind);
        tShape.mId = static_cast<int>(tInner.mKind) * kShapeIdBase + tInner.mNumber;
        tShape.mLabel = Drawings::LabelOf(tInner);
        for (const ::LatLon& tPoint : tInner.mPoints)
        {
            tShape.mPoints.push_back({tPoint.mLat, tPoint.mLon});
        }
        tShapes.push_back(tShape);
    }
    return tShapes;
}

bool Map::RemoveShape(int pId)
{
    MapWindow* tWindow = mImpl->Window();
    const int tKind = pId / kShapeIdBase;
    if (!tWindow || tKind < 0 || tKind > 2 ||
        !mImpl->Instance()->UserDrawings().Remove(static_cast<Drawings::ShapeKind>(tKind), pId % kShapeIdBase))
    {
        Log("WARNING: RemoveShape: no shape with id {}", pId);
        return false;
    }
    tWindow->DrawingsChanged();
    return true;
}

std::string Map::ExportDrawings() const
{
    return mImpl->Instance() ? mImpl->Instance()->UserDrawings().Serialize() : std::string();
}

bool Map::ImportDrawings(const std::string& pText)
{
    MapWindow* tWindow = mImpl->Window();
    if (!tWindow)
    {
        return false;
    }
    const int tCount = mImpl->Instance()->UserDrawings().Deserialize(pText);
    tWindow->DrawingsChanged();
    Log("drawings imported: {} shapes", tCount);
    return tCount > 0 || pText.find_first_not_of(" \t\r\n") == std::string::npos;
}

// --- Ucak, iz, rota -------------------------------------------------------------

void Map::SetAircraftSource(DataSource pSource)
{
    if (MapInstance* tInstance = mImpl->Instance())
    {
        tInstance->Aircraft().SetFromHost(pSource == DataSource::Host);
        tInstance->MarkChanged();
    }
}

DataSource Map::GetAircraftSource() const
{
    return (mImpl->Instance() && mImpl->Instance()->Aircraft().IsFromHost()) ? DataSource::Host : DataSource::XPlane;
}

void Map::SetAircraft(const AircraftState& pState)
{
    if (!mImpl->Instance())
    {
        return;
    }
    ::AircraftState tInner;
    tInner.mValid = pState.mValid;
    tInner.mPosition = {pState.mPosition.mLat, pState.mPosition.mLon};
    tInner.mHeadingDeg = pState.mHeadingDeg;
    tInner.mGroundSpeedKt = pState.mGroundSpeedKt;
    tInner.mAltitudeFt = pState.mAltitudeFt;
    mImpl->Instance()->Aircraft().SetHostState(tInner);
}

AircraftState Map::GetAircraft() const
{
    AircraftState tState;
    if (mImpl->Instance())
    {
        const ::AircraftState& tInner = mImpl->Instance()->Aircraft().State();
        tState.mValid = tInner.mValid;
        tState.mPosition = {tInner.mPosition.mLat, tInner.mPosition.mLon};
        tState.mHeadingDeg = tInner.mHeadingDeg;
        tState.mGroundSpeedKt = tInner.mGroundSpeedKt;
        tState.mAltitudeFt = tInner.mAltitudeFt;
    }
    return tState;
}

void Map::ClearTrack()
{
    if (mImpl->Instance() && mImpl->Instance()->Track())
    {
        mImpl->Instance()->Track()->Clear();
        Log("flight track cleared (map {})", mImpl->Instance()->Id());
    }
}

void Map::SetRouteSource(DataSource pSource)
{
    if (mImpl->Instance() && mImpl->Instance()->Fms())
    {
        mImpl->Instance()->Fms()->SetFromHost(pSource == DataSource::Host);
        mImpl->Instance()->MarkChanged();
    }
}

DataSource Map::GetRouteSource() const
{
    return (mImpl->Instance() && mImpl->Instance()->Fms() && mImpl->Instance()->Fms()->IsFromHost())
               ? DataSource::Host
               : DataSource::XPlane;
}

void Map::SetRoute(const std::vector<RouteWaypoint>& pWaypoints, int pActiveIndex)
{
    if (!mImpl->Instance() || !mImpl->Instance()->Fms())
    {
        return;
    }
    if (!mImpl->Instance()->Fms()->IsFromHost())
    {
        Log("WARNING: SetRoute ignored: route source is X-Plane (call SetRouteSource(DataSource::Host))");
        return;
    }
    std::vector<FmsLayer::Waypoint> tInner;
    for (const RouteWaypoint& tPoint : pWaypoints)
    {
        tInner.push_back({{tPoint.mPosition.mLat, tPoint.mPosition.mLon}, tPoint.mIdent, tPoint.mAltitudeFt});
    }
    mImpl->Instance()->Fms()->SetHostRoute(tInner, pActiveIndex);
}

// --- Sorgu ------------------------------------------------------------------------

std::vector<MapObject> Map::QueryAt(double pLat, double pLon) const
{
    int tX = 0;
    int tY = 0;
    if (!GeoToScreen(pLat, pLon, &tX, &tY))
    {
        return {};
    }
    return QueryAtScreen(tX, tY);
}

std::vector<MapObject> Map::QueryAtScreen(int pX, int pY) const
{
    std::vector<MapObject> tObjects;
    MapWindow* tWindow = mImpl->Window();
    if (!tWindow || !tWindow->IsCreated())
    {
        return tObjects;  // sorgu son cizilen kareye gore: pencere acik olmali
    }
    for (const auto& [tId, tText] : tWindow->DescribeAt(pX, pY))
    {
        MapObject tObject;
        tObject.mLayer = LayerFromKey(tId).value_or(MapLayer::Count);
        tObject.mText = tText;
        tObjects.push_back(tObject);
    }
    return tObjects;
}

// --- Cizim secenekleri --------------------------------------------------------------

void Map::SetImageCache(bool pEnabled)
{
    if (MapWindow* tWindow = mImpl->Window())
    {
        tWindow->SetImageCacheEnabled(pEnabled);
    }
}

bool Map::IsImageCacheEnabled() const
{
    MapWindow* tWindow = mImpl->Window();
    return tWindow && tWindow->IsImageCacheEnabled();
}

void Map::SetMaxRenderFps(int pFps)
{
    if (MapWindow* tWindow = mImpl->Window())
    {
        tWindow->SetMaxRenderFps(pFps);
    }
}

int Map::GetMaxRenderFps() const
{
    MapWindow* tWindow = mImpl->Window();
    return tWindow ? tWindow->MaxRenderFps() : 0;
}

void Map::SetXPlaneFont(bool pXPlaneFont)
{
    if (MapWindow* tWindow = mImpl->Window())
    {
        tWindow->SetXPlaneFont(pXPlaneFont);
    }
}

bool Map::IsXPlaneFont() const
{
    MapWindow* tWindow = mImpl->Window();
    return tWindow && tWindow->UsesXPlaneFont();
}

// --- Ayarlar ----------------------------------------------------------------------

std::string Map::ExportSettings() const
{
    return mImpl->Instance() ? mImpl->Instance()->ExportSettings() : std::string();
}

bool Map::ImportSettings(const std::string& pText)
{
    if (!mImpl->Instance())
    {
        return false;
    }
    mImpl->Instance()->ImportSettings(pText);
    return true;
}

TerrainSettings Map::GetTerrainSettings() const
{
    TerrainSettings tOut;
    if (!mImpl->Instance())
    {
        return tOut;
    }
    Settings tIni;
    mImpl->Instance()->Palette().Save(tIni);
    tOut.mUseXPlaneColors = tIni.GetString("palette", "source", "xplane") != "ini";
    for (int tClass = 0; tClass < kTerrainClassCount; ++tClass)
    {
        const std::string tKey = TerrainClassKey(static_cast<TerrainClass>(tClass));
        if (tIni.Has("colors", tKey))
        {
            std::istringstream tStream(tIni.GetString("colors", tKey, ""));
            tOut.mClassColors.emplace_back(tKey, ParseRgb(tStream));
        }
    }
    tOut.mShadingStrength = tIni.GetDouble("shading", "strength", 0.0);
    tOut.mBlendMeters = tIni.GetDouble("blend", "meters", 0.0);
    tOut.mBlendMaxPixels = tIni.GetDouble("blend", "max_pixels", 0.0);
    tOut.mWarpMeters = tIni.GetDouble("warp", "meters", 0.0);
    tOut.mWarpWavelength = tIni.GetDouble("warp", "wavelength", 0.0);
    tOut.mCloseupClassMix = tIni.GetDouble("closeup", "class_mix", 0.0);
    tOut.mReliefMultiDirectional = tIni.GetBool("relief", "multi_directional", true);
    tOut.mReliefExaggeration = tIni.GetDouble("relief", "exaggeration", 0.0);
    tOut.mReliefCoarseBoost = tIni.GetDouble("relief", "coarse_boost", 0.0);
    tOut.mReliefSlopeTone = tIni.GetDouble("relief", "slope_tone", 0.0);
    tOut.mReliefFlatSlope = tIni.GetDouble("relief", "flat_slope", 0.0);
    tOut.mCoastline = tIni.GetBool("water", "coastline", true);
    tOut.mCoastlineStrength = tIni.GetDouble("water", "coastline_strength", 0.0);
    tOut.mWaterBandMeters = tIni.GetDouble("water", "band_meters", 0.0);
    tOut.mWaterBandMaxPixels = tIni.GetDouble("water", "band_max_pixels", 0.0);
    tOut.mWaterBandStrength = tIni.GetDouble("water", "band_strength", 0.0);
    tOut.mDepthTone = tIni.GetDouble("water", "depth_tone", 0.0);
    tOut.mDepthFullMeters = tIni.GetDouble("water", "depth_full_meters", 0.0);
    tOut.mVfrBands = tIni.GetBool("vfr", "bands", true);
    tOut.mVfrContourLines = tIni.GetBool("vfr", "contour_lines", false);
    tOut.mVfrContourStrength = tIni.GetDouble("vfr", "contour_strength", 0.0);
    for (int tBand = 1; tBand <= kVfrBandCount; ++tBand)
    {
        std::istringstream tStream(tIni.GetString("vfr", std::format("band_{}", tBand), ""));
        double tFeet = 0.0;
        tStream >> tFeet;
        tOut.mVfrBandsFeet.emplace_back(tFeet, ParseRgb(tStream));
    }
    return tOut;
}

void Map::SetTerrainSettings(const TerrainSettings& pSettings)
{
    if (!mImpl->Instance())
    {
        return;
    }
    Settings tIni;
    mImpl->Instance()->Palette().Save(tIni);  // [palette] version ve bilinmeyen anahtarlar korunur
    tIni.SetString("palette", "source", pSettings.mUseXPlaneColors ? "xplane" : "ini");
    for (const auto& [tKey, tColor] : pSettings.mClassColors)
    {
        tIni.SetString("colors", tKey, std::format("{} {} {}", tColor.mR, tColor.mG, tColor.mB));
    }
    tIni.SetDouble("shading", "strength", pSettings.mShadingStrength);
    tIni.SetDouble("blend", "meters", pSettings.mBlendMeters);
    tIni.SetDouble("blend", "max_pixels", pSettings.mBlendMaxPixels);
    tIni.SetDouble("warp", "meters", pSettings.mWarpMeters);
    tIni.SetDouble("warp", "wavelength", pSettings.mWarpWavelength);
    tIni.SetDouble("closeup", "class_mix", pSettings.mCloseupClassMix);
    tIni.SetBool("relief", "multi_directional", pSettings.mReliefMultiDirectional);
    tIni.SetDouble("relief", "exaggeration", pSettings.mReliefExaggeration);
    tIni.SetDouble("relief", "coarse_boost", pSettings.mReliefCoarseBoost);
    tIni.SetDouble("relief", "slope_tone", pSettings.mReliefSlopeTone);
    tIni.SetDouble("relief", "flat_slope", pSettings.mReliefFlatSlope);
    tIni.SetBool("water", "coastline", pSettings.mCoastline);
    tIni.SetDouble("water", "coastline_strength", pSettings.mCoastlineStrength);
    tIni.SetDouble("water", "band_meters", pSettings.mWaterBandMeters);
    tIni.SetDouble("water", "band_max_pixels", pSettings.mWaterBandMaxPixels);
    tIni.SetDouble("water", "band_strength", pSettings.mWaterBandStrength);
    tIni.SetDouble("water", "depth_tone", pSettings.mDepthTone);
    tIni.SetDouble("water", "depth_full_meters", pSettings.mDepthFullMeters);
    tIni.SetBool("vfr", "bands", pSettings.mVfrBands);
    tIni.SetBool("vfr", "contour_lines", pSettings.mVfrContourLines);
    tIni.SetDouble("vfr", "contour_strength", pSettings.mVfrContourStrength);
    for (size_t tBand = 0; tBand < pSettings.mVfrBandsFeet.size() && tBand < kVfrBandCount; ++tBand)
    {
        const auto& [tFeet, tColor] = pSettings.mVfrBandsFeet[tBand];
        tIni.SetString("vfr", std::format("band_{}", tBand + 1),
                       std::format("{:.0f} {} {} {}", tFeet, tColor.mR, tColor.mG, tColor.mB));
    }
    mImpl->Instance()->ImportTerrainSettings(tIni.ToText({}));
}

std::string Map::ExportTerrainSettings() const
{
    return mImpl->Instance() ? mImpl->Instance()->ExportTerrainSettings() : std::string();
}

bool Map::ImportTerrainSettings(const std::string& pText)
{
    if (!mImpl->Instance())
    {
        return false;
    }
    mImpl->Instance()->ImportTerrainSettings(pText);
    return true;
}

uint64_t Map::GetSettingsRevision() const
{
    MapWindow* tWindow = mImpl->Window();
    return tWindow ? tWindow->SettingsRevision() : 0;
}

uint64_t Map::GetDrawingsRevision() const
{
    MapWindow* tWindow = mImpl->Window();
    return tWindow ? tWindow->DrawingsRevision() : 0;
}

// --- Durum -------------------------------------------------------------------------

std::string Map::GetStatusText() const
{
    return mImpl->Instance() ? mImpl->Instance()->StatusText() : std::string();
}

int Map::GetTerrainPendingImages() const
{
    return mImpl->Instance() ? mImpl->Instance()->PendingTerrainImages() : 0;
}

// --- On onbellek ve teshis --------------------------------------------------------

bool Map::PrecacheVisibleArea()
{
    return PrecacheArea(GetVisibleBounds());
}

bool Map::PrecacheArea(const GeoBounds& pArea)
{
    if (!mImpl->Instance())
    {
        return false;
    }
    ::GeoBounds tInner;
    tInner.mSouth = pArea.mSouth;
    tInner.mWest = pArea.mWest;
    tInner.mNorth = pArea.mNorth;
    tInner.mEast = pArea.mEast;
    return mImpl->Instance()->Data().StartPrecache(tInner);
}

void Map::CancelPrecache()
{
    if (mImpl->Instance())
    {
        mImpl->Instance()->Data().CancelPrecache();
    }
}

void Map::SetDiagnostics(bool pOn)
{
    if (MapWindow* tWindow = mImpl->Window())
    {
        tWindow->SetDiagnostics(pOn);
        Log("diagnostics mode: {} (DIAG lines in Log.txt)", pOn ? "on" : "off");
    }
}

bool Map::IsDiagnosticsOn() const
{
    MapWindow* tWindow = mImpl->Window();
    return tWindow && tWindow->IsDiagnosticsOn();
}

}  // namespace prime
