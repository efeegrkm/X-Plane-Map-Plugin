#include "XMapApiCalls.h"

bool XMapApiCalls::initLibrary(const std::string& pLogPrefix, const std::string& pDataDirectory)
{
    if (!mManager)
    {
        mManager = prime::XMapManagerFactory::createXMapManager();
    }
    mManager->setLogPrefix(pLogPrefix);
    mManager->setDataDirectory(pDataDirectory);
    return mManager->initialize();
}

void XMapApiCalls::shutdownLibrary()
{
    mManager->shutdown();
}

bool XMapApiCalls::openXMap(const prime::Geometry& pGeometry, prime::WindowStyle pStyle, bool pUi, bool pMouse)
{
    if (!mManager)
    {
        return false;
    }
    prime::XMapConfig tConfig;
    tConfig.mName = pStyle == prime::WindowStyle::Texture ? "ui_test_texture" : "ui_test_standalone";
    tConfig.mGeometry = pGeometry;
    tConfig.mStyle = pStyle;
    tConfig.mUiVisible = pUi;
    tConfig.mMouseInput = pMouse;
    mXMap = mManager->addXMap(tConfig);
    if (mXMap)
    {
        mXMap->initialize();
    }
    return mXMap != nullptr;
}

bool XMapApiCalls::openSecondXMap()
{
    if (mSecondXMap)
    {
        if (mManager)
        {
            mManager->removeXMap(mSecondXMap);
        }
        mSecondXMap = nullptr;
        return false;
    }
    if (!mManager)
    {
        return false;
    }
    mSecondXMap = mManager->addXMap();
    if (!mSecondXMap)
    {
        return false;
    }
    mSecondXMap->setName("second");
    mSecondXMap->setStyle(prime::WindowStyle::Standalone);
    mSecondXMap->setTitle("Second map");
    mSecondXMap->setGeometry({900, 700, 500, 360});
    mSecondXMap->setMode(prime::XMapMode::Vfr);
    mSecondXMap->setIsUiVisible(true);
    mSecondXMap->setIsMouseInputEnabled(true);
    return mSecondXMap->initialize();
}

void XMapApiCalls::closeXMap()
{
    if (mSecondXMap && mManager)
    {
        mManager->removeXMap(mSecondXMap);
    }
    mSecondXMap = nullptr;
    if (mXMap && mManager)
    {
        mManager->removeXMap(mXMap);
    }
    mXMap = nullptr;
}

bool XMapApiCalls::isOpen() const
{
    return mXMap != nullptr;
}

bool XMapApiCalls::isInitialized() const
{
    return mManager && mManager->isInitialized();
}

prime::XMapManager* XMapApiCalls::manager() const
{
    return mManager;
}
prime::Status XMapApiCalls::getStatus() const
{
    if (!mManager)
    {
        return prime::Status{};
    }
    return mManager->getStatus();
}

std::string XMapApiCalls::versionString() const
{
    return prime::versionString();
}

double XMapApiCalls::minZoom() const
{
    return prime::XMap::minZoom();
}

double XMapApiCalls::maxZoom() const
{
    return prime::XMap::maxZoom();
}

std::optional<prime::XMapLayer> XMapApiCalls::parentOf(prime::XMapLayer pLayer) const
{
    if (!mManager)
    {
        return std::nullopt;
    }
    return mManager->parentOf(pLayer);
}

std::vector<prime::XMapLayer> XMapApiCalls::childrenOf(prime::XMapLayer pLayer) const
{
    if (!mManager)
    {
        return {};
    }
    return mManager->childrenOf(pLayer);
}

std::vector<prime::XMapLayer> XMapApiCalls::topLevelLayers() const
{
    if (!mManager)
    {
        return {};
    }
    return mManager->topLevelLayers();
}

prime::LayerKind XMapApiCalls::kindOf(prime::XMapLayer pLayer) const
{
    if (!mManager)
    {
        return prime::LayerKind::Layer;
    }
    return mManager->kindOf(pLayer);
}

std::string XMapApiCalls::layerName(prime::XMapLayer pLayer) const
{
    if (!mManager)
    {
        return {};
    }
    return mManager->layerName(pLayer);
}

std::string XMapApiCalls::layerKey(prime::XMapLayer pLayer) const
{
    if (!mManager)
    {
        return {};
    }
    return mManager->layerKey(pLayer);
}

std::optional<prime::XMapLayer> XMapApiCalls::layerFromKey(const std::string& pKey) const
{
    if (!mManager)
    {
        return std::nullopt;
    }
    return mManager->layerFromKey(pKey);
}

bool XMapApiCalls::isLayerDefaultVisible(prime::XMapLayer pLayer, prime::XMapMode pMode) const
{
    if (!mManager)
    {
        return false;
    }
    return mManager->isLayerDefaultVisible(pLayer, pMode);
}

void XMapApiCalls::setGeometry(const prime::Geometry& pGeometry)
{
    if (mXMap)
    {
        mXMap->setGeometry(pGeometry);
    }
}

prime::Geometry XMapApiCalls::getGeometry() const
{
    if (!mXMap)
    {
        return prime::Geometry();
    }
    return mXMap->getGeometry();
}

void XMapApiCalls::setVisible(bool pVisible)
{
    if (mXMap)
    {
        mXMap->setVisible(pVisible);
    }
}

bool XMapApiCalls::isVisible() const
{
    if (!mXMap)
    {
        return false;
    }
    return mXMap->isVisible();
}

void XMapApiCalls::bringToFront()
{
    if (mXMap)
    {
        mXMap->bringToFront();
    }
}

prime::WindowStyle XMapApiCalls::getStyle() const
{
    return mXMap ? mXMap->getStyle() : prime::WindowStyle::Texture;
}

bool XMapApiCalls::setTextureSize(int pWidth, int pHeight)
{
    return mXMap && mXMap->setTextureSize(pWidth, pHeight);
}

bool XMapApiCalls::renderTexture()
{
    return mXMap && mXMap->renderTexture();
}

unsigned int XMapApiCalls::getTextureId() const
{
    return mXMap ? mXMap->getTextureId() : 0u;
}

bool XMapApiCalls::injectMouse(int pX, int pY, prime::MouseAction pAction, prime::MouseButton pButton)
{
    return mXMap && mXMap->injectMouse(pX, pY, pAction, pButton);
}

void XMapApiCalls::injectMouseMove(int pX, int pY)
{
    if (mXMap)
    {
        mXMap->injectMouseMove(pX, pY);
    }
}

bool XMapApiCalls::injectWheel(int pX, int pY, int pClicks)
{
    return mXMap && mXMap->injectWheel(pX, pY, pClicks);
}

bool XMapApiCalls::injectKey(char pKey, int pFlags, char pVirtualKey)
{
    return mXMap && mXMap->injectKey(pKey, pFlags, pVirtualKey);
}

bool XMapApiCalls::wantsKeyboard() const
{
    return mXMap && mXMap->wantsKeyboard();
}

void XMapApiCalls::setPoppedOut(bool pPoppedOut)
{
    if (mXMap)
    {
        mXMap->setPoppedOut(pPoppedOut);
    }
}

bool XMapApiCalls::isPoppedOut() const
{
    if (!mXMap)
    {
        return false;
    }
    return mXMap->isPoppedOut();
}

void XMapApiCalls::setUiVisible(bool pVisible)
{
    if (mXMap)
    {
        mXMap->setIsUiVisible(pVisible);
    }
}

bool XMapApiCalls::isUiVisible() const
{
    if (!mXMap)
    {
        return false;
    }
    return mXMap->isUiVisible();
}

void XMapApiCalls::setMouseInput(bool pEnabled)
{
    if (mXMap)
    {
        mXMap->setIsMouseInputEnabled(pEnabled);
    }
}

bool XMapApiCalls::isMouseInputEnabled() const
{
    if (!mXMap)
    {
        return false;
    }
    return mXMap->isMouseInputEnabled();
}

void XMapApiCalls::setCenter(double pLat, double pLon)
{
    if (mXMap)
    {
        mXMap->setCenter(pLat, pLon);
    }
}

prime::LatLon XMapApiCalls::getCenter() const
{
    if (!mXMap)
    {
        return prime::LatLon();
    }
    return mXMap->getCenter();
}

void XMapApiCalls::setZoom(double pZoom)
{
    if (mXMap)
    {
        mXMap->setZoom(pZoom);
    }
}

double XMapApiCalls::getZoom() const
{
    if (!mXMap)
    {
        return 0.0;
    }
    return mXMap->getZoom();
}

void XMapApiCalls::zoomIn()
{
    if (mXMap)
    {
        mXMap->zoomIn();
    }
}

void XMapApiCalls::zoomOut()
{
    if (mXMap)
    {
        mXMap->zoomOut();
    }
}

void XMapApiCalls::setZoomStep(double pStep)
{
    if (mXMap)
    {
        mXMap->setZoomStep(pStep);
    }
}

void XMapApiCalls::panLeft()
{
    if (mXMap)
    {
        mXMap->panLeft();
    }
}

void XMapApiCalls::panRight()
{
    if (mXMap)
    {
        mXMap->panRight();
    }
}

void XMapApiCalls::panUp()
{
    if (mXMap)
    {
        mXMap->panUp();
    }
}

void XMapApiCalls::panDown()
{
    if (mXMap)
    {
        mXMap->panDown();
    }
}

void XMapApiCalls::setPanStep(double pFraction)
{
    if (mXMap)
    {
        mXMap->setPanStep(pFraction);
    }
}

void XMapApiCalls::panBy(double pDeltaX, double pDeltaY)
{
    if (mXMap)
    {
        mXMap->panBy(pDeltaX, pDeltaY);
    }
}

void XMapApiCalls::fitBounds(const prime::GeoBounds& pBounds)
{
    if (mXMap)
    {
        mXMap->fitBounds(pBounds);
    }
}

prime::GeoBounds XMapApiCalls::getVisibleBounds() const
{
    if (!mXMap)
    {
        return prime::GeoBounds();
    }
    return mXMap->getVisibleBounds();
}

double XMapApiCalls::getMetersPerBoxel() const
{
    if (!mXMap)
    {
        return 0.0;
    }
    return mXMap->getMetersPerBoxel();
}

void XMapApiCalls::setFollowAircraft(bool pFollow)
{
    if (mXMap)
    {
        mXMap->setFollowAircraft(pFollow);
    }
}

bool XMapApiCalls::isFollowingAircraft() const
{
    if (!mXMap)
    {
        return false;
    }
    return mXMap->isFollowingAircraft();
}

void XMapApiCalls::centerOnAircraft()
{
    if (mXMap)
    {
        mXMap->centerOnAircraft();
    }
}

bool XMapApiCalls::goToAirport(const std::string& pIdent)
{
    if (!mXMap)
    {
        return false;
    }
    return mXMap->goToAirport(pIdent);
}

std::vector<prime::AirportMatch> XMapApiCalls::findAirports(const std::string& pQuery, int pMax) const
{
    if (!mXMap)
    {
        return {};
    }
    return mXMap->findAirports(pQuery, pMax);
}

prime::LatLon XMapApiCalls::screenToGeo(int pX, int pY) const
{
    if (!mXMap)
    {
        return prime::LatLon();
    }
    return mXMap->screenToGeo(pX, pY);
}

bool XMapApiCalls::geoToScreen(double pLat, double pLon, int* pX, int* pY) const
{
    if (!mXMap)
    {
        return false;
    }
    return mXMap->geoToScreen(pLat, pLon, pX, pY);
}

void XMapApiCalls::setMode(prime::XMapMode pMode)
{
    if (mXMap)
    {
        mXMap->setMode(pMode);
    }
}

prime::XMapMode XMapApiCalls::getMode() const
{
    if (!mXMap)
    {
        return prime::XMapMode::Scenery;
    }
    return mXMap->getMode();
}

void XMapApiCalls::nextMode()
{
    if (mXMap)
    {
        mXMap->nextMode();
    }
}

void XMapApiCalls::setLayerVisible(prime::XMapLayer pLayer, bool pVisible)
{
    if (mXMap)
    {
        mXMap->setIsLayerVisible(pLayer, pVisible);
    }
}

bool XMapApiCalls::isLayerVisible(prime::XMapLayer pLayer) const
{
    if (!mXMap)
    {
        return false;
    }
    return mXMap->isLayerVisible(pLayer);
}

void XMapApiCalls::toggleLayer(prime::XMapLayer pLayer)
{
    if (mXMap)
    {
        mXMap->toggleLayer(pLayer);
    }
}

void XMapApiCalls::setLayersVisible(std::initializer_list<prime::XMapLayer> pLayers, bool pVisible)
{
    if (mXMap)
    {
        mXMap->setLayersVisible(pLayers, pVisible);
    }
}

void XMapApiCalls::showOnlyLayers(std::initializer_list<prime::XMapLayer> pLayers)
{
    if (mXMap)
    {
        mXMap->showOnlyLayers(pLayers);
    }
}

void XMapApiCalls::resetLayersToDefaults()
{
    if (mXMap)
    {
        mXMap->resetLayersToDefaults();
    }
}

prime::LayerState XMapApiCalls::getLayerState(prime::XMapLayer pLayer) const
{
    if (!mXMap)
    {
        return prime::LayerState::Off;
    }
    return mXMap->getLayerState(pLayer);
}

void XMapApiCalls::setDrawTool(prime::DrawTool pTool)
{
    if (mXMap)
    {
        mXMap->setDrawTool(pTool);
    }
}

prime::DrawTool XMapApiCalls::getDrawTool() const
{
    if (!mXMap)
    {
        return prime::DrawTool::Pan;
    }
    return mXMap->getDrawTool();
}

bool XMapApiCalls::addDrawingPoint(double pLat, double pLon)
{
    if (!mXMap)
    {
        return false;
    }
    return mXMap->addDrawingPoint(pLat, pLon);
}

bool XMapApiCalls::finishDrawing()
{
    if (!mXMap)
    {
        return false;
    }
    return mXMap->finishDrawing();
}

bool XMapApiCalls::undoDrawing()
{
    if (!mXMap)
    {
        return false;
    }
    return mXMap->undoDrawing();
}

void XMapApiCalls::clearDrawings()
{
    if (mXMap)
    {
        mXMap->clearDrawings();
    }
}

bool XMapApiCalls::hasUnfinishedDrawing() const
{
    if (!mXMap)
    {
        return false;
    }
    return mXMap->hasUnfinishedDrawing();
}

std::vector<prime::Shape> XMapApiCalls::getShapes() const
{
    if (!mXMap)
    {
        return {};
    }
    return mXMap->getShapes();
}

bool XMapApiCalls::removeShape(int pId)
{
    if (!mXMap)
    {
        return false;
    }
    return mXMap->removeShape(pId);
}

std::string XMapApiCalls::exportDrawings() const
{
    if (!mXMap)
    {
        return {};
    }
    return mXMap->exportDrawings();
}

bool XMapApiCalls::importDrawings(const std::string& pText)
{
    if (!mXMap)
    {
        return false;
    }
    return mXMap->importDrawings(pText);
}

void XMapApiCalls::setAircraftSource(prime::DataSource pSource)
{
    if (mXMap)
    {
        mXMap->setAircraftSource(pSource);
    }
}

prime::DataSource XMapApiCalls::getAircraftSource() const
{
    if (!mXMap)
    {
        return prime::DataSource::XPlane;
    }
    return mXMap->getAircraftSource();
}

void XMapApiCalls::setAircraft(const prime::AircraftState& pState)
{
    if (mXMap)
    {
        mXMap->setAircraft(pState);
    }
}

prime::AircraftState XMapApiCalls::getAircraft() const
{
    if (!mXMap)
    {
        return prime::AircraftState();
    }
    return mXMap->getAircraft();
}

void XMapApiCalls::clearTrack()
{
    if (mXMap)
    {
        mXMap->clearTrack();
    }
}

void XMapApiCalls::setRouteSource(prime::DataSource pSource)
{
    if (mXMap)
    {
        mXMap->setRouteSource(pSource);
    }
}

prime::DataSource XMapApiCalls::getRouteSource() const
{
    if (!mXMap)
    {
        return prime::DataSource::XPlane;
    }
    return mXMap->getRouteSource();
}

void XMapApiCalls::setRoute(const std::vector<prime::RouteWaypoint>& pWaypoints, int pActive)
{
    if (mXMap)
    {
        mXMap->setRoute(pWaypoints, pActive);
    }
}

std::vector<prime::XMapObject> XMapApiCalls::queryAt(double pLat, double pLon) const
{
    if (!mXMap)
    {
        return {};
    }
    return mXMap->queryAt(pLat, pLon);
}

std::vector<prime::XMapObject> XMapApiCalls::queryAtScreen(int pX, int pY) const
{
    if (!mXMap)
    {
        return {};
    }
    return mXMap->queryAtScreen(pX, pY);
}

std::string XMapApiCalls::exportSettings() const
{
    if (!mXMap)
    {
        return {};
    }
    return mXMap->exportSettings();
}

bool XMapApiCalls::importSettings(const std::string& pText)
{
    if (!mXMap)
    {
        return false;
    }
    return mXMap->importSettings(pText);
}

prime::TerrainSettings XMapApiCalls::getTerrainSettings() const
{
    if (!mXMap)
    {
        return prime::TerrainSettings{};
    }
    return mXMap->getTerrainSettings();
}

void XMapApiCalls::setTerrainSettings(const prime::TerrainSettings& pSettings)
{
    if (mXMap)
    {
        mXMap->setTerrainSettings(pSettings);
    }
}

std::string XMapApiCalls::exportTerrainSettings() const
{
    if (!mXMap)
    {
        return {};
    }
    return mXMap->exportTerrainSettings();
}

bool XMapApiCalls::importTerrainSettings(const std::string& pText)
{
    if (!mXMap)
    {
        return false;
    }
    return mXMap->importTerrainSettings(pText);
}

uint64_t XMapApiCalls::getSettingsRevision() const
{
    if (!mXMap)
    {
        return 0;
    }
    return mXMap->getSettingsRevision();
}

uint64_t XMapApiCalls::getDrawingsRevision() const
{
    if (!mXMap)
    {
        return 0;
    }
    return mXMap->getDrawingsRevision();
}

bool XMapApiCalls::precacheVisibleArea()
{
    if (!mXMap)
    {
        return false;
    }
    return mXMap->precacheVisibleArea();
}

bool XMapApiCalls::precacheArea(const prime::GeoBounds& pArea)
{
    if (!mXMap)
    {
        return false;
    }
    return mXMap->precacheArea(pArea);
}

void XMapApiCalls::cancelPrecache()
{
    if (mXMap)
    {
        mXMap->cancelPrecache();
    }
}

void XMapApiCalls::setDiagnostics(bool pOn)
{
    if (mXMap)
    {
        mXMap->setDiagnostics(pOn);
    }
}

bool XMapApiCalls::isDiagnosticsOn() const
{
    if (!mXMap)
    {
        return false;
    }
    return mXMap->isDiagnosticsOn();
}
