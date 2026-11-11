// PrimeMap.cpp
// Genel API (PrimeMap.h) -> ic siniflar koprusu:
//
//   prime::MapLibrary (Init / Shutdown / GetStatus)  ->  MapData (veri, yukleme, flight loop)
//   prime::MapLibrary::CreateMap / prime::Map        ->  MapWindow (pencere, cizim, fare, UI)
//   katman fonksiyonlari                             ->  engine/LayerTable (panelle ayni kurallar)
//
// Kutuphanenin durumu (MapData + acik harita pencereleri + host sayisi) MapLibrary'nin
// Impl'inde yasar: prime_map.dll basina TEK ornek (MapLibrary::Get()). Ayni X-Plane
// surecindeki tum hostlar ayni DLL'i kullandigi icin durumu paylasir: Init bir kez
// yapilir, coklu host referans sayaciyla yonetilir. Butun fonksiyonlar X-Plane ana
// thread'inden cagrilmalidir.
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

}  // namespace

namespace prime
{

// ============================================================================
// MapLibrary: kutuphanenin kendisi (surecte tek ornek)
// ============================================================================

struct MapLibrary::Impl
{
    std::unique_ptr<MapData> mData;        // tum harita verisi ve arka plan isleri
    std::vector<MapWindow*> mOpenWindows;  // acik harita pencereleri (coklu harita / coklu host)
    int mHostCount = 0;                    // Init/Shutdown dengesi (coklu host)
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
        // TEK'tir; ilk host'un yapilandirmasi (log oneki, onbellek klasoru)
        // gecerli kalir. Sonraki hostlarin Init'i ek is yapmaz, true doner.
        ++mImpl->mHostCount;
        Log("Init: library already running (hosts: {}); first host's config is in effect",
            mImpl->mHostCount);
        return true;
    }
    SetLogPrefix(pConfig.mLogPrefix);
    SetLogCallback(pConfig.mLogCallback);
    // Dosya yollari isletim sisteminin bicimiyle (C:\...); Windows'ta host'un yollarini degistirmez
    XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);

    MapDataConfig tConfig;
    tConfig.mCacheDirectory = pConfig.mCacheDirectory.empty() ? HostPluginDirectory() + "cache/"
                                                              : pConfig.mCacheDirectory;
    if (tConfig.mCacheDirectory.back() != '/' && tConfig.mCacheDirectory.back() != '\\')
    {
        tConfig.mCacheDirectory += '/';
    }
    tConfig.mTerrainCacheMb = std::max(0, pConfig.mTerrainCacheMb);
    tConfig.mPrefetchTiles = std::max(0, pConfig.mPrefetchTiles);
    mImpl->mData = std::make_unique<MapData>(tConfig);
    mImpl->mData->Start();
    mImpl->mHostCount = 1;
    Log("Prime Map API {} ready (cache: {})", VersionString(), tConfig.mCacheDirectory);
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
    for (MapWindow* tWindow : mImpl->mOpenWindows)
    {
        // Host haritayi kapatmayi unuttu: pencereleri burada kapat (Map nesnesi bos kalir)
        Log("WARNING: MapLibrary::Shutdown with open map(s); closing the map window(s)");
        tWindow->Destroy();
    }
    mImpl->mOpenWindows.clear();
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
    MapWindow* tDiag = nullptr;
    for (MapWindow* tWindow : mImpl->mOpenWindows)
    {
        if (tWindow->IsDiagnosticsOn())
        {
            tDiag = tWindow;
            break;
        }
    }
    if (tDiag)
    {
        tStatus.mPerformanceText = tDiag->PerformanceText();
    }
    return tStatus;
}

// ============================================================================
// Map
// ============================================================================

struct Map::Impl
{
    MapData* mData = nullptr;                 // kutuphanenin verisi (MapLibrary::Impl'de yasar)
    std::unique_ptr<MapWindow> mWindow;
    double mZoomStep = 0.5;   // bir tekerlek tiki
    double mPanStep = 0.25;   // gorunen alanin orani

    // Shutdown haritayi kapattiysa nullptr (MapData yok edildiyse guvenli sekilde bos doner)
    MapData* Data() const
    {
        return (mData && mData->IsRunning()) ? mData : nullptr;
    }

    // Shutdown haritayi once kapattiysa nullptr
    MapWindow* Window() const
    {
        return (Data() && mWindow && mWindow->IsCreated()) ? mWindow.get() : nullptr;
    }
};

Map::Map()
    : mImpl(std::make_unique<Impl>())
{
}

std::unique_ptr<Map> MapLibrary::CreateMap(const OpenConfig& pConfig)
{
    if (!mImpl->mData)
    {
        Log("WARNING: CreateMap before MapLibrary::Init");
        return nullptr;
    }
    std::unique_ptr<Map> tMap(new Map());
    tMap->mImpl->mData = mImpl->mData.get();
    tMap->mImpl->mWindow = std::make_unique<MapWindow>(mImpl->mData->Layers(), mImpl->mData->Aircraft(),
                                                       mImpl->mData->UserDrawings(),
                                                       mImpl->mData->Airports());
    MapWindowOptions tOptions;
    tOptions.mGeometry.mLeft = pConfig.mGeometry.mLeft;
    tOptions.mGeometry.mTop = pConfig.mGeometry.mTop;
    tOptions.mGeometry.mRight = pConfig.mGeometry.mLeft + pConfig.mGeometry.mWidth;
    tOptions.mGeometry.mBottom = pConfig.mGeometry.mTop - pConfig.mGeometry.mHeight;
    tOptions.mEmbedded = pConfig.mStyle == WindowStyle::Embedded;
    tOptions.mVisible = pConfig.mVisible;
    tOptions.mUiVisible = pConfig.mUiVisible;
    tOptions.mMouseInput = pConfig.mMouseInput;
    tOptions.mMode = static_cast<::MapMode>(pConfig.mMode);
    tOptions.mImageCache = pConfig.mImageCache;
    tOptions.mMaxRenderFps = pConfig.mMaxRenderFps;
    tOptions.mXPlaneFont = pConfig.mXPlaneFont;
    tMap->mImpl->mWindow->Create(tOptions);
    mImpl->mOpenWindows.push_back(tMap->mImpl->mWindow.get());
    mImpl->mData->AddWindow(tMap->mImpl->mWindow.get());
    return tMap;
}

std::unique_ptr<Map> Map::Open(const OpenConfig& pConfig)
{
    return MapLibrary::Get().CreateMap(pConfig);
}

Map::~Map()
{
    if (MapWindow* tWindow = mImpl->Window())
    {
        mImpl->Data()->RemoveWindow(tWindow);
        tWindow->Destroy();
    }
    // Pencereyi kutuphanenin acik pencere listesinden dusur (Map, MapLibrary'nin dostu)
    auto& tOpenWindows = MapLibrary::Get().mImpl->mOpenWindows;
    tOpenWindows.erase(std::remove(tOpenWindows.begin(), tOpenWindows.end(), mImpl->mWindow.get()),
                       tOpenWindows.end());
}

// --- Pencere ---------------------------------------------------------------------

void Map::SetGeometry(const Geometry& pGeometry)
{
    if (MapWindow* tWindow = mImpl->Window())
    {
        RectI tRect;
        tRect.mLeft = pGeometry.mLeft;
        tRect.mTop = pGeometry.mTop;
        tRect.mRight = pGeometry.mLeft + pGeometry.mWidth;
        tRect.mBottom = pGeometry.mTop - pGeometry.mHeight;
        tWindow->SetGeometry(tRect);
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

void Map::ZoomIn()
{
    SetZoom(GetZoom() + mImpl->mZoomStep);
}

void Map::ZoomOut()
{
    SetZoom(GetZoom() - mImpl->mZoomStep);
}

void Map::SetZoomStep(double pStep)
{
    if (pStep > 0.0 && pStep <= 5.0)
    {
        mImpl->mZoomStep = pStep;
    }
    else
    {
        Log("WARNING: SetZoomStep: {} out of range (0, 5]", pStep);
    }
}

void Map::PanLeft()
{
    // Sola bakmak = harita icerigi saga kayar
    PanBy(GetGeometry().mWidth * mImpl->mPanStep, 0.0);
}

void Map::PanRight()
{
    PanBy(-GetGeometry().mWidth * mImpl->mPanStep, 0.0);
}

void Map::PanUp()
{
    // Kuzeye bakmak = icerik asagi kayar (y yukari artar)
    PanBy(0.0, -GetGeometry().mHeight * mImpl->mPanStep);
}

void Map::PanDown()
{
    PanBy(0.0, GetGeometry().mHeight * mImpl->mPanStep);
}

void Map::SetPanStep(double pFractionOfView)
{
    if (pFractionOfView > 0.0 && pFractionOfView <= 1.0)
    {
        mImpl->mPanStep = pFractionOfView;
    }
    else
    {
        Log("WARNING: SetPanStep: {} out of range (0, 1]", pFractionOfView);
    }
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
    const std::string tWanted = Upper(pIdent);
    for (int tIndex : ::FindAirports(mImpl->Data()->Airports(), pIdent, 8))
    {
        if (Upper(mImpl->Data()->Airports().At(tIndex).mId) == tWanted)
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
    if (!mImpl->Data())
    {
        return tMatches;
    }
    for (int tIndex : ::FindAirports(mImpl->Data()->Airports(), pQuery, std::max(0, pMaxResults)))
    {
        const AirportGeometry& tAirport = mImpl->Data()->Airports().At(tIndex);
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
    if (tWindow && LayerRules::SetVisible(mImpl->Data()->Layers(), ThemeFor(tWindow->GetMapMode()), pLayer, pVisible))
    {
        tWindow->SaveSettings();
    }
}

bool Map::IsLayerVisible(MapLayer pLayer) const
{
    return mImpl->Data() && LayerRules::IsVisible(mImpl->Data()->Layers(), pLayer);
}

void Map::ToggleLayer(MapLayer pLayer)
{
    MapWindow* tWindow = mImpl->Window();
    if (tWindow && LayerRules::Toggle(mImpl->Data()->Layers(), ThemeFor(tWindow->GetMapMode()), pLayer))
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
        LayerRules::ShowOnly(mImpl->Data()->Layers(), ThemeFor(tWindow->GetMapMode()), pLayers.begin(), pLayers.end());
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
    return mImpl->Data() ? LayerRules::State(mImpl->Data()->Layers(), pLayer) : LayerState::Off;
}

// --- Cizimler -------------------------------------------------------------------

void Map::SetDrawTool(DrawTool pTool)
{
    if (MapWindow* tWindow = mImpl->Window())
    {
        mImpl->Data()->UserDrawings().SetMode(static_cast<Drawings::Mode>(pTool));  // yarim cizim biter
        tWindow->DrawingsChanged();
    }
}

DrawTool Map::GetDrawTool() const
{
    return mImpl->Data() ? static_cast<DrawTool>(mImpl->Data()->UserDrawings().GetMode()) : DrawTool::Pan;
}

bool Map::AddDrawingPoint(double pLat, double pLon)
{
    MapWindow* tWindow = mImpl->Window();
    if (!tWindow || mImpl->Data()->UserDrawings().GetMode() == Drawings::Mode::Pan)
    {
        return false;
    }
    mImpl->Data()->UserDrawings().AddVertex({pLat, pLon});
    tWindow->DrawingsChanged();
    return true;
}

bool Map::FinishDrawing()
{
    MapWindow* tWindow = mImpl->Window();
    if (!tWindow || !mImpl->Data()->UserDrawings().Current())
    {
        return false;
    }
    mImpl->Data()->UserDrawings().Finish();
    tWindow->DrawingsChanged();
    return true;
}

bool Map::UndoDrawing()
{
    MapWindow* tWindow = mImpl->Window();
    Drawings* tDrawings = mImpl->Data() ? &mImpl->Data()->UserDrawings() : nullptr;
    if (!tWindow || (!tDrawings->Current() && tDrawings->ShapeCount() == 0))
    {
        return false;
    }
    tDrawings->Undo();
    tWindow->DrawingsChanged();
    return true;
}

void Map::ClearDrawings()
{
    if (MapWindow* tWindow = mImpl->Window())
    {
        mImpl->Data()->UserDrawings().Clear();
        tWindow->DrawingsChanged();
    }
}

bool Map::HasUnfinishedDrawing() const
{
    return mImpl->Data() && mImpl->Data()->UserDrawings().Current() != nullptr;
}

std::vector<Shape> Map::GetShapes() const
{
    std::vector<Shape> tShapes;
    if (!mImpl->Data())
    {
        return tShapes;
    }
    for (const Drawings::Shape& tInner : mImpl->Data()->UserDrawings().Shapes())
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
        !mImpl->Data()->UserDrawings().Remove(static_cast<Drawings::ShapeKind>(tKind), pId % kShapeIdBase))
    {
        Log("WARNING: RemoveShape: no shape with id {}", pId);
        return false;
    }
    tWindow->DrawingsChanged();
    return true;
}

std::string Map::ExportDrawings() const
{
    return mImpl->Data() ? mImpl->Data()->UserDrawings().Serialize() : std::string();
}

bool Map::ImportDrawings(const std::string& pText)
{
    MapWindow* tWindow = mImpl->Window();
    if (!tWindow)
    {
        return false;
    }
    const int tCount = mImpl->Data()->UserDrawings().Deserialize(pText);
    tWindow->DrawingsChanged();
    Log("drawings imported: {} shapes", tCount);
    return tCount > 0 || pText.find_first_not_of(" \t\r\n") == std::string::npos;
}

// --- Ucak, iz, rota -------------------------------------------------------------

void Map::SetAircraftSource(DataSource pSource)
{
    if (mImpl->Data())
    {
        mImpl->Data()->Aircraft().SetFromHost(pSource == DataSource::Host);
    }
}

DataSource Map::GetAircraftSource() const
{
    return (mImpl->Data() && mImpl->Data()->Aircraft().IsFromHost()) ? DataSource::Host : DataSource::XPlane;
}

void Map::SetAircraft(const AircraftState& pState)
{
    if (!mImpl->Data())
    {
        return;
    }
    ::AircraftState tInner;
    tInner.mValid = pState.mValid;
    tInner.mPosition = {pState.mPosition.mLat, pState.mPosition.mLon};
    tInner.mHeadingDeg = pState.mHeadingDeg;
    tInner.mGroundSpeedKt = pState.mGroundSpeedKt;
    tInner.mAltitudeFt = pState.mAltitudeFt;
    mImpl->Data()->Aircraft().SetHostState(tInner);
}

AircraftState Map::GetAircraft() const
{
    AircraftState tState;
    if (mImpl->Data())
    {
        const ::AircraftState& tInner = mImpl->Data()->Aircraft().State();
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
    if (mImpl->Data() && mImpl->Data()->Track())
    {
        mImpl->Data()->Track()->Clear();
        Log("flight track cleared");
    }
}

void Map::SetRouteSource(DataSource pSource)
{
    if (mImpl->Data() && mImpl->Data()->Fms())
    {
        mImpl->Data()->Fms()->SetFromHost(pSource == DataSource::Host);
    }
}

DataSource Map::GetRouteSource() const
{
    return (mImpl->Data() && mImpl->Data()->Fms() && mImpl->Data()->Fms()->IsFromHost()) ? DataSource::Host : DataSource::XPlane;
}

void Map::SetRoute(const std::vector<RouteWaypoint>& pWaypoints, int pActiveIndex)
{
    if (!mImpl->Data() || !mImpl->Data()->Fms())
    {
        return;
    }
    if (!mImpl->Data()->Fms()->IsFromHost())
    {
        Log("WARNING: SetRoute ignored: route source is X-Plane (call SetRouteSource(DataSource::Host))");
        return;
    }
    std::vector<FmsLayer::Waypoint> tInner;
    for (const RouteWaypoint& tPoint : pWaypoints)
    {
        tInner.push_back({{tPoint.mPosition.mLat, tPoint.mPosition.mLon}, tPoint.mIdent, tPoint.mAltitudeFt});
    }
    mImpl->Data()->Fms()->SetHostRoute(tInner, pActiveIndex);
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
    if (!tWindow)
    {
        return tObjects;
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

// --- Ayarlar ----------------------------------------------------------------------

std::string Map::ExportSettings() const
{
    MapWindow* tWindow = mImpl->Window();
    return tWindow ? tWindow->ExportSettings() : std::string();
}

bool Map::ImportSettings(const std::string& pText)
{
    MapWindow* tWindow = mImpl->Window();
    if (!tWindow)
    {
        return false;
    }
    tWindow->ImportSettings(pText);
    return true;
}

TerrainSettings Map::GetTerrainSettings() const
{
    TerrainSettings tOut;
    if (!mImpl->Data())
    {
        return tOut;
    }
    Settings tIni;
    mImpl->Data()->Palette().Save(tIni);
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
    if (!mImpl->Data())
    {
        return;
    }
    Settings tIni;
    mImpl->Data()->Palette().Save(tIni);  // [palette] version ve bilinmeyen anahtarlar korunur
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
    mImpl->Data()->ImportTerrainSettings(tIni.ToText({}));
}

std::string Map::ExportTerrainSettings() const
{
    return mImpl->Data() ? mImpl->Data()->ExportTerrainSettings() : std::string();
}

bool Map::ImportTerrainSettings(const std::string& pText)
{
    if (!mImpl->Data())
    {
        return false;
    }
    mImpl->Data()->ImportTerrainSettings(pText);
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

// --- On onbellek ve teshis --------------------------------------------------------

bool Map::PrecacheVisibleArea()
{
    return PrecacheArea(GetVisibleBounds());
}

bool Map::PrecacheArea(const GeoBounds& pArea)
{
    if (!mImpl->Data())
    {
        return false;
    }
    ::GeoBounds tInner;
    tInner.mSouth = pArea.mSouth;
    tInner.mWest = pArea.mWest;
    tInner.mNorth = pArea.mNorth;
    tInner.mEast = pArea.mEast;
    return mImpl->Data()->StartPrecache(tInner);
}

void Map::CancelPrecache()
{
    if (mImpl->Data())
    {
        mImpl->Data()->CancelPrecache();
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
