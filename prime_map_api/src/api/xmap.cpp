// xmap.cpp: prime::XMap -> XMapInstance koprusu. XMapManager yaratır ve yok eder.
#include "PrimeMap.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <sstream>
#include <vector>

#include "Drawings.h"
#include "LayerManager.h"
#include "Log.h"
#include "XMapApiUtil.h"
#include "XMapManagerPrivate.h"
#include "MapTheme.h"
#include "MapView.h"
#include "MapWindow.h"
#include "ScreenMath.h"
#include "Settings.h"
#include "data/AirportGeometry.h"
#include "data/AirportSearch.h"
#include "data/TerrainClasses.h"
#include "engine/LayerTable.h"
#include "engine/MapData.h"
#include "engine/MapInstance.h"
#include "layers/FmsLayer.h"
#include "layers/TrackLayer.h"


namespace
{

const int kShapeIdBase = 1000000;  // Shape kimligi: tur * kShapeIdBase + etiket numarasi

RectI toRect(const prime::Geometry& pGeometry)
{
    RectI tRect;
    tRect.mLeft = pGeometry.mLeft;
    tRect.mTop = pGeometry.mTop;
    tRect.mRight = pGeometry.mLeft + pGeometry.mWidth;
    tRect.mBottom = pGeometry.mTop + pGeometry.mHeight;
    return tRect;
}

XMapWindow* windowOf(const std::unique_ptr<XMapInstance>& pInstance)
{
    return pInstance ? &pInstance->window() : nullptr;
}

}  // namespace

namespace prime
{

XMap::XMap() = default;

XMap::~XMap()
{
    if (mManager)
    {
        // yonetici listesinden dus; harita kapanir (XMapInstance yikicisi ayarlari yazar, pencereyi kapatir)
        auto& tMaps = mManager->mPrivate->mMaps;
        tMaps.erase(std::remove(tMaps.begin(), tMaps.end(), this), tMaps.end());
    }
    mInstance.reset();
}

int XMap::getId() const
{
    return mInstance.get() ? mInstance->id() : 0;
}

bool XMap::setName(const std::string& pName)
{
    XMapInstance* tInstance = mInstance.get();
    if (!tInstance)
    {
        return false;
    }
    if (tInstance->isOpen())
    {
        log("WARNING: setName: map {} is open; close it first (the name selects the settings folder)",
            tInstance->id());
        return false;
    }
    // Ayni klasoru iki harita kullanmasin (birbirinin dosyasini ezerlerdi)
    const std::string tFolder = XMapInstance::safeFolderName(pName);
    for (const XMap* tOther : mManager->mPrivate->mMaps)
    {
        if (tOther != this && tOther->mInstance.get() && !tFolder.empty() &&
            tOther->mInstance->folderName() == tFolder)
        {
            log("WARNING: setName: '{}' is already used by map {}", pName, tOther->getId());
            return false;
        }
    }
    tInstance->setName(pName);
    return true;
}

std::string XMap::getName() const
{
    return mInstance.get() ? mInstance->name() : std::string();
}

std::string XMap::getSettingsDirectory() const
{
    return mInstance.get() ? mInstance->directory() : std::string();
}

void XMap::setPersistence(Persistence pPersistence)
{
    if (XMapInstance* tInstance = mInstance.get())
    {
        tInstance->setPersistence(static_cast<MapPersistence>(pPersistence));
    }
}

Persistence XMap::getPersistence() const
{
    return mInstance.get() ? static_cast<Persistence>(mInstance->persistence()) : Persistence::Off;
}

bool XMap::saveSettings()
{
    return mInstance.get() && mInstance->saveNow();
}

bool XMap::initialize()
{
    return mInstance.get() && mInstance->open();
}

void XMap::close()
{
    if (XMapInstance* tInstance = mInstance.get())
    {
        tInstance->close();
    }
}

bool XMap::isInitialized() const
{
    return mInstance.get() && mInstance->isOpen();
}

void XMap::setStyle(WindowStyle pStyle)
{
    if (XMapWindow* tWindow = windowOf(mInstance))
    {
        tWindow->setEmbedded(pStyle == WindowStyle::Embedded);  // acik pencerede uyari loglar, degistirmez
    }
}

WindowStyle XMap::getStyle() const
{
    XMapWindow* tWindow = windowOf(mInstance);
    return (tWindow && !tWindow->isEmbedded()) ? WindowStyle::Standalone : WindowStyle::Embedded;
}

void XMap::setTitle(const std::string& pTitle)
{
    if (XMapWindow* tWindow = windowOf(mInstance))
    {
        tWindow->setTitle(pTitle);
    }
}

std::string XMap::getTitle() const
{
    XMapWindow* tWindow = windowOf(mInstance);
    return tWindow ? tWindow->title() : std::string();
}

void XMap::setGeometry(const Geometry& pGeometry)
{
    if (XMapWindow* tWindow = windowOf(mInstance))
    {
        tWindow->setGeometry(toRect(pGeometry));
    }
}

Geometry XMap::getGeometry() const
{
    Geometry tGeometry;
    if (XMapWindow* tWindow = windowOf(mInstance))
    {
        const RectI tRect = tWindow->getGeometry();
        tGeometry.mLeft = tRect.mLeft;
        tGeometry.mTop = tRect.mTop;
        tGeometry.mWidth = tRect.width();
        tGeometry.mHeight = tRect.height();
    }
    return tGeometry;
}

void XMap::setVisible(bool pVisible)
{
    if (XMapWindow* tWindow = windowOf(mInstance))
    {
        tWindow->setVisible(pVisible);
    }
}

bool XMap::isVisible() const
{
    XMapWindow* tWindow = windowOf(mInstance);
    return tWindow && tWindow->isVisible();
}

void XMap::bringToFront()
{
    if (XMapWindow* tWindow = windowOf(mInstance))
    {
        tWindow->bringToFront();
    }
}

void XMap::setPoppedOut(bool pPoppedOut)
{
    if (XMapWindow* tWindow = windowOf(mInstance))
    {
        tWindow->setPoppedOut(pPoppedOut);
    }
}

bool XMap::isPoppedOut() const
{
    XMapWindow* tWindow = windowOf(mInstance);
    return tWindow && tWindow->isPoppedOut();
}

void XMap::setIsUiVisible(bool pVisible)
{
    if (XMapWindow* tWindow = windowOf(mInstance))
    {
        tWindow->setUiVisible(pVisible);
    }
}

bool XMap::isUiVisible() const
{
    XMapWindow* tWindow = windowOf(mInstance);
    return tWindow && tWindow->isUiVisible();
}

void XMap::setIsMouseInputEnabled(bool pEnabled)
{
    if (XMapWindow* tWindow = windowOf(mInstance))
    {
        tWindow->setMouseInput(pEnabled);
    }
}

bool XMap::isMouseInputEnabled() const
{
    XMapWindow* tWindow = windowOf(mInstance);
    return tWindow && tWindow->isMouseInputEnabled();
}

void XMap::setCenter(double pLat, double pLon)
{
    if (XMapWindow* tWindow = windowOf(mInstance))
    {
        tWindow->setFollowAircraft(false);  // takip acik kalirsa harita hemen ucaga doner
        tWindow->view().setCenter({pLat, pLon});
    }
}

LatLon XMap::getCenter() const
{
    LatLon tCenter;
    if (XMapWindow* tWindow = windowOf(mInstance))
    {
        const ::LatLon tInner = tWindow->view().center();
        tCenter.mLat = tInner.mLat;
        tCenter.mLon = tInner.mLon;
    }
    return tCenter;
}

void XMap::setZoom(double pZoom)
{
    if (XMapWindow* tWindow = windowOf(mInstance))
    {
        tWindow->view().setZoom(pZoom);
    }
}

double XMap::getZoom() const
{
    XMapWindow* tWindow = windowOf(mInstance);
    return tWindow ? tWindow->view().zoom() : 0.0;
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
    XMapWindow* tWindow = windowOf(mInstance);
    if (!tWindow)
    {
        return false;
    }
    if (!(pMinZoom <= pMaxZoom))
    {
        log("WARNING: setZoomLimits: min {} is greater than max {}", pMinZoom, pMaxZoom);
        return false;
    }
    tWindow->setZoomLimits(pMinZoom, pMaxZoom);
    return true;
}

double XMap::getMinZoomLimit() const
{
    XMapWindow* tWindow = windowOf(mInstance);
    return tWindow ? tWindow->view().minZoomLimit() : XMapView::kMinZoom;
}

double XMap::getMaxZoomLimit() const
{
    XMapWindow* tWindow = windowOf(mInstance);
    return tWindow ? tWindow->view().maxZoomLimit() : XMapView::kMaxZoom;
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
    XMapWindow* tWindow = windowOf(mInstance);
    if (tWindow && pStep > 0.0 && pStep <= 5.0)
    {
        tWindow->setZoomStep(pStep);
    }
    else
    {
        log("WARNING: setZoomStep: {} out of range (0, 5]", pStep);
    }
}

double XMap::getZoomStep() const
{
    XMapWindow* tWindow = windowOf(mInstance);
    return tWindow ? tWindow->zoomStep() : 0.5;
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
    XMapWindow* tWindow = windowOf(mInstance);
    if (tWindow && pFractionOfView > 0.0 && pFractionOfView <= 1.0)
    {
        tWindow->setPanStep(pFractionOfView);
    }
    else
    {
        log("WARNING: setPanStep: {} out of range (0, 1]", pFractionOfView);
    }
}

double XMap::getPanStep() const
{
    XMapWindow* tWindow = windowOf(mInstance);
    return tWindow ? tWindow->panStep() : 0.25;
}

void XMap::panBy(double pDeltaX, double pDeltaY)
{
    if (XMapWindow* tWindow = windowOf(mInstance))
    {
        tWindow->setFollowAircraft(false);  // elle kaydirma gibi: takip biter
        tWindow->view().panByBoxels(pDeltaX, pDeltaY);
    }
}

void XMap::fitBounds(const GeoBounds& pBounds)
{
    XMapWindow* tWindow = windowOf(mInstance);
    if (!tWindow || pBounds.mNorth <= pBounds.mSouth || pBounds.mEast <= pBounds.mWest)
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
    tWindow->setFollowAircraft(false);
    tWindow->view().setCenterMercator(std::fmod(tX0 + tWidth * 0.5, 1.0), (tY0 + tY1) * 0.5);
    tWindow->view().setZoom(std::log2(std::max(tScale, 1e-9)));
}

GeoBounds XMap::getVisibleBounds() const
{
    GeoBounds tBounds;
    if (XMapWindow* tWindow = windowOf(mInstance))
    {
        const ::GeoBounds tInner = tWindow->view().visibleBounds();
        tBounds.mSouth = tInner.mSouth;
        tBounds.mWest = tInner.mWest;
        tBounds.mNorth = tInner.mNorth;
        tBounds.mEast = tInner.mEast;
    }
    return tBounds;
}

double XMap::getMetersPerBoxel() const
{
    XMapWindow* tWindow = windowOf(mInstance);
    return tWindow ? tWindow->view().metersPerBoxel() : 0.0;
}

void XMap::setFollowAircraft(bool pFollow)
{
    if (XMapWindow* tWindow = windowOf(mInstance))
    {
        tWindow->setFollowAircraft(pFollow);
    }
}

bool XMap::isFollowingAircraft() const
{
    XMapWindow* tWindow = windowOf(mInstance);
    return tWindow && tWindow->isFollowingAircraft();
}

void XMap::centerOnAircraft()
{
    if (XMapWindow* tWindow = windowOf(mInstance))
    {
        tWindow->centerOnAircraft();
    }
}

bool XMap::goToAirport(const std::string& pIdent)
{
    XMapWindow* tWindow = windowOf(mInstance);
    if (!tWindow)
    {
        return false;
    }
    const AirportGeometryStore& tAirports = mInstance->data().airports();
    const std::string tWanted = api::upper(pIdent);
    for (int tIndex : ::findAirports(tAirports, pIdent, 8))
    {
        if (api::upper(tAirports.at(tIndex).mId) == tWanted)
        {
            tWindow->goToAirport(tIndex);
            return true;
        }
    }
    log("go to airport: '{}' not found", pIdent);
    return false;
}

std::vector<AirportMatch> XMap::findAirports(const std::string& pQuery, int pMaxResults) const
{
    std::vector<AirportMatch> tMatches;
    if (!mInstance.get())
    {
        return tMatches;
    }
    const AirportGeometryStore& tAirports = mInstance->data().airports();
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
    if (XMapWindow* tWindow = windowOf(mInstance))
    {
        const ::LatLon tInner = tWindow->view().boxelToLatLon(pX, pY);
        tPoint.mLat = tInner.mLat;
        tPoint.mLon = tInner.mLon;
    }
    return tPoint;
}

bool XMap::geoToScreen(double pLat, double pLon, int* pX, int* pY) const
{
    XMapWindow* tWindow = windowOf(mInstance);
    if (!tWindow || !pX || !pY)
    {
        return false;
    }
    double tX = 0.0;
    double tY = 0.0;
    tWindow->view().latLonToBoxel({pLat, pLon}, &tX, &tY);
    *pX = static_cast<int>(std::lround(tX));
    *pY = static_cast<int>(std::lround(tY));
    const RectD& tArea = tWindow->view().viewport();
    return tX >= tArea.mLeft && tX <= tArea.mRight && tY >= tArea.mBottom && tY <= tArea.mTop;
}

void XMap::setMode(MapMode pMode)
{
    if (XMapWindow* tWindow = windowOf(mInstance))
    {
        tWindow->setMapMode(static_cast<::MapMode>(pMode));
    }
}

MapMode XMap::getMode() const
{
    XMapWindow* tWindow = windowOf(mInstance);
    return tWindow ? static_cast<MapMode>(tWindow->getMapMode()) : MapMode::Scenery;
}

void XMap::nextMode()
{
    if (XMapWindow* tWindow = windowOf(mInstance))
    {
        tWindow->nextMapMode();
    }
}

void XMap::setIsLayerVisible(MapLayer pLayer, bool pVisible)
{
    XMapWindow* tWindow = windowOf(mInstance);
    if (tWindow &&
        LayerRules::setVisible(mInstance->layers(), themeFor(tWindow->getMapMode()), pLayer, pVisible))
    {
        tWindow->saveSettings();
    }
}

bool XMap::isLayerVisible(MapLayer pLayer) const
{
    return mInstance.get() && LayerRules::isVisible(mInstance->layers(), pLayer);
}

void XMap::toggleLayer(MapLayer pLayer)
{
    XMapWindow* tWindow = windowOf(mInstance);
    if (tWindow && LayerRules::toggle(mInstance->layers(), themeFor(tWindow->getMapMode()), pLayer))
    {
        tWindow->saveSettings();
    }
}

void XMap::setLayersVisible(std::initializer_list<MapLayer> pLayers, bool pVisible)
{
    for (MapLayer tLayer : pLayers)
    {
        setIsLayerVisible(tLayer, pVisible);
    }
}

void XMap::showOnlyLayers(std::initializer_list<MapLayer> pLayers)
{
    if (XMapWindow* tWindow = windowOf(mInstance))
    {
        LayerRules::showOnly(mInstance->layers(), themeFor(tWindow->getMapMode()), pLayers.begin(),
                             pLayers.end());
        tWindow->saveSettings();
    }
}

void XMap::resetLayersToDefaults()
{
    if (XMapWindow* tWindow = windowOf(mInstance))
    {
        tWindow->resetLayers();
    }
}

LayerState XMap::getLayerState(MapLayer pLayer) const
{
    return mInstance.get() ? LayerRules::state(mInstance->layers(), pLayer) : LayerState::Off;
}

bool XMap::setLayerZoomRange(MapLayer pLayer, double pMinZoom, double pMaxZoom)
{
    XMapWindow* tWindow = windowOf(mInstance);
    if (!tWindow)
    {
        return false;
    }
    if (!(pMinZoom <= pMaxZoom))
    {
        log("WARNING: setLayerZoomRange: min {} is greater than max {}", pMinZoom, pMaxZoom);
        return false;
    }
    LayerManager& tLayers = mInstance->layers();
    bool tApplied = false;
    if (LayerRules::kindOf(pLayer) == LayerKind::Group)
    {
        for (MapLayer tChild : LayerRules::childrenOf(pLayer))
        {
            if (Layer* tLayer = LayerRules::findLayer(tLayers, tChild))
            {
                tLayer->setZoomRange(pMinZoom, pMaxZoom);
                tApplied = true;
            }
        }
    }
    else if (Layer* tLayer = LayerRules::findLayer(tLayers, pLayer))
    {
        const int tSub = LayerRules::subLayerIndex(tLayers, pLayer);
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
        tWindow->saveSettings();
    }
    return tApplied;
}

void XMap::getLayerZoomRange(MapLayer pLayer, double* pMinZoom, double* pMaxZoom) const
{
    double tMin = Layer::kNoMinZoom;
    double tMax = Layer::kNoMaxZoom;
    if (mInstance.get())
    {
        const LayerManager& tLayers = mInstance->layers();
        MapLayer tItem = pLayer;
        if (LayerRules::kindOf(pLayer) == LayerKind::Group)
        {
            const std::vector<MapLayer> tChildren = LayerRules::childrenOf(pLayer);
            tItem = tChildren.empty() ? pLayer : tChildren.front();
        }
        if (const Layer* tLayer = LayerRules::findLayer(tLayers, tItem))
        {
            const int tSub = LayerRules::subLayerIndex(tLayers, tItem);
            tMin = tSub >= 0 ? tLayer->subLayerAt(tSub).mMinZoom : tLayer->minZoom();
            tMax = tSub >= 0 ? tLayer->subLayerAt(tSub).mMaxZoom : tLayer->maxZoom();
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

void XMap::clearLayerZoomRange(MapLayer pLayer)
{
    setLayerZoomRange(pLayer, Layer::kNoMinZoom, Layer::kNoMaxZoom);
}

void XMap::clearAllLayerZoomRanges()
{
    if (XMapWindow* tWindow = windowOf(mInstance))
    {
        mInstance->layers().resetZoomRanges();
        tWindow->saveSettings();
    }
}

void XMap::setDrawTool(DrawTool pTool)
{
    if (XMapWindow* tWindow = windowOf(mInstance))
    {
        mInstance->userDrawings().setMode(static_cast<Drawings::Mode>(pTool));  // yarim cizim biter
        tWindow->drawingsChanged();
    }
}

DrawTool XMap::getDrawTool() const
{
    return mInstance.get() ? static_cast<DrawTool>(mInstance->userDrawings().getMode()) : DrawTool::Pan;
}

bool XMap::addDrawingPoint(double pLat, double pLon)
{
    XMapWindow* tWindow = windowOf(mInstance);
    if (!tWindow || mInstance->userDrawings().getMode() == Drawings::Mode::Pan)
    {
        return false;
    }
    mInstance->userDrawings().addVertex({pLat, pLon});
    tWindow->drawingsChanged();
    return true;
}

bool XMap::finishDrawing()
{
    XMapWindow* tWindow = windowOf(mInstance);
    if (!tWindow || !mInstance->userDrawings().current())
    {
        return false;
    }
    mInstance->userDrawings().finish();
    tWindow->drawingsChanged();
    return true;
}

bool XMap::undoDrawing()
{
    XMapWindow* tWindow = windowOf(mInstance);
    if (!tWindow)
    {
        return false;
    }
    Drawings& tDrawings = mInstance->userDrawings();
    if (!tDrawings.current() && tDrawings.shapeCount() == 0)
    {
        return false;
    }
    tDrawings.undo();
    tWindow->drawingsChanged();
    return true;
}

void XMap::clearDrawings()
{
    if (XMapWindow* tWindow = windowOf(mInstance))
    {
        mInstance->userDrawings().clear();
        tWindow->drawingsChanged();
    }
}

bool XMap::hasUnfinishedDrawing() const
{
    return mInstance.get() && mInstance->userDrawings().current() != nullptr;
}

std::vector<Shape> XMap::getShapes() const
{
    std::vector<Shape> tShapes;
    if (!mInstance.get())
    {
        return tShapes;
    }
    for (const Drawings::Shape& tInner : mInstance->userDrawings().shapes())
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
    XMapWindow* tWindow = windowOf(mInstance);
    const int tKind = pId / kShapeIdBase;
    if (!tWindow || tKind < 0 || tKind > 2 ||
        !mInstance->userDrawings().remove(static_cast<Drawings::ShapeKind>(tKind), pId % kShapeIdBase))
    {
        log("WARNING: removeShape: no shape with id {}", pId);
        return false;
    }
    tWindow->drawingsChanged();
    return true;
}

std::string XMap::exportDrawings() const
{
    return mInstance.get() ? mInstance->userDrawings().serialize() : std::string();
}

bool XMap::importDrawings(const std::string& pText)
{
    XMapWindow* tWindow = windowOf(mInstance);
    if (!tWindow)
    {
        return false;
    }
    const int tCount = mInstance->userDrawings().deserialize(pText);
    tWindow->drawingsChanged();
    log("drawings imported: {} shapes", tCount);
    return tCount > 0 || pText.find_first_not_of(" \t\r\n") == std::string::npos;
}

void XMap::setAircraftSource(DataSource pSource)
{
    if (XMapInstance* tInstance = mInstance.get())
    {
        tInstance->aircraft().setFromHost(pSource == DataSource::Host);
        tInstance->markChanged();
    }
}

DataSource XMap::getAircraftSource() const
{
    return (mInstance.get() && mInstance->aircraft().isFromHost()) ? DataSource::Host : DataSource::XPlane;
}

void XMap::setAircraft(const AircraftState& pState)
{
    if (!mInstance.get())
    {
        return;
    }
    ::AircraftState tInner;
    tInner.mValid = pState.mValid;
    tInner.mPosition = {pState.mPosition.mLat, pState.mPosition.mLon};
    tInner.mHeadingDeg = pState.mHeadingDeg;
    tInner.mGroundSpeedKt = pState.mGroundSpeedKt;
    tInner.mAltitudeFt = pState.mAltitudeFt;
    mInstance->aircraft().setHostState(tInner);
}

AircraftState XMap::getAircraft() const
{
    AircraftState tState;
    if (mInstance.get())
    {
        const ::AircraftState& tInner = mInstance->aircraft().state();
        tState.mValid = tInner.mValid;
        tState.mPosition = {tInner.mPosition.mLat, tInner.mPosition.mLon};
        tState.mHeadingDeg = tInner.mHeadingDeg;
        tState.mGroundSpeedKt = tInner.mGroundSpeedKt;
        tState.mAltitudeFt = tInner.mAltitudeFt;
    }
    return tState;
}

void XMap::clearTrack()
{
    if (mInstance.get() && mInstance->track())
    {
        mInstance->track()->clear();
        log("flight track cleared (map {})", mInstance->id());
    }
}

void XMap::setRouteSource(DataSource pSource)
{
    if (mInstance.get() && mInstance->fms())
    {
        mInstance->fms()->setFromHost(pSource == DataSource::Host);
        mInstance->markChanged();
    }
}

DataSource XMap::getRouteSource() const
{
    return (mInstance.get() && mInstance->fms() && mInstance->fms()->isFromHost())
               ? DataSource::Host
               : DataSource::XPlane;
}

void XMap::setRoute(const std::vector<RouteWaypoint>& pWaypoints, int pActiveIndex)
{
    if (!mInstance.get() || !mInstance->fms())
    {
        return;
    }
    if (!mInstance->fms()->isFromHost())
    {
        log("WARNING: setRoute ignored: route source is X-plane(call setRouteSource(DataSource::Host))");
        return;
    }
    std::vector<FmsLayer::Waypoint> tInner;
    for (const RouteWaypoint& tPoint : pWaypoints)
    {
        tInner.push_back({{tPoint.mPosition.mLat, tPoint.mPosition.mLon}, tPoint.mIdent, tPoint.mAltitudeFt});
    }
    mInstance->fms()->setHostRoute(tInner, pActiveIndex);
}

std::vector<MapObject> XMap::queryAt(double pLat, double pLon) const
{
    int tX = 0;
    int tY = 0;
    if (!geoToScreen(pLat, pLon, &tX, &tY))
    {
        return {};
    }
    return queryAtScreen(tX, tY);
}

std::vector<MapObject> XMap::queryAtScreen(int pX, int pY) const
{
    std::vector<MapObject> tObjects;
    XMapWindow* tWindow = windowOf(mInstance);
    if (!tWindow || !tWindow->isCreated())
    {
        return tObjects;  // sorgu son cizilen kareye gore: pencere acik olmali
    }
    for (const auto& [tId, tText] : tWindow->describeAt(pX, pY))
    {
        MapObject tObject;
        tObject.mLayer = LayerRules::layerFromKey(tId).value_or(MapLayer::Count);
        tObject.mText = tText;
        tObjects.push_back(tObject);
    }
    return tObjects;
}

void XMap::setImageCache(bool pEnabled)
{
    if (XMapWindow* tWindow = windowOf(mInstance))
    {
        tWindow->setImageCacheEnabled(pEnabled);
    }
}

bool XMap::isImageCacheEnabled() const
{
    XMapWindow* tWindow = windowOf(mInstance);
    return tWindow && tWindow->isImageCacheEnabled();
}

void XMap::setMaxRenderFps(int pFps)
{
    if (XMapWindow* tWindow = windowOf(mInstance))
    {
        tWindow->setMaxRenderFps(pFps);
    }
}

int XMap::getMaxRenderFps() const
{
    XMapWindow* tWindow = windowOf(mInstance);
    return tWindow ? tWindow->maxRenderFps() : 0;
}

void XMap::setXPlaneFont(bool pXPlaneFont)
{
    if (XMapWindow* tWindow = windowOf(mInstance))
    {
        tWindow->setXPlaneFont(pXPlaneFont);
    }
}

bool XMap::isXPlaneFont() const
{
    XMapWindow* tWindow = windowOf(mInstance);
    return tWindow && tWindow->usesXPlaneFont();
}

std::string XMap::exportSettings() const
{
    return mInstance.get() ? mInstance->exportSettings() : std::string();
}

bool XMap::importSettings(const std::string& pText)
{
    if (!mInstance.get())
    {
        return false;
    }
    mInstance->importSettings(pText);
    return true;
}

TerrainSettings XMap::getTerrainSettings() const
{
    TerrainSettings tOut;
    if (!mInstance.get())
    {
        return tOut;
    }
    Settings tIni;
    mInstance->palette().save(tIni);
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
    if (!mInstance.get())
    {
        return;
    }
    Settings tIni;
    mInstance->palette().save(tIni);  // [palette] version ve bilinmeyen anahtarlar korunur
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
    mInstance->importTerrainSettings(tIni.toText({}));
}

std::string XMap::exportTerrainSettings() const
{
    return mInstance.get() ? mInstance->exportTerrainSettings() : std::string();
}

bool XMap::importTerrainSettings(const std::string& pText)
{
    if (!mInstance.get())
    {
        return false;
    }
    mInstance->importTerrainSettings(pText);
    return true;
}

uint64_t XMap::getSettingsRevision() const
{
    XMapWindow* tWindow = windowOf(mInstance);
    return tWindow ? tWindow->settingsRevision() : 0;
}

uint64_t XMap::getDrawingsRevision() const
{
    XMapWindow* tWindow = windowOf(mInstance);
    return tWindow ? tWindow->drawingsRevision() : 0;
}

std::string XMap::getStatusText() const
{
    return mInstance.get() ? mInstance->statusText() : std::string();
}

int XMap::getTerrainPendingImages() const
{
    return mInstance.get() ? mInstance->pendingTerrainImages() : 0;
}

bool XMap::precacheVisibleArea()
{
    return precacheArea(getVisibleBounds());
}

bool XMap::precacheArea(const GeoBounds& pArea)
{
    if (!mInstance.get())
    {
        return false;
    }
    ::GeoBounds tInner;
    tInner.mSouth = pArea.mSouth;
    tInner.mWest = pArea.mWest;
    tInner.mNorth = pArea.mNorth;
    tInner.mEast = pArea.mEast;
    return mInstance->data().startPrecache(tInner);
}

void XMap::cancelPrecache()
{
    if (mInstance.get())
    {
        mInstance->data().cancelPrecache();
    }
}

void XMap::setDiagnostics(bool pOn)
{
    if (XMapWindow* tWindow = windowOf(mInstance))
    {
        tWindow->setDiagnostics(pOn);
        log("diagnostics mode: {} (DIAG lines in Log.txt)", pOn ? "on" : "off");
    }
}

bool XMap::isDiagnosticsOn() const
{
    XMapWindow* tWindow = windowOf(mInstance);
    return tWindow && tWindow->isDiagnosticsOn();
}

void XMap::onRender()
{
    if (XMapWindow* tWindow = windowOf(mInstance))
    {
        tWindow->render();
    }
}

bool XMap::isDataLoaded() const
{
    return mIsDataLoaded;
}

bool XMap::setColor(MapLayer pLayer, const Rgb& pColor)
{
    XMapWindow* tWindow = windowOf(mInstance);
    if (!tWindow || !tWindow->setLayerColor(pLayer,
                                            Rgba{pColor.mR / 255.0f, pColor.mG / 255.0f, pColor.mB / 255.0f}))
    {
        return false;
    }
    mInstance->markChanged();
    return true;
}

Rgb XMap::getColor(MapLayer pLayer) const
{
    XMapWindow* tWindow = windowOf(mInstance);
    if (!tWindow)
    {
        return Rgb{-1, 0, 0};
    }
    const Rgba tColor = tWindow->layerColor(pLayer);
    return Rgb{int(tColor.mR * 255.0f + 0.5f), int(tColor.mG * 255.0f + 0.5f), int(tColor.mB * 255.0f + 0.5f)};
}

void XMap::clearColor(MapLayer pLayer)
{
    if (XMapWindow* tWindow = windowOf(mInstance))
    {
        tWindow->clearLayerColor(pLayer);
        mInstance->markChanged();
    }
}

void XMap::clearAllColors()
{
    if (XMapWindow* tWindow = windowOf(mInstance))
    {
        tWindow->clearLayerColors();
        mInstance->markChanged();
    }
}
}  // namespace prime
