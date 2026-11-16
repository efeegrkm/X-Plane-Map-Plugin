// prime::XMap uygulamasi: API yuzeyi ve motor (pencere, katmanlar, dosyalar) ayni sinifta.
// Nesneyi yalnizca XMapManager olusturur/yok eder; shutdown motor parcalarini birakir,
// nesne "bos" olarak yasar (metotlar no-op).
#include "api/XMap.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
#include <system_error>

#include "XPLMDisplay.h"

#include "Log.h"
#include "XMapTheme.h"
#include "XMapView.h"
#include "Settings.h"
#include "XMapApiUtil.h"
#include "api/XMapManager.h"
#include "data/AirportGeometry.h"
#include "data/AirportSearch.h"
#include "engine/LayerTable.h"
#include "engine/XMapData.h"
#include "layers/AircraftLayer.h"
#include "layers/AirportLayer.h"
#include "layers/AirportObjectsLayer.h"
#include "layers/AirportSurfaceLayer.h"
#include "layers/AirwayLayer.h"
#include "layers/DrawingLayer.h"
#include "layers/FmsLayer.h"
#include "layers/GridLayer.h"
#include "layers/NavaidLayer.h"
#include "layers/RoadLayer.h"
#include "layers/RunwayLayer.h"
#include "layers/TaxiRouteLayer.h"
#include "layers/TerrainLayer.h"
#include "layers/TrackLayer.h"

namespace
{

const char* kSettingsFileName = "map_settings.json";
const char* kDrawingsFileName = "map_drawings.txt";
const char* kTerrainFileName = "terrain_colors.json";
const char* kWindowSection = "window";
const char* kSourcesSection = "sources";
const char* kLayerColorsSection = "layer_colors";
const int kMinStandaloneWidth = 400;   // Standalone pencerenin kullanilabilir en kucuk boyutu (boxel)
const int kMinStandaloneHeight = 260;
const int kShapeIdBase = 1000000;      // Shape kimligi: tur * kShapeIdBase + etiket numarasi

RectI toRect(const prime::Geometry& pGeometry)
{
    RectI tRect;
    tRect.mLeft = pGeometry.mLeft;
    tRect.mTop = pGeometry.mTop;
    tRect.mRight = pGeometry.mLeft + pGeometry.mWidth;
    tRect.mBottom = pGeometry.mTop + pGeometry.mHeight;
    return tRect;
}

bool readTextFile(const std::string& pPath, std::string* pText)
{
    std::ifstream tFile(pPath);
    if (!tFile)
    {
        return false;
    }
    std::ostringstream tText;
    tText << tFile.rdbuf();
    *pText = tText.str();
    return true;
}

bool writeTextFile(const std::string& pPath, const std::string& pText)
{
    // Once gecici dosyaya, sonra yeniden adlandir: yarim yazilmis dosya kalmasin
    const std::string tTemporary = pPath + ".tmp";
    {
        std::ofstream tFile(tTemporary, std::ios::trunc);
        if (!tFile)
        {
            return false;
        }
        tFile << pText;
        if (!tFile)
        {
            return false;
        }
    }
    std::error_code tError;
    std::filesystem::rename(tTemporary, pPath, tError);
    return !tError;
}

void screenBounds(int* pLeft, int* pTop, int* pRight, int* pBottom)
{
    XPLMGetScreenBoundsGlobal(pLeft, pTop, pRight, pBottom);
}

// Ana X-Plane penceresinin ortasinda, ekranin %70'i (en fazla 1000x700)
RectI defaultGeometry()
{
    int tLeft = 0;
    int tTop = 0;
    int tRight = 0;
    int tBottom = 0;
    screenBounds(&tLeft, &tTop, &tRight, &tBottom);
    const int tWidth = std::clamp((tRight - tLeft) * 7 / 10, kMinStandaloneWidth, 1000);
    const int tHeight = std::clamp((tTop - tBottom) * 7 / 10, kMinStandaloneHeight, 700);
    RectI tGeometry;
    tGeometry.mLeft = (tLeft + tRight) / 2 - tWidth / 2;
    tGeometry.mTop = (tTop + tBottom) / 2 + tHeight / 2;
    tGeometry.mRight = tGeometry.mLeft + tWidth;
    tGeometry.mBottom = tGeometry.mTop - tHeight;
    return tGeometry;
}

// Standalone pencere: boyut yeterli ve baslik cubugu ekranda mi (monitor sokulmus /
// cozunurluk degismis olabilir)
bool isStandaloneGeometryUsable(const RectI& pGeometry)
{
    if (pGeometry.width() < kMinStandaloneWidth || pGeometry.height() < kMinStandaloneHeight)
    {
        return false;
    }
    int tLeft = 0;
    int tTop = 0;
    int tRight = 0;
    int tBottom = 0;
    screenBounds(&tLeft, &tTop, &tRight, &tBottom);
    const int tTitleBar = 30;
    const bool tTitleVisible = pGeometry.mTop + tTitleBar <= tTop && pGeometry.mTop >= tBottom + 20;
    const bool tHorizontalVisible = pGeometry.mRight >= tLeft + 100 && pGeometry.mLeft <= tRight - 100;
    return tTitleVisible && tHorizontalVisible;
}

}  // namespace

namespace prime
{

XMap::XMap(XMapData& pData, int pId)
    : mData(pData),
      mId(pId)
{
    mDataLoadedFlag = &mIsDataLoaded;
    mAircraft = std::make_unique<AircraftData>();
    mAircraft->init();
    mDrawings = std::make_unique<Drawings>();
    mPalette = std::make_unique<TerrainPalette>();
    mPaletteId = mData.registerPalette(mPalette.get());  // varsayilan zemin ayarlari (cogu zaman palet 0)
    mLayers = std::make_unique<LayerManager>();
    createLayers();
    // XMapWindow, katmanlar eklenmeden kuruldu: modun varsayilan katman durumunu simdi uygula
    mLayers->resetToDefaults(themeFor(mWindow->getMapMode()));
    mData.addInstance(this);
}

XMap::~XMap()
{
    if (mManager)
    {
        // yonetici listesinden dus; harita kapanir (motor parcalari ayarlari yazar, pencereyi kapatir)
        auto& tXMaps = mManager->mXMaps;
        tXMaps.erase(std::remove(tXMaps.begin(), tXMaps.end(), this), tXMaps.end());
    }
    if (mLayers)
    {
        close();
        mData.removeInstance(this);
    }
}

// shutdown: nesne yasar ama motoru bosaltir; sonrasindaki API cagrilari no-op
void XMap::teardown()
{
    close();
    mData.removeInstance(this);
    mWindow.reset();
    mLayers.reset();
    mTerrainLayer = nullptr;
    mFmsLayer = nullptr;
    mTrackLayer = nullptr;
    mAircraft.reset();
    mDrawings.reset();
    mPalette.reset();
}

void XMap::createLayers()
{
    // Alttan uste cizim sirasi (ayrinti: MIMARI.pdf bolum 7). Etiketler de bu sirayla
    // yer bulur: ustteki satirdaki katmanin etiketi onceliklidir (FMS ve
    // kullanici cizimleri etiketlerinin yerini onceden ayirir).
    // Veri depolari ortak (XMapData); katman nesneleri ve durumlari bu haritanin.
    AirportGeometryStore& tAirports = mData.airportGeometry();
    AirportDetailStore& tDetails = mData.airportDetails();
    std::unique_ptr<TerrainLayer> tTerrain = std::make_unique<TerrainLayer>(mData.hub(), *mPalette, mPaletteId);
    mTerrainLayer = tTerrain.get();
    mLayers->add(std::move(tTerrain));  // en altta: zemin
    mLayers->add(std::make_unique<RoadLayer>(mData.hub()));  // DSF yollari: zeminin ustu, yuzeylerin alti
    mLayers->add(std::make_unique<AirportSurfaceLayer>(tAirports, tDetails));
    mLayers->add(std::make_unique<GridLayer>());
    // ILS tuyleri pistlerin ALTINDA: buyuk yari saydam alanlar pistleri ortmesin
    mLayers->add(std::make_unique<NavaidLayer>(NavaidType::Ils, mData.navs(), &tAirports));
    mLayers->add(std::make_unique<RunwayLayer>(tAirports));
    mLayers->add(std::make_unique<TaxiRouteLayer>(tAirports, tDetails));  // varsayilan kapali
    mLayers->add(std::make_unique<AirportObjectsLayer>(tAirports, tDetails));  // pistlerin ustunde
    // Hava yollari navaid sembollerinin altinda (cizgiler sembolleri ortmesin)
    mLayers->add(std::make_unique<AirwayLayer>(mData.airways(), false));
    mLayers->add(std::make_unique<AirwayLayer>(mData.airways(), true));
    mLayers->add(std::make_unique<AirportLayer>(mData.navs(), &tAirports));
    mLayers->add(std::make_unique<NavaidLayer>(NavaidType::Vor, mData.navs()));
    mLayers->add(std::make_unique<NavaidLayer>(NavaidType::Ndb, mData.navs()));
    mLayers->add(std::make_unique<NavaidLayer>(NavaidType::Fix, mData.navs()));
    std::unique_ptr<FmsLayer> tFms = std::make_unique<FmsLayer>();
    mFmsLayer = tFms.get();
    mLayers->add(std::move(tFms));
    std::unique_ptr<TrackLayer> tTrack = std::make_unique<TrackLayer>(*mAircraft);
    mTrackLayer = tTrack.get();
    mLayers->add(std::move(tTrack));
    mLayers->add(std::make_unique<AircraftLayer>(*mAircraft));
    mLayers->add(std::make_unique<DrawingLayer>(*mDrawings));
}

int XMap::getId() const
{
    return mLayers ? mId : 0;
}

bool XMap::setName(const std::string& pName)
{
    if (!mLayers)
    {
        return false;
    }
    if (mWindow->isCreated())
    {
        log("WARNING: setName: map {} is open; close it first (the name selects the settings folder)", mId);
        return false;
    }
    // Ayni klasoru iki harita kullanmasin (birbirinin dosyasini ezerlerdi)
    const std::string tFolder = safeFolderName(pName);
    for (const XMap* tOther : mManager->mXMaps)
    {
        if (tOther != this && tOther->mLayers && !tFolder.empty() && tOther->folderName() == tFolder)
        {
            log("WARNING: setName: '{}' is already used by map {}", pName, tOther->getId());
            return false;
        }
    }
    mName = pName;
    return true;
}

std::string XMap::getName() const
{
    return mName;
}

std::string XMap::getSettingsDirectory() const
{
    return mData.mapsDirectory() + folderName() + "/";
}

void XMap::setPersistence(Persistence pPersistence)
{
    mPersistence = pPersistence;
}

Persistence XMap::getPersistence() const
{
    return mPersistence;
}

bool XMap::saveSettings()
{
    return saveNow();
}

bool XMap::initialize()
{
    if (mWindow->isCreated())
    {
        return true;
    }
    if (mPersistence == Persistence::LoadAndSave)
    {
        loadFiles();
    }
    // Yer verilmemisse (ya da Standalone pencere ekran disinda kalacaksa) varsayilan yer
    const RectI tGeometry = mWindow->getGeometry();
    const bool tUsable = mWindow->isEmbedded() ? (tGeometry.width() > 0 && tGeometry.height() > 0)
                                               : isStandaloneGeometryUsable(tGeometry);
    if (!tUsable)
    {
        mWindow->setGeometry(defaultGeometry());
    }
    mFloatingGeometry = mWindow->getGeometry();
    mWindow->create();
    if (!mWindow->isCreated())
    {
        return false;
    }
    mData.openXMapsChanged();  // ortak onbellek sinirlari acik harita sayisina gore buyur
    if (mPersistence != Persistence::Off)
    {
        // Dosyalar ilk acilista olussun (kullanici acip duzenleyebilsin)
        std::error_code tError;
        std::filesystem::create_directories(getSettingsDirectory(), tError);
        const std::string tTerrainPath = getSettingsDirectory() + kTerrainFileName;
        if (!std::filesystem::exists(tTerrainPath, tError) || !mPalette->loadedCurrentVersion())
        {
            writeTerrainFile();
        }
        writeSettingsFile();
    }
    mSavedSettings = mWindow->settingsRevision();
    mSavedDrawings = mWindow->drawingsRevision();
    mSavedState = stateSignature();
    mSavedExtra = mExtraRevision;
    log("map {} '{}' opened ({})", mId, folderName(),
        mPersistence == Persistence::Off ? std::string("no settings files") : "settings: " + getSettingsDirectory());
    return true;
}

void XMap::close()
{
    if (!mWindow || !mWindow->isCreated())
    {
        return;
    }
    if (!mWindow->isPoppedOut())
    {
        mFloatingGeometry = mWindow->getGeometry();
    }
    saveNow();
    mWindow->destroy();
    mData.openXMapsChanged();
    log("map {} '{}' closed", mId, folderName());
}

bool XMap::isInitialized() const
{
    return mWindow && mWindow->isCreated();
}

void XMap::setStyle(WindowStyle pStyle)
{
    mWindow->setEmbedded(pStyle == WindowStyle::Embedded);  // acik pencerede uyari loglar, degistirmez
}

WindowStyle XMap::getStyle() const
{
    return !mWindow->isEmbedded() ? WindowStyle::Standalone : WindowStyle::Embedded;
}

void XMap::setTitle(const std::string& pTitle)
{
    mWindow->setTitle(pTitle);
}

std::string XMap::getTitle() const
{
    return mWindow->title();
}

void XMap::setGeometry(const Geometry& pGeometry)
{
    mWindow->setGeometry(toRect(pGeometry));
}

Geometry XMap::getGeometry() const
{
    Geometry tGeometry;
    const RectI tRect = mWindow->getGeometry();
    tGeometry.mLeft = tRect.mLeft;
    tGeometry.mTop = tRect.mTop;
    tGeometry.mWidth = tRect.width();
    tGeometry.mHeight = tRect.height();
    return tGeometry;
}

void XMap::setVisible(bool pVisible)
{
    mWindow->setVisible(pVisible);
}

bool XMap::isVisible() const
{
    return mWindow->isVisible();
}

void XMap::bringToFront()
{
    mWindow->bringToFront();
}

void XMap::setPoppedOut(bool pPoppedOut)
{
    mWindow->setPoppedOut(pPoppedOut);
}

bool XMap::isPoppedOut() const
{
    return mWindow->isPoppedOut();
}

void XMap::setIsUiVisible(bool pVisible)
{
    mWindow->setUiVisible(pVisible);
}

bool XMap::isUiVisible() const
{
    return mWindow->isUiVisible();
}

void XMap::setIsMouseInputEnabled(bool pEnabled)
{
    mWindow->setMouseInput(pEnabled);
}

bool XMap::isMouseInputEnabled() const
{
    return mWindow->isMouseInputEnabled();
}

void XMap::setCenter(double pLat, double pLon)
{
    mWindow->setFollowAircraft(false);  // takip acik kalirsa harita hemen ucaga doner
    mWindow->view().setCenter({pLat, pLon});
}

LatLon XMap::getCenter() const
{
    LatLon tCenter;
    const ::LatLon tInner = mWindow->view().center();
    tCenter.mLat = tInner.mLat;
    tCenter.mLon = tInner.mLon;
    return tCenter;
}

void XMap::setZoom(double pZoom)
{
    mWindow->view().setZoom(pZoom);
}

double XMap::getZoom() const
{
    return mWindow->view().zoom();
}

double XMap::minZoom()
{
    return XMapView::kMinZoom;
}

double XMap::maxZoom()
{
    return XMapView::kMaxZoom;
}

double XMap::hardMaxZoom()
{
    return XMapView::kHardMaxZoom;
}

bool XMap::setZoomLimits(double pMinZoom, double pMaxZoom)
{
    if (!(pMinZoom <= pMaxZoom))
    {
        log("WARNING: setZoomLimits: min {} is greater than max {}", pMinZoom, pMaxZoom);
        return false;
    }
    mWindow->setZoomLimits(pMinZoom, pMaxZoom);
    return true;
}

double XMap::getMinZoomLimit() const
{
    return mWindow->view().minZoomLimit();
}

double XMap::getMaxZoomLimit() const
{
    return mWindow->view().maxZoomLimit();
}

void XMap::zoomIn()
{
    setZoom(getZoom() + getZoomStep());
}

void XMap::zoomOut()
{
    setZoom(getZoom() - getZoomStep());
}

void XMap::setZoomStep(double pStep)
{
    if (pStep > 0.0 && pStep <= 5.0)
    {
        mWindow->setZoomStep(pStep);
    }
    else
    {
        log("WARNING: setZoomStep: {} out of range (0, 5]", pStep);
    }
}

double XMap::getZoomStep() const
{
    return mWindow->zoomStep();
}

void XMap::panLeft()
{
    // Sola bakmak = harita icerigi saga kayar
    panBy(getGeometry().mWidth * getPanStep(), 0.0);
}

void XMap::panRight()
{
    panBy(-getGeometry().mWidth * getPanStep(), 0.0);
}

void XMap::panUp()
{
    // Kuzeye bakmak = icerik asagi kayar (y yukari artar)
    panBy(0.0, -getGeometry().mHeight * getPanStep());
}

void XMap::panDown()
{
    panBy(0.0, getGeometry().mHeight * getPanStep());
}

void XMap::setPanStep(double pFractionOfView)
{
    if (pFractionOfView > 0.0 && pFractionOfView <= 1.0)
    {
        mWindow->setPanStep(pFractionOfView);
    }
    else
    {
        log("WARNING: setPanStep: {} out of range (0, 1]", pFractionOfView);
    }
}

double XMap::getPanStep() const
{
    return mWindow->panStep();
}

void XMap::panBy(double pDeltaX, double pDeltaY)
{
    mWindow->setFollowAircraft(false);  // elle kaydirma gibi: takip biter
    mWindow->view().panByBoxels(pDeltaX, pDeltaY);
}

void XMap::fitBounds(const GeoBounds& pBounds)
{
    if (pBounds.mNorth <= pBounds.mSouth || pBounds.mEast <= pBounds.mWest)
    {
        log("WARNING: fitBounds: empty or invalid area");
        return;
    }
    const Geometry tGeometry = getGeometry();
    if (tGeometry.mWidth <= 0 || tGeometry.mHeight <= 0)
    {
        log("WARNING: fitBounds: the map has no size yet (call setGeometry first)");
        return;
    }
    const double tX0 = Mercator::lonToX(pBounds.mWest);
    const double tX1 = Mercator::lonToX(pBounds.mWest) + (pBounds.mEast - pBounds.mWest) / 360.0;
    const double tY0 = Mercator::latToY(pBounds.mSouth);
    const double tY1 = Mercator::latToY(pBounds.mNorth);
    const double tWidth = std::max(tX1 - tX0, 1e-12);
    const double tHeight = std::max(tY1 - tY0, 1e-12);
    // Dunya kenari 256 * 2^zoom boxel: alanin iki yonde de sigacagi en buyuk zoom; kenarlarda %5 pay
    // (alan pencere kenarina yapismasin, yuvarlama ile disarida kalmasin)
    const double kMargin = 1.05;
    const double tScale = std::min(tGeometry.mWidth / (XMapView::kTileSize * tWidth * kMargin),
                                   tGeometry.mHeight / (XMapView::kTileSize * tHeight * kMargin));
    mWindow->setFollowAircraft(false);
    mWindow->view().setCenterMercator(std::fmod(tX0 + tWidth * 0.5, 1.0), (tY0 + tY1) * 0.5);
    mWindow->view().setZoom(std::log2(std::max(tScale, 1e-9)));
}

GeoBounds XMap::getVisibleBounds() const
{
    GeoBounds tBounds;
    const ::GeoBounds tInner = mWindow->view().visibleBounds();
    tBounds.mSouth = tInner.mSouth;
    tBounds.mWest = tInner.mWest;
    tBounds.mNorth = tInner.mNorth;
    tBounds.mEast = tInner.mEast;
    return tBounds;
}

double XMap::getMetersPerBoxel() const
{
    return mWindow->view().metersPerBoxel();
}

void XMap::setFollowAircraft(bool pFollow)
{
    mWindow->setFollowAircraft(pFollow);
}

bool XMap::isFollowingAircraft() const
{
    return mWindow->isFollowingAircraft();
}

void XMap::centerOnAircraft()
{
    mWindow->centerOnAircraft();
}

bool XMap::goToAirport(const std::string& pIdent)
{
    const AirportGeometryStore& tAirports = mData.airports();
    const std::string tWanted = api::upper(pIdent);
    for (int tIndex : ::findAirports(tAirports, pIdent, 8))
    {
        if (api::upper(tAirports.at(tIndex).mId) == tWanted)
        {
            mWindow->goToAirport(tIndex);
            return true;
        }
    }
    log("go to airport: '{}' not found", pIdent);
    return false;
}

std::vector<AirportMatch> XMap::findAirports(const std::string& pQuery, int pMaxResults) const
{
    std::vector<AirportMatch> tMatches;
    const AirportGeometryStore& tAirports = mData.airports();
    for (int tIndex : ::findAirports(tAirports, pQuery, std::max(0, pMaxResults)))
    {
        const AirportGeometry& tAirport = tAirports.at(tIndex);
        const ::LatLon tCenter = airportCenter(tAirport);
        AirportMatch tMatch;
        tMatch.mIdent = tAirport.mId;
        tMatch.mName = tAirport.mName;
        tMatch.mPosition.mLat = tCenter.mLat;
        tMatch.mPosition.mLon = tCenter.mLon;
        tMatches.push_back(tMatch);
    }
    return tMatches;
}

LatLon XMap::screenToGeo(int pX, int pY) const
{
    LatLon tPoint;
    const ::LatLon tInner = mWindow->view().boxelToLatLon(pX, pY);
    tPoint.mLat = tInner.mLat;
    tPoint.mLon = tInner.mLon;
    return tPoint;
}

bool XMap::geoToScreen(double pLat, double pLon, int* pX, int* pY) const
{
    if (!pX || !pY)
    {
        return false;
    }
    double tX = 0.0;
    double tY = 0.0;
    mWindow->view().latLonToBoxel({pLat, pLon}, &tX, &tY);
    *pX = static_cast<int>(std::lround(tX));
    *pY = static_cast<int>(std::lround(tY));
    const RectD& tArea = mWindow->view().viewport();
    return tX >= tArea.mLeft && tX <= tArea.mRight && tY >= tArea.mBottom && tY <= tArea.mTop;
}

void XMap::setMode(XMapMode pMode)
{
    mWindow->setMapMode(static_cast<::XMapMode>(pMode));
}

XMapMode XMap::getMode() const
{
    return static_cast<XMapMode>(mWindow->getMapMode());
}

void XMap::nextMode()
{
    mWindow->nextMapMode();
}

void XMap::setIsLayerVisible(XMapLayer pLayer, bool pVisible)
{
    if (LayerRules::setVisible(*mLayers, themeFor(mWindow->getMapMode()), pLayer, pVisible))
    {
        mWindow->saveSettings();
    }
}

bool XMap::isLayerVisible(XMapLayer pLayer) const
{
    return LayerRules::isVisible(*mLayers, pLayer);
}

void XMap::toggleLayer(XMapLayer pLayer)
{
    if (LayerRules::toggle(*mLayers, themeFor(mWindow->getMapMode()), pLayer))
    {
        mWindow->saveSettings();
    }
}

void XMap::setLayersVisible(std::initializer_list<XMapLayer> pLayers, bool pVisible)
{
    for (XMapLayer tLayer : pLayers)
    {
        setIsLayerVisible(tLayer, pVisible);
    }
}

void XMap::showOnlyLayers(std::initializer_list<XMapLayer> pLayers)
{
    LayerRules::showOnly(*mLayers, themeFor(mWindow->getMapMode()), pLayers.begin(), pLayers.end());
    mWindow->saveSettings();
}

void XMap::resetLayersToDefaults()
{
    mWindow->resetLayers();
}

LayerState XMap::getLayerState(XMapLayer pLayer) const
{
    return LayerRules::state(*mLayers, pLayer);
}

bool XMap::setLayerZoomRange(XMapLayer pLayer, double pMinZoom, double pMaxZoom)
{
    if (!(pMinZoom <= pMaxZoom))
    {
        log("WARNING: setLayerZoomRange: min {} is greater than max {}", pMinZoom, pMaxZoom);
        return false;
    }
    bool tApplied = false;
    if (LayerRules::kindOf(pLayer) == LayerKind::Group)
    {
        for (XMapLayer tChild : LayerRules::childrenOf(pLayer))
        {
            if (Layer* tLayer = LayerRules::findLayer(*mLayers, tChild))
            {
                tLayer->setZoomRange(pMinZoom, pMaxZoom);
                tApplied = true;
            }
        }
    }
    else if (Layer* tLayer = LayerRules::findLayer(*mLayers, pLayer))
    {
        const int tSub = LayerRules::subLayerIndex(*mLayers, pLayer);
        if (tSub >= 0)
        {
            tLayer->setSubLayerZoomRange(tSub, pMinZoom, pMaxZoom);
        }
        else
        {
            tLayer->setZoomRange(pMinZoom, pMaxZoom);
        }
        tApplied = true;
    }
    if (tApplied)
    {
        mWindow->saveSettings();
    }
    return tApplied;
}

void XMap::getLayerZoomRange(XMapLayer pLayer, double* pMinZoom, double* pMaxZoom) const
{
    double tMin = Layer::kNoMinZoom;
    double tMax = Layer::kNoMaxZoom;
    XMapLayer tItem = pLayer;
    if (LayerRules::kindOf(pLayer) == LayerKind::Group)
    {
        const std::vector<XMapLayer> tChildren = LayerRules::childrenOf(pLayer);
        tItem = tChildren.empty() ? pLayer : tChildren.front();
    }
    if (const Layer* tLayer = LayerRules::findLayer(*mLayers, tItem))
    {
        const int tSub = LayerRules::subLayerIndex(*mLayers, tItem);
        tMin = tSub >= 0 ? tLayer->subLayerAt(tSub).mMinZoom : tLayer->minZoom();
        tMax = tSub >= 0 ? tLayer->subLayerAt(tSub).mMaxZoom : tLayer->maxZoom();
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

void XMap::clearLayerZoomRange(XMapLayer pLayer)
{
    setLayerZoomRange(pLayer, Layer::kNoMinZoom, Layer::kNoMaxZoom);
}

void XMap::clearAllLayerZoomRanges()
{
    mLayers->resetZoomRanges();
    mWindow->saveSettings();
}

void XMap::setDrawTool(DrawTool pTool)
{
    mDrawings->setMode(static_cast<Drawings::Mode>(pTool));  // yarim cizim biter
    mWindow->drawingsChanged();
}

DrawTool XMap::getDrawTool() const
{
    return static_cast<DrawTool>(mDrawings->getMode());
}

bool XMap::addDrawingPoint(double pLat, double pLon)
{
    if (mDrawings->getMode() == Drawings::Mode::Pan)
    {
        return false;
    }
    mDrawings->addVertex({pLat, pLon});
    mWindow->drawingsChanged();
    return true;
}

bool XMap::finishDrawing()
{
    if (!mDrawings->current())
    {
        return false;
    }
    mDrawings->finish();
    mWindow->drawingsChanged();
    return true;
}

bool XMap::undoDrawing()
{
    if (!mDrawings->current() && mDrawings->shapeCount() == 0)
    {
        return false;
    }
    mDrawings->undo();
    mWindow->drawingsChanged();
    return true;
}

void XMap::clearDrawings()
{
    mDrawings->clear();
    mWindow->drawingsChanged();
}

bool XMap::hasUnfinishedDrawing() const
{
    return mDrawings->current() != nullptr;
}

std::vector<Shape> XMap::getShapes() const
{
    std::vector<Shape> tShapes;
    for (const Drawings::Shape& tInner : mDrawings->shapes())
    {
        Shape tShape;
        tShape.mKind = static_cast<ShapeKind>(tInner.mKind);
        tShape.mId = static_cast<int>(tInner.mKind) * kShapeIdBase + tInner.mNumber;
        tShape.mLabel = Drawings::labelOf(tInner);
        for (const ::LatLon& tPoint : tInner.mPoints)
        {
            tShape.mPoints.push_back({tPoint.mLat, tPoint.mLon});
        }
        tShapes.push_back(tShape);
    }
    return tShapes;
}

bool XMap::removeShape(int pId)
{
    const int tKind = pId / kShapeIdBase;
    if (tKind < 0 || tKind > 2 ||
        !mDrawings->remove(static_cast<Drawings::ShapeKind>(tKind), pId % kShapeIdBase))
    {
        log("WARNING: removeShape: no shape with id {}", pId);
        return false;
    }
    mWindow->drawingsChanged();
    return true;
}

std::string XMap::exportDrawings() const
{
    return mDrawings->serialize();
}

bool XMap::importDrawings(const std::string& pText)
{
    const int tCount = mDrawings->deserialize(pText);
    mWindow->drawingsChanged();
    log("drawings imported: {} shapes", tCount);
    return tCount > 0 || pText.find_first_not_of(" \t\r\n") == std::string::npos;
}

void XMap::setAircraftSource(DataSource pSource)
{
    mAircraft->setFromHost(pSource == DataSource::Host);
    markChanged();
}

DataSource XMap::getAircraftSource() const
{
    return mAircraft->isFromHost() ? DataSource::Host : DataSource::XPlane;
}

void XMap::setAircraft(const AircraftState& pState)
{
    ::AircraftState tInner;
    tInner.mValid = pState.mValid;
    tInner.mPosition = {pState.mPosition.mLat, pState.mPosition.mLon};
    tInner.mHeadingDeg = pState.mHeadingDeg;
    tInner.mGroundSpeedKt = pState.mGroundSpeedKt;
    tInner.mAltitudeFt = pState.mAltitudeFt;
    mAircraft->setHostState(tInner);
}

AircraftState XMap::getAircraft() const
{
    AircraftState tState;
    const ::AircraftState& tInner = mAircraft->state();
    tState.mValid = tInner.mValid;
    tState.mPosition = {tInner.mPosition.mLat, tInner.mPosition.mLon};
    tState.mHeadingDeg = tInner.mHeadingDeg;
    tState.mGroundSpeedKt = tInner.mGroundSpeedKt;
    tState.mAltitudeFt = tInner.mAltitudeFt;
    return tState;
}

void XMap::clearTrack()
{
    if (mTrackLayer)
    {
        mTrackLayer->clear();
        log("flight track cleared (map {})", mId);
    }
}

void XMap::setRouteSource(DataSource pSource)
{
    if (mFmsLayer)
    {
        mFmsLayer->setFromHost(pSource == DataSource::Host);
        markChanged();
    }
}

DataSource XMap::getRouteSource() const
{
    return (mFmsLayer && mFmsLayer->isFromHost()) ? DataSource::Host : DataSource::XPlane;
}

void XMap::setRoute(const std::vector<RouteWaypoint>& pWaypoints, int pActiveIndex)
{
    if (!mFmsLayer)
    {
        return;
    }
    if (!mFmsLayer->isFromHost())
    {
        log("WARNING: setRoute ignored: route source is X-plane(call setRouteSource(DataSource::Host))");
        return;
    }
    std::vector<FmsLayer::Waypoint> tInner;
    for (const RouteWaypoint& tPoint : pWaypoints)
    {
        tInner.push_back({{tPoint.mPosition.mLat, tPoint.mPosition.mLon}, tPoint.mIdent, tPoint.mAltitudeFt});
    }
    mFmsLayer->setHostRoute(tInner, pActiveIndex);
}

std::vector<XMapObject> XMap::queryAt(double pLat, double pLon) const
{
    int tX = 0;
    int tY = 0;
    if (!geoToScreen(pLat, pLon, &tX, &tY))
    {
        return {};
    }
    return queryAtScreen(tX, tY);
}

std::vector<XMapObject> XMap::queryAtScreen(int pX, int pY) const
{
    std::vector<XMapObject> tObjects;
    if (!mWindow->isCreated())
    {
        return tObjects;  // sorgu son cizilen kareye gore: pencere acik olmali
    }
    for (const auto& [tId, tText] : mWindow->describeAt(pX, pY))
    {
        XMapObject tObject;
        tObject.mLayer = LayerRules::layerFromKey(tId).value_or(XMapLayer::Count);
        tObject.mText = tText;
        tObjects.push_back(tObject);
    }
    return tObjects;
}

void XMap::setImageCache(bool pEnabled)
{
    mWindow->setImageCacheEnabled(pEnabled);
}

bool XMap::isImageCacheEnabled() const
{
    return mWindow->isImageCacheEnabled();
}

void XMap::setMaxRenderFps(int pFps)
{
    mWindow->setMaxRenderFps(pFps);
}

int XMap::getMaxRenderFps() const
{
    return mWindow->maxRenderFps();
}

void XMap::setXPlaneFont(bool pXPlaneFont)
{
    mWindow->setXPlaneFont(pXPlaneFont);
}

bool XMap::isXPlaneFont() const
{
    return mWindow->usesXPlaneFont();
}

std::string XMap::exportSettings() const
{
    storeExtraSections();
    mWindow->saveSettings();
    return mWindow->settingsStore().toText({kWindowSection, "view", "render", "text", "tools", "layers*",
                                            "layer_zoom", kSourcesSection, kLayerColorsSection});
}

bool XMap::importSettings(const std::string& pText)
{
    mWindow->importSettings(pText);
    // [window] / [sources] sadece metinde varsa uygulanir (importSettings bellekteki ayarlara ekledi)
    Settings tGiven;
    tGiven.loadText(pText);
    if (tGiven.has(kWindowSection, "left") || tGiven.has(kWindowSection, "visible"))
    {
        applyWindowSection();
    }
    applySourcesSection();
    applyLayerColorsSection(tGiven);
    return true;
}

TerrainSettings XMap::getTerrainSettings() const
{
    TerrainSettings tOut;
    Settings tIni;
    mPalette->save(tIni);
    tOut.mUseXPlaneColors = tIni.getString("palette", "source", "xplane") != "ini";
    for (int tClass = 0; tClass < kTerrainClassCount; ++tClass)
    {
        const std::string tKey = terrainClassKey(static_cast<TerrainClass>(tClass));
        if (tIni.has("colors", tKey))
        {
            std::istringstream tStream(tIni.getString("colors", tKey, ""));
            tOut.mClassColors.emplace_back(tKey, api::parseRgb(tStream));
        }
    }
    tOut.mShadingStrength = tIni.getDouble("shading", "strength", 0.0);
    tOut.mBlendMeters = tIni.getDouble("blend", "meters", 0.0);
    tOut.mBlendMaxPixels = tIni.getDouble("blend", "max_pixels", 0.0);
    tOut.mWarpMeters = tIni.getDouble("warp", "meters", 0.0);
    tOut.mWarpWavelength = tIni.getDouble("warp", "wavelength", 0.0);
    tOut.mCloseupClassMix = tIni.getDouble("closeup", "class_mix", 0.0);
    tOut.mReliefMultiDirectional = tIni.getBool("relief", "multi_directional", true);
    tOut.mReliefExaggeration = tIni.getDouble("relief", "exaggeration", 0.0);
    tOut.mReliefCoarseBoost = tIni.getDouble("relief", "coarse_boost", 0.0);
    tOut.mReliefSlopeTone = tIni.getDouble("relief", "slope_tone", 0.0);
    tOut.mReliefFlatSlope = tIni.getDouble("relief", "flat_slope", 0.0);
    tOut.mCoastline = tIni.getBool("water", "coastline", true);
    tOut.mCoastlineStrength = tIni.getDouble("water", "coastline_strength", 0.0);
    tOut.mWaterBandMeters = tIni.getDouble("water", "band_meters", 0.0);
    tOut.mWaterBandMaxPixels = tIni.getDouble("water", "band_max_pixels", 0.0);
    tOut.mWaterBandStrength = tIni.getDouble("water", "band_strength", 0.0);
    tOut.mDepthTone = tIni.getDouble("water", "depth_tone", 0.0);
    tOut.mDepthFullMeters = tIni.getDouble("water", "depth_full_meters", 0.0);
    tOut.mVfrBands = tIni.getBool("vfr", "bands", true);
    tOut.mVfrContourLines = tIni.getBool("vfr", "contour_lines", false);
    tOut.mVfrContourStrength = tIni.getDouble("vfr", "contour_strength", 0.0);
    for (int tBand = 1; tBand <= kVfrBandCount; ++tBand)
    {
        std::istringstream tStream(tIni.getString("vfr", std::format("band_{}", tBand), ""));
        double tFeet = 0.0;
        tStream >> tFeet;
        tOut.mVfrBandsFeet.emplace_back(tFeet, api::parseRgb(tStream));
    }
    return tOut;
}

void XMap::setTerrainSettings(const TerrainSettings& pSettings)
{
    Settings tIni;
    mPalette->save(tIni);  // [palette] version ve bilinmeyen anahtarlar korunur
    tIni.setString("palette", "source", pSettings.mUseXPlaneColors ? "xplane" : "ini");
    for (const auto& [tKey, tColor] : pSettings.mClassColors)
    {
        tIni.setString("colors", tKey, std::format("{} {} {}", tColor.mR, tColor.mG, tColor.mB));
    }
    tIni.setDouble("shading", "strength", pSettings.mShadingStrength);
    tIni.setDouble("blend", "meters", pSettings.mBlendMeters);
    tIni.setDouble("blend", "max_pixels", pSettings.mBlendMaxPixels);
    tIni.setDouble("warp", "meters", pSettings.mWarpMeters);
    tIni.setDouble("warp", "wavelength", pSettings.mWarpWavelength);
    tIni.setDouble("closeup", "class_mix", pSettings.mCloseupClassMix);
    tIni.setBool("relief", "multi_directional", pSettings.mReliefMultiDirectional);
    tIni.setDouble("relief", "exaggeration", pSettings.mReliefExaggeration);
    tIni.setDouble("relief", "coarse_boost", pSettings.mReliefCoarseBoost);
    tIni.setDouble("relief", "slope_tone", pSettings.mReliefSlopeTone);
    tIni.setDouble("relief", "flat_slope", pSettings.mReliefFlatSlope);
    tIni.setBool("water", "coastline", pSettings.mCoastline);
    tIni.setDouble("water", "coastline_strength", pSettings.mCoastlineStrength);
    tIni.setDouble("water", "band_meters", pSettings.mWaterBandMeters);
    tIni.setDouble("water", "band_max_pixels", pSettings.mWaterBandMaxPixels);
    tIni.setDouble("water", "band_strength", pSettings.mWaterBandStrength);
    tIni.setDouble("water", "depth_tone", pSettings.mDepthTone);
    tIni.setDouble("water", "depth_full_meters", pSettings.mDepthFullMeters);
    tIni.setBool("vfr", "bands", pSettings.mVfrBands);
    tIni.setBool("vfr", "contour_lines", pSettings.mVfrContourLines);
    tIni.setDouble("vfr", "contour_strength", pSettings.mVfrContourStrength);
    for (size_t tBand = 0; tBand < pSettings.mVfrBandsFeet.size() && tBand < kVfrBandCount; ++tBand)
    {
        const auto& [tFeet, tColor] = pSettings.mVfrBandsFeet[tBand];
        tIni.setString("vfr", std::format("band_{}", tBand + 1),
                       std::format("{:.0f} {} {} {}", tFeet, tColor.mR, tColor.mG, tColor.mB));
    }
    importTerrainSettings(tIni.toText({}));
}

std::string XMap::exportTerrainSettings() const
{
    Settings tColors;
    mPalette->save(tColors);
    return tColors.toText({});
}

bool XMap::importTerrainSettings(const std::string& pText)
{
    Settings tColors;
    tColors.loadText(pText);
    TerrainPalette tPalette;
    tPalette.load(tColors);
    *mPalette = tPalette;
    mPaletteId = mData.registerPalette(mPalette.get());  // X-Plane renkleri uygulanir; ayni ayar = ayni numara
    if (mTerrainLayer)
    {
        mTerrainLayer->setPalette(*mPalette, mPaletteId);  // yeni dokular yeni paletle istenir (hemen)
    }
    mWindow->invalidateImage();  // onbellekteki harita resmi eski renklerle: hemen yeniden ciz
    markChanged();
    if (mWindow->isCreated() && mPersistence != Persistence::Off)
    {
        writeTerrainFile();
    }
    return true;
}

uint64_t XMap::getSettingsRevision() const
{
    return mWindow->settingsRevision();
}

uint64_t XMap::getDrawingsRevision() const
{
    return mWindow->drawingsRevision();
}

std::string XMap::getStatusText() const
{
    return mStatusText;
}

int XMap::getTerrainPendingImages() const
{
    return (mTerrainLayer && mTerrainLayer->isVisible() && mWindow->isVisible())
               ? mTerrainLayer->pendingTextureCount()
               : 0;
}

bool XMap::precacheVisibleArea()
{
    return precacheArea(getVisibleBounds());
}

bool XMap::precacheArea(const GeoBounds& pArea)
{
    ::GeoBounds tInner;
    tInner.mSouth = pArea.mSouth;
    tInner.mWest = pArea.mWest;
    tInner.mNorth = pArea.mNorth;
    tInner.mEast = pArea.mEast;
    return mData.startPrecache(tInner);
}

void XMap::cancelPrecache()
{
    mData.cancelPrecache();
}

void XMap::setDiagnostics(bool pOn)
{
    mWindow->setDiagnostics(pOn);
    log("diagnostics mode: {} (DIAG lines in Log.txt)", pOn ? "on" : "off");
}

bool XMap::isDiagnosticsOn() const
{
    return mWindow->isDiagnosticsOn();
}

void XMap::onRender()
{
    mWindow->render();
}

bool XMap::isDataLoaded() const
{
    return mIsDataLoaded;
}

bool XMap::setColor(XMapLayer pLayer, const Rgb& pColor)
{
    if (!mWindow->setLayerColor(pLayer, Rgba{pColor.mR / 255.0f, pColor.mG / 255.0f, pColor.mB / 255.0f}))
    {
        return false;
    }
    markChanged();
    return true;
}

Rgb XMap::getColor(XMapLayer pLayer) const
{
    const Rgba tColor = mWindow->layerColor(pLayer);
    return Rgb{int(tColor.mR * 255.0f + 0.5f), int(tColor.mG * 255.0f + 0.5f), int(tColor.mB * 255.0f + 0.5f)};
}

void XMap::clearColor(XMapLayer pLayer)
{
    mWindow->clearLayerColor(pLayer);
    markChanged();
}

void XMap::clearAllColors()
{
    mWindow->clearLayerColors();
    markChanged();
}

// --- Motor: harita basina durum ve dosyalar ---

void XMap::update(float pNow)
{
    mAircraft->refresh();
    mLayers->updateAll(pNow);

    // Alt seritteki yazi: ortak durum + BU haritanin zemin durumu. Ekranda istenen kalitede olmayan
    // (kaba yedek / bos) doku oldugu surece yaz; isci bosta ise bekleyen bir sey yoktur (uretilemeyen
    // parca): yazma, takili kalmasin.
    std::string tText = mData.sharedStatusText();
    std::string tTerrain = mData.terrainWorkerStatus();
    const int tPending = getTerrainPendingImages();
    if (tPending > 0 && !mData.isTerrainIdle())
    {
        const std::string tLoading = std::format("Loading terrain: {} image{}", tPending, tPending == 1 ? "" : "s");
        tTerrain = tTerrain.empty() ? tLoading : tLoading + " (" + tTerrain + ")";
    }
    if (!tTerrain.empty())
    {
        tText += tText.empty() ? "" : "   ";
        tText += tTerrain;
    }
    mStatusText = tText;
    mWindow->setStatusMessage(mStatusText);
    mWindow->setDataBusy(mData.isBusy());

    // isDataLoaded: ortak veri hazir ve bu haritada bekleyen doku yok
    if (mDataLoadedFlag)
    {
        const XMapDataStatus& tData = mData.status();
        *mDataLoadedFlag = tData.mNavReady && tData.mAirportsReady && tData.mAirwaysReady &&
                           tData.mTerrainIndexReady && tPending == 0;
    }

    if (!mWindow->isCreated())
    {
        return;
    }
    mWindow->update(pNow);
    if (!mWindow->isPoppedOut())
    {
        mFloatingGeometry = mWindow->getGeometry();
    }

    // Otomatik kayit: bir sey degistiyse, en erken autosave_seconds arayla
    const XMapManagerSettings& tLibrary = mData.managerSettings();
    if (mPersistence == Persistence::Off || !tLibrary.mAutoSave ||
        pNow - mLastSaveTime < static_cast<float>(tLibrary.mAutoSaveSeconds))
    {
        return;
    }
    if (mWindow->settingsRevision() != mSavedSettings || stateSignature() != mSavedState ||
        mExtraRevision != mSavedExtra)
    {
        mLastSaveTime = pNow;
        if (!writeSettingsFile())
        {
            log("WARNING: could not write {}{}", getSettingsDirectory(), kSettingsFileName);
        }
    }
    if (mWindow->drawingsRevision() != mSavedDrawings)
    {
        mLastSaveTime = pNow;
        if (!writeDrawingsFile())
        {
            log("WARNING: could not write {}{}", getSettingsDirectory(), kDrawingsFileName);
        }
    }
}

std::string XMap::folderName() const
{
    const std::string tSafe = safeFolderName(mName);
    return tSafe.empty() ? std::format("map_{}", mId) : tSafe;
}

std::string XMap::safeFolderName(const std::string& pName)
{
    std::string tSafe;
    for (const char tChar : pName)
    {
        const bool tAllowed = (tChar >= 'a' && tChar <= 'z') || (tChar >= 'A' && tChar <= 'Z') ||
                              (tChar >= '0' && tChar <= '9') || tChar == '-' || tChar == '_' || tChar == '.';
        tSafe += tAllowed ? tChar : '_';
    }
    // "." / ".." ust klasore cikmasin; Windows sondaki noktayi siler
    while (!tSafe.empty() && tSafe.back() == '.')
    {
        tSafe.back() = '_';
    }
    return tSafe;
}

RectI XMap::savedGeometry() const
{
    return mWindow->isCreated() && !mWindow->isPoppedOut() ? mWindow->getGeometry() : mFloatingGeometry;
}

uint64_t XMap::stateSignature() const
{
    // Kullanicinin fareyle yaptigi, revizyon sayacini artirmayan degisiklik: pencerenin yeri / boyutu.
    // gorunum(merkez, zoom) bilerek YOK: ucak takibinde her karede degisir, dosya surekli yazilirdi.
    // Gorunum, baska bir ayar degistiginde ve harita kapanirken yazilir.
    uint64_t tHash = 1469598103934665603ull;
    auto tMix = [&tHash](uint64_t pValue)
    {
        tHash = (tHash ^ pValue) * 1099511628211ull;
    };
    const RectI tGeometry = savedGeometry();
    tMix(static_cast<uint64_t>(static_cast<int64_t>(tGeometry.mLeft)));
    tMix(static_cast<uint64_t>(static_cast<int64_t>(tGeometry.mTop)));
    tMix(static_cast<uint64_t>(static_cast<int64_t>(tGeometry.mRight)));
    tMix(static_cast<uint64_t>(static_cast<int64_t>(tGeometry.mBottom)));
    return tHash;
}

void XMap::storeExtraSections() const
{
    Settings& tStore = mWindow->settingsStore();
    const RectI tGeometry = savedGeometry();
    tStore.setInt(kWindowSection, "left", tGeometry.mLeft);
    tStore.setInt(kWindowSection, "top", tGeometry.mTop);
    tStore.setInt(kWindowSection, "right", tGeometry.mRight);
    tStore.setInt(kWindowSection, "bottom", tGeometry.mBottom);
    tStore.setBool(kWindowSection, "visible", mWindow->isCreated() ? mWindow->isVisible() : mWindow->wantsVisible());
    tStore.setString(kSourcesSection, "aircraft", mAircraft->isFromHost() ? "host" : "xplane");
    tStore.setString(kSourcesSection, "route", mFmsLayer && mFmsLayer->isFromHost() ? "host" : "xplane");

    // [layer_colors]: API setColor ile degistirilen katman renkleri (tema uzerine yazmalar)
    tStore.clearSection(kLayerColorsSection);
    for (const auto& [tLayer, tColor] : mWindow->layerColors())
    {
        const auto tChannel = [](float pValue)
        {
            return std::clamp(static_cast<int>(pValue * 255.0f + 0.5f), 0, 255);
        };
        tStore.setString(kLayerColorsSection, LayerRules::layerKey(tLayer),
                         std::format("{} {} {}", tChannel(tColor.mR), tChannel(tColor.mG), tChannel(tColor.mB)));
    }
}

void XMap::applyWindowSection()
{
    // Sadece Standalone: kullanici pencereyi tasir / kapatir, bir sonraki acilista ayni yerde bulur.
    // Embedded haritanin yerini ve gorunurlugunu host belirler (kendi arayuzunun bir parcasi).
    if (mWindow->isEmbedded())
    {
        return;
    }
    const Settings& tStore = mWindow->settingsStore();
    if (tStore.has(kWindowSection, "left") && tStore.has(kWindowSection, "right"))
    {
        RectI tGeometry;
        tGeometry.mLeft = tStore.getInt(kWindowSection, "left", 0);
        tGeometry.mTop = tStore.getInt(kWindowSection, "top", 0);
        tGeometry.mRight = tStore.getInt(kWindowSection, "right", 0);
        tGeometry.mBottom = tStore.getInt(kWindowSection, "bottom", 0);
        if (isStandaloneGeometryUsable(tGeometry))
        {
            mWindow->setGeometry(tGeometry);
            mFloatingGeometry = tGeometry;
        }
    }
    if (tStore.has(kWindowSection, "visible"))
    {
        const bool tVisible = tStore.getBool(kWindowSection, "visible", true);
        if (tVisible != (mWindow->isCreated() ? mWindow->isVisible() : mWindow->wantsVisible()))
        {
            mWindow->setVisible(tVisible);
        }
    }
}

void XMap::applySourcesSection()
{
    const Settings& tStore = mWindow->settingsStore();
    if (tStore.has(kSourcesSection, "aircraft"))
    {
        mAircraft->setFromHost(tStore.getString(kSourcesSection, "aircraft", "xplane") == "host");
    }
    if (tStore.has(kSourcesSection, "route") && mFmsLayer)
    {
        mFmsLayer->setFromHost(tStore.getString(kSourcesSection, "route", "xplane") == "host");
    }
}

void XMap::applyLayerColorsSection(const Settings& pStore)
{
    const auto* tSection = pStore.findSection(kLayerColorsSection);
    if (!tSection)
    {
        return;
    }
    std::map<XMapLayer, Rgba> tColors;
    for (const auto& [tKey, tValue] : *tSection)
    {
        const std::optional<XMapLayer> tLayer = LayerRules::layerFromKey(tKey);
        std::istringstream tStream(tValue);
        int tR = 0;
        int tG = 0;
        int tB = 0;
        if (!(tStream >> tR >> tG >> tB))
        {
            log("WARNING: map {} '{}': bad [layer_colors] entry '{}'", mId, folderName(), tKey);
            continue;
        }
        if (!tLayer)
        {
            log("WARNING: map {} '{}': unknown [layer_colors] key '{}'", mId, folderName(), tKey);
            continue;
        }
        const auto tNorm = [](int pChannel)
        {
            return std::clamp(pChannel, 0, 255) / 255.0f;
        };
        tColors[*tLayer] = Rgba{tNorm(tR), tNorm(tG), tNorm(tB), 1.0f};
    }
    mWindow->setLayerColors(tColors);
}

void XMap::loadFiles()
{
    const std::string tDirectory = getSettingsDirectory();
    std::string tText;
    if (readTextFile(tDirectory + kTerrainFileName, &tText) ||
        readTextFile(tDirectory + "terrain_colors.ini", &tText))  // 0.23 dosyasi: gocsuz okunur
    {
        importTerrainSettings(tText);
    }
    if (readTextFile(tDirectory + kSettingsFileName, &tText) ||
        readTextFile(tDirectory + "map_settings.ini", &tText))  // 0.23 dosyasi: gocsuz okunur
    {
        importSettings(tText);
        log("map {} '{}': settings loaded from {}", mId, folderName(), tDirectory + kSettingsFileName);
    }
    else
    {
        log("map {} '{}': no settings file yet, using the values given through the API", mId, folderName());
    }
    if (readTextFile(tDirectory + kDrawingsFileName, &tText))
    {
        mDrawings->deserialize(tText);
        mWindow->drawingsChanged();
    }
}

bool XMap::writeSettingsFile()
{
    std::error_code tError;
    std::filesystem::create_directories(getSettingsDirectory(), tError);
    Settings tFile;
    tFile.loadText(exportSettings());
    const bool tOk = writeTextFile(getSettingsDirectory() + kSettingsFileName, tFile.toText({}));
    mSavedSettings = mWindow->settingsRevision();  // exportSettings sayaci artirir: sonra oku
    mSavedState = stateSignature();
    mSavedExtra = mExtraRevision;
    return tOk;
}

bool XMap::writeDrawingsFile()
{
    std::error_code tError;
    std::filesystem::create_directories(getSettingsDirectory(), tError);
    const bool tOk = writeTextFile(getSettingsDirectory() + kDrawingsFileName, mDrawings->serialize());
    mSavedDrawings = mWindow->drawingsRevision();
    return tOk;
}

bool XMap::writeTerrainFile()
{
    std::error_code tError;
    std::filesystem::create_directories(getSettingsDirectory(), tError);
    return writeTextFile(getSettingsDirectory() + kTerrainFileName, exportTerrainSettings());
}

bool XMap::saveNow()
{
    if (mPersistence == Persistence::Off)
    {
        return false;
    }
    bool tOk = writeSettingsFile();
    tOk = writeDrawingsFile() && tOk;
    tOk = writeTerrainFile() && tOk;
    if (!tOk)
    {
        log("WARNING: map {} '{}': could not write the settings files in {}", mId, folderName(),
            getSettingsDirectory());
    }
    return tOk;
}

void XMap::markChanged()
{
    ++mExtraRevision;
}

}  // namespace prime
