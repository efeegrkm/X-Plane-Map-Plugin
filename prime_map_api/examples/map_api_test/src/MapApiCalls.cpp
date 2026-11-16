// MapApiCalls.cpp
// Her metot tek bir Prime XMap API cagrisidir. Harita acik degilse (mMap bos) bir sey yapmaz /
// varsayilan doner; API'nin kendisi de ayni kurali uygular.
#include "MapApiCalls.h"

bool MapApiCalls::initLibrary(const std::string& pLogPrefix)
{
    if (!mManager)
    {
        mManager = prime::XMapManagerFactory::createXMapManager();
    }
    mManager->setLogPrefix(pLogPrefix);
    return mManager->initialize();
}

void MapApiCalls::shutdownLibrary()
{
    if (!mManager)
    {
        return;
    }
    mManager->shutdown();
    prime::XMapManagerFactory::destroyXMapManager(mManager);  // kalan haritalar da birakilir
    mManager = nullptr;
    mMap = nullptr;
    mSecondMap = nullptr;
}

bool MapApiCalls::openMap(const prime::Geometry& pGeometry, prime::WindowStyle pStyle, bool pUi, bool pMouse)
{
    if (!mManager)
    {
        return false;
    }
    if (!mManager)
    {
        return false;
    }
    prime::MapConfig tConfig;
    tConfig.mGeometry = pGeometry;
    tConfig.mStyle = pStyle;
    tConfig.mUiVisible = pUi;
    tConfig.mMouseInput = pMouse;
    mMap = mManager->addMap(tConfig);  // ayarlar uygulanir, acilmaz
    if (mMap)
    {
        mMap->initialize();
    }
    return mMap != nullptr;
}

bool MapApiCalls::openSecondMap()
{
    if (mSecondMap)
    {
        if (mManager)
        {
            mManager->removeMap(mSecondMap);  // ayarlari maps/second/ altina yazilir, pencere kapanir
        }
        mSecondMap = nullptr;
        return false;
    }
    // Once nesne, sonra ayarlar, en son initialize (PrimeMap.h basindaki akis)
    if (!mManager)
    {
        return false;
    }
    mSecondMap = mManager->addMap();
    if (!mSecondMap)
    {
        return false;
    }
    mSecondMap->setName("second");
    mSecondMap->setStyle(prime::WindowStyle::Standalone);
    mSecondMap->setTitle("Second map");
    mSecondMap->setGeometry({900, 700, 500, 360});
    mSecondMap->setMode(prime::MapMode::Vfr);
    mSecondMap->setIsUiVisible(true);
    mSecondMap->setIsMouseInputEnabled(true);
    return mSecondMap->initialize();
}

void MapApiCalls::closeMap()
{
    if (mMap && mManager)
    {
        mManager->removeMap(mMap);  // ayarlari yazar, pencereyi kapatir
    }
    mMap = nullptr;
}

bool MapApiCalls::isOpen() const
{
    return mMap != nullptr;
}

bool MapApiCalls::isInitialized() const
{
    return mManager && mManager->isInitialized();
}

prime::XMapManager* MapApiCalls::manager() const
{
    return mManager;
}
prime::Status MapApiCalls::getStatus() const
{
    if (!mManager)
    {
        return prime::Status{};
    }
    return mManager->getStatus();
}

std::string MapApiCalls::versionString() const
{
    return prime::versionString();
}

double MapApiCalls::minZoom() const
{
    return prime::XMap::minZoom();
}

double MapApiCalls::maxZoom() const
{
    return prime::XMap::maxZoom();
}

std::optional<prime::MapLayer> MapApiCalls::parentOf(prime::MapLayer pLayer) const
{
    if (!mManager)
    {
        return std::nullopt;
    }
    return mManager->parentOf(pLayer);
}

std::vector<prime::MapLayer> MapApiCalls::childrenOf(prime::MapLayer pLayer) const
{
    if (!mManager)
    {
        return {};
    }
    return mManager->childrenOf(pLayer);
}

std::vector<prime::MapLayer> MapApiCalls::topLevelLayers() const
{
    if (!mManager)
    {
        return {};
    }
    return mManager->topLevelLayers();
}

prime::LayerKind MapApiCalls::kindOf(prime::MapLayer pLayer) const
{
    if (!mManager)
    {
        return prime::LayerKind::Layer;
    }
    return mManager->kindOf(pLayer);
}

std::string MapApiCalls::layerName(prime::MapLayer pLayer) const
{
    if (!mManager)
    {
        return {};
    }
    return mManager->layerName(pLayer);
}

std::string MapApiCalls::layerKey(prime::MapLayer pLayer) const
{
    if (!mManager)
    {
        return {};
    }
    return mManager->layerKey(pLayer);
}

std::optional<prime::MapLayer> MapApiCalls::layerFromKey(const std::string& pKey) const
{
    if (!mManager)
    {
        return std::nullopt;
    }
    return mManager->layerFromKey(pKey);
}

bool MapApiCalls::isLayerDefaultVisible(prime::MapLayer pLayer, prime::MapMode pMode) const
{
    if (!mManager)
    {
        return false;
    }
    return mManager->isLayerDefaultVisible(pLayer, pMode);
}

void MapApiCalls::setGeometry(const prime::Geometry& pGeometry)
{
    if (mMap)
    {
        mMap->setGeometry(pGeometry);
    }
}

prime::Geometry MapApiCalls::getGeometry() const
{
    if (!mMap)
    {
        return prime::Geometry();
    }
    return mMap->getGeometry();
}

void MapApiCalls::setVisible(bool pVisible)
{
    if (mMap)
    {
        mMap->setVisible(pVisible);
    }
}

bool MapApiCalls::isVisible() const
{
    if (!mMap)
    {
        return false;
    }
    return mMap->isVisible();
}

void MapApiCalls::bringToFront()
{
    if (mMap)
    {
        mMap->bringToFront();
    }
}

void MapApiCalls::setPoppedOut(bool pPoppedOut)
{
    if (mMap)
    {
        mMap->setPoppedOut(pPoppedOut);
    }
}

bool MapApiCalls::isPoppedOut() const
{
    if (!mMap)
    {
        return false;
    }
    return mMap->isPoppedOut();
}

void MapApiCalls::setUiVisible(bool pVisible)
{
    if (mMap)
    {
        mMap->setIsUiVisible(pVisible);
    }
}

bool MapApiCalls::isUiVisible() const
{
    if (!mMap)
    {
        return false;
    }
    return mMap->isUiVisible();
}

void MapApiCalls::setMouseInput(bool pEnabled)
{
    if (mMap)
    {
        mMap->setIsMouseInputEnabled(pEnabled);
    }
}

bool MapApiCalls::isMouseInputEnabled() const
{
    if (!mMap)
    {
        return false;
    }
    return mMap->isMouseInputEnabled();
}

void MapApiCalls::setCenter(double pLat, double pLon)
{
    if (mMap)
    {
        mMap->setCenter(pLat, pLon);
    }
}

prime::LatLon MapApiCalls::getCenter() const
{
    if (!mMap)
    {
        return prime::LatLon();
    }
    return mMap->getCenter();
}

void MapApiCalls::setZoom(double pZoom)
{
    if (mMap)
    {
        mMap->setZoom(pZoom);
    }
}

double MapApiCalls::getZoom() const
{
    if (!mMap)
    {
        return 0.0;
    }
    return mMap->getZoom();
}

void MapApiCalls::zoomIn()
{
    if (mMap)
    {
        mMap->zoomIn();
    }
}

void MapApiCalls::zoomOut()
{
    if (mMap)
    {
        mMap->zoomOut();
    }
}

void MapApiCalls::setZoomStep(double pStep)
{
    if (mMap)
    {
        mMap->setZoomStep(pStep);
    }
}

void MapApiCalls::panLeft()
{
    if (mMap)
    {
        mMap->panLeft();
    }
}

void MapApiCalls::panRight()
{
    if (mMap)
    {
        mMap->panRight();
    }
}

void MapApiCalls::panUp()
{
    if (mMap)
    {
        mMap->panUp();
    }
}

void MapApiCalls::panDown()
{
    if (mMap)
    {
        mMap->panDown();
    }
}

void MapApiCalls::setPanStep(double pFraction)
{
    if (mMap)
    {
        mMap->setPanStep(pFraction);
    }
}

void MapApiCalls::panBy(double pDeltaX, double pDeltaY)
{
    if (mMap)
    {
        mMap->panBy(pDeltaX, pDeltaY);
    }
}

void MapApiCalls::fitBounds(const prime::GeoBounds& pBounds)
{
    if (mMap)
    {
        mMap->fitBounds(pBounds);
    }
}

prime::GeoBounds MapApiCalls::getVisibleBounds() const
{
    if (!mMap)
    {
        return prime::GeoBounds();
    }
    return mMap->getVisibleBounds();
}

double MapApiCalls::getMetersPerBoxel() const
{
    if (!mMap)
    {
        return 0.0;
    }
    return mMap->getMetersPerBoxel();
}

void MapApiCalls::setFollowAircraft(bool pFollow)
{
    if (mMap)
    {
        mMap->setFollowAircraft(pFollow);
    }
}

bool MapApiCalls::isFollowingAircraft() const
{
    if (!mMap)
    {
        return false;
    }
    return mMap->isFollowingAircraft();
}

void MapApiCalls::centerOnAircraft()
{
    if (mMap)
    {
        mMap->centerOnAircraft();
    }
}

bool MapApiCalls::goToAirport(const std::string& pIdent)
{
    if (!mMap)
    {
        return false;
    }
    return mMap->goToAirport(pIdent);
}

std::vector<prime::AirportMatch> MapApiCalls::findAirports(const std::string& pQuery, int pMax) const
{
    if (!mMap)
    {
        return {};
    }
    return mMap->findAirports(pQuery, pMax);
}

prime::LatLon MapApiCalls::screenToGeo(int pX, int pY) const
{
    if (!mMap)
    {
        return prime::LatLon();
    }
    return mMap->screenToGeo(pX, pY);
}

bool MapApiCalls::geoToScreen(double pLat, double pLon, int* pX, int* pY) const
{
    if (!mMap)
    {
        return false;
    }
    return mMap->geoToScreen(pLat, pLon, pX, pY);
}

void MapApiCalls::setMode(prime::MapMode pMode)
{
    if (mMap)
    {
        mMap->setMode(pMode);
    }
}

prime::MapMode MapApiCalls::getMode() const
{
    if (!mMap)
    {
        return prime::MapMode::Scenery;
    }
    return mMap->getMode();
}

void MapApiCalls::nextMode()
{
    if (mMap)
    {
        mMap->nextMode();
    }
}

void MapApiCalls::setLayerVisible(prime::MapLayer pLayer, bool pVisible)
{
    if (mMap)
    {
        mMap->setIsLayerVisible(pLayer, pVisible);
    }
}

bool MapApiCalls::isLayerVisible(prime::MapLayer pLayer) const
{
    if (!mMap)
    {
        return false;
    }
    return mMap->isLayerVisible(pLayer);
}

void MapApiCalls::toggleLayer(prime::MapLayer pLayer)
{
    if (mMap)
    {
        mMap->toggleLayer(pLayer);
    }
}

void MapApiCalls::setLayersVisible(std::initializer_list<prime::MapLayer> pLayers, bool pVisible)
{
    if (mMap)
    {
        mMap->setLayersVisible(pLayers, pVisible);
    }
}

void MapApiCalls::showOnlyLayers(std::initializer_list<prime::MapLayer> pLayers)
{
    if (mMap)
    {
        mMap->showOnlyLayers(pLayers);
    }
}

void MapApiCalls::resetLayersToDefaults()
{
    if (mMap)
    {
        mMap->resetLayersToDefaults();
    }
}

prime::LayerState MapApiCalls::getLayerState(prime::MapLayer pLayer) const
{
    if (!mMap)
    {
        return prime::LayerState::Off;
    }
    return mMap->getLayerState(pLayer);
}

void MapApiCalls::setDrawTool(prime::DrawTool pTool)
{
    if (mMap)
    {
        mMap->setDrawTool(pTool);
    }
}

prime::DrawTool MapApiCalls::getDrawTool() const
{
    if (!mMap)
    {
        return prime::DrawTool::Pan;
    }
    return mMap->getDrawTool();
}

bool MapApiCalls::addDrawingPoint(double pLat, double pLon)
{
    if (!mMap)
    {
        return false;
    }
    return mMap->addDrawingPoint(pLat, pLon);
}

bool MapApiCalls::finishDrawing()
{
    if (!mMap)
    {
        return false;
    }
    return mMap->finishDrawing();
}

bool MapApiCalls::undoDrawing()
{
    if (!mMap)
    {
        return false;
    }
    return mMap->undoDrawing();
}

void MapApiCalls::clearDrawings()
{
    if (mMap)
    {
        mMap->clearDrawings();
    }
}

bool MapApiCalls::hasUnfinishedDrawing() const
{
    if (!mMap)
    {
        return false;
    }
    return mMap->hasUnfinishedDrawing();
}

std::vector<prime::Shape> MapApiCalls::getShapes() const
{
    if (!mMap)
    {
        return {};
    }
    return mMap->getShapes();
}

bool MapApiCalls::removeShape(int pId)
{
    if (!mMap)
    {
        return false;
    }
    return mMap->removeShape(pId);
}

std::string MapApiCalls::exportDrawings() const
{
    if (!mMap)
    {
        return {};
    }
    return mMap->exportDrawings();
}

bool MapApiCalls::importDrawings(const std::string& pText)
{
    if (!mMap)
    {
        return false;
    }
    return mMap->importDrawings(pText);
}

void MapApiCalls::setAircraftSource(prime::DataSource pSource)
{
    if (mMap)
    {
        mMap->setAircraftSource(pSource);
    }
}

prime::DataSource MapApiCalls::getAircraftSource() const
{
    if (!mMap)
    {
        return prime::DataSource::XPlane;
    }
    return mMap->getAircraftSource();
}

void MapApiCalls::setAircraft(const prime::AircraftState& pState)
{
    if (mMap)
    {
        mMap->setAircraft(pState);
    }
}

prime::AircraftState MapApiCalls::getAircraft() const
{
    if (!mMap)
    {
        return prime::AircraftState();
    }
    return mMap->getAircraft();
}

void MapApiCalls::clearTrack()
{
    if (mMap)
    {
        mMap->clearTrack();
    }
}

void MapApiCalls::setRouteSource(prime::DataSource pSource)
{
    if (mMap)
    {
        mMap->setRouteSource(pSource);
    }
}

prime::DataSource MapApiCalls::getRouteSource() const
{
    if (!mMap)
    {
        return prime::DataSource::XPlane;
    }
    return mMap->getRouteSource();
}

void MapApiCalls::setRoute(const std::vector<prime::RouteWaypoint>& pWaypoints, int pActive)
{
    if (mMap)
    {
        mMap->setRoute(pWaypoints, pActive);
    }
}

std::vector<prime::MapObject> MapApiCalls::queryAt(double pLat, double pLon) const
{
    if (!mMap)
    {
        return {};
    }
    return mMap->queryAt(pLat, pLon);
}

std::vector<prime::MapObject> MapApiCalls::queryAtScreen(int pX, int pY) const
{
    if (!mMap)
    {
        return {};
    }
    return mMap->queryAtScreen(pX, pY);
}

std::string MapApiCalls::exportSettings() const
{
    if (!mMap)
    {
        return {};
    }
    return mMap->exportSettings();
}

bool MapApiCalls::importSettings(const std::string& pText)
{
    if (!mMap)
    {
        return false;
    }
    return mMap->importSettings(pText);
}

prime::TerrainSettings MapApiCalls::getTerrainSettings() const
{
    if (!mMap)
    {
        return prime::TerrainSettings{};
    }
    return mMap->getTerrainSettings();
}

void MapApiCalls::setTerrainSettings(const prime::TerrainSettings& pSettings)
{
    if (mMap)
    {
        mMap->setTerrainSettings(pSettings);
    }
}

std::string MapApiCalls::exportTerrainSettings() const
{
    if (!mMap)
    {
        return {};
    }
    return mMap->exportTerrainSettings();
}

bool MapApiCalls::importTerrainSettings(const std::string& pText)
{
    if (!mMap)
    {
        return false;
    }
    return mMap->importTerrainSettings(pText);
}

uint64_t MapApiCalls::getSettingsRevision() const
{
    if (!mMap)
    {
        return 0;
    }
    return mMap->getSettingsRevision();
}

uint64_t MapApiCalls::getDrawingsRevision() const
{
    if (!mMap)
    {
        return 0;
    }
    return mMap->getDrawingsRevision();
}

bool MapApiCalls::precacheVisibleArea()
{
    if (!mMap)
    {
        return false;
    }
    return mMap->precacheVisibleArea();
}

bool MapApiCalls::precacheArea(const prime::GeoBounds& pArea)
{
    if (!mMap)
    {
        return false;
    }
    return mMap->precacheArea(pArea);
}

void MapApiCalls::cancelPrecache()
{
    if (mMap)
    {
        mMap->cancelPrecache();
    }
}

void MapApiCalls::setDiagnostics(bool pOn)
{
    if (mMap)
    {
        mMap->setDiagnostics(pOn);
    }
}

bool MapApiCalls::isDiagnosticsOn() const
{
    if (!mMap)
    {
        return false;
    }
    return mMap->isDiagnosticsOn();
}
