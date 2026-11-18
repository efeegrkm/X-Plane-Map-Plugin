// Test eklentisinin haritaya eristigi tek sinif: API'nin her cagrisi ayri bir metottur.
#ifndef PRIME_MAP_TEST_SRC_XMAPAPICALLS_H
#define PRIME_MAP_TEST_SRC_XMAPAPICALLS_H

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <prime_map_api/PrimeMap.h>

class XMapApiCalls
{
public:
    bool initLibrary(const std::string& pLogPrefix, const std::string& pDataDirectory);
    void shutdownLibrary();
    bool isInitialized() const;
    prime::Status getStatus() const;
    prime::XMapManager* manager() const;
    std::string versionString() const;

    bool openXMap(const prime::Geometry& pGeometry, prime::WindowStyle pStyle, bool pUi, bool pMouse);
    bool openSecondXMap();
    void closeXMap();
    bool isOpen() const;
    void setGeometry(const prime::Geometry& pGeometry);
    prime::Geometry getGeometry() const;
    void setVisible(bool pVisible);
    bool isVisible() const;
    void bringToFront();
    void setPoppedOut(bool pPoppedOut);
    bool isPoppedOut() const;
    void setUiVisible(bool pVisible);
    bool isUiVisible() const;
    void setMouseInput(bool pEnabled);
    bool isMouseInputEnabled() const;
    prime::WindowStyle getStyle() const;

    bool setTextureSize(int pWidth, int pHeight);
    bool renderTexture();
    unsigned int getTextureId() const;
    bool injectMouse(int pX, int pY, prime::MouseAction pAction, prime::MouseButton pButton);
    void injectMouseMove(int pX, int pY);
    bool injectWheel(int pX, int pY, int pClicks);
    bool injectKey(char pKey, int pFlags, char pVirtualKey);
    bool wantsKeyboard() const;

    void setCenter(double pLat, double pLon);
    prime::LatLon getCenter() const;
    void setZoom(double pZoom);
    double getZoom() const;
    double minZoom() const;
    double maxZoom() const;
    void zoomIn();
    void zoomOut();
    void setZoomStep(double pStep);
    void panLeft();
    void panRight();
    void panUp();
    void panDown();
    void setPanStep(double pFraction);
    void panBy(double pDeltaX, double pDeltaY);
    void fitBounds(const prime::GeoBounds& pBounds);
    prime::GeoBounds getVisibleBounds() const;
    double getMetersPerBoxel() const;
    void setFollowAircraft(bool pFollow);
    bool isFollowingAircraft() const;
    void centerOnAircraft();
    bool goToAirport(const std::string& pIdent);
    std::vector<prime::AirportMatch> findAirports(const std::string& pQuery, int pMax) const;
    prime::LatLon screenToGeo(int pX, int pY) const;
    bool geoToScreen(double pLat, double pLon, int* pX, int* pY) const;

    void setMode(prime::XMapMode pMode);
    prime::XMapMode getMode() const;
    void nextMode();

    void setLayerVisible(prime::XMapLayer pLayer, bool pVisible);
    bool isLayerVisible(prime::XMapLayer pLayer) const;
    void toggleLayer(prime::XMapLayer pLayer);
    void setLayersVisible(std::initializer_list<prime::XMapLayer> pLayers, bool pVisible);
    void showOnlyLayers(std::initializer_list<prime::XMapLayer> pLayers);
    void resetLayersToDefaults();
    prime::LayerState getLayerState(prime::XMapLayer pLayer) const;
    std::optional<prime::XMapLayer> parentOf(prime::XMapLayer pLayer) const;
    std::vector<prime::XMapLayer> childrenOf(prime::XMapLayer pLayer) const;
    std::vector<prime::XMapLayer> topLevelLayers() const;
    prime::LayerKind kindOf(prime::XMapLayer pLayer) const;
    std::string layerName(prime::XMapLayer pLayer) const;
    std::string layerKey(prime::XMapLayer pLayer) const;
    std::optional<prime::XMapLayer> layerFromKey(const std::string& pKey) const;
    bool isLayerDefaultVisible(prime::XMapLayer pLayer, prime::XMapMode pMode) const;

    void setDrawTool(prime::DrawTool pTool);
    prime::DrawTool getDrawTool() const;
    bool addDrawingPoint(double pLat, double pLon);
    bool finishDrawing();
    bool undoDrawing();
    void clearDrawings();
    bool hasUnfinishedDrawing() const;
    std::vector<prime::Shape> getShapes() const;
    bool removeShape(int pId);
    std::string exportDrawings() const;
    bool importDrawings(const std::string& pText);

    void setAircraftSource(prime::DataSource pSource);
    prime::DataSource getAircraftSource() const;
    void setAircraft(const prime::AircraftState& pState);
    prime::AircraftState getAircraft() const;
    void clearTrack();
    void setRouteSource(prime::DataSource pSource);
    prime::DataSource getRouteSource() const;
    void setRoute(const std::vector<prime::RouteWaypoint>& pWaypoints, int pActive);

    std::vector<prime::XMapObject> queryAt(double pLat, double pLon) const;
    std::vector<prime::XMapObject> queryAtScreen(int pX, int pY) const;

    std::string exportSettings() const;
    bool importSettings(const std::string& pText);
    prime::TerrainSettings getTerrainSettings() const;
    void setTerrainSettings(const prime::TerrainSettings& pSettings);
    std::string exportTerrainSettings() const;
    bool importTerrainSettings(const std::string& pText);
    uint64_t getSettingsRevision() const;
    uint64_t getDrawingsRevision() const;

    bool precacheVisibleArea();
    bool precacheArea(const prime::GeoBounds& pArea);
    void cancelPrecache();
    void setDiagnostics(bool pOn);
    bool isDiagnosticsOn() const;

private:
    prime::XMap* mXMap = nullptr;
    prime::XMap* mSecondXMap = nullptr;
    prime::XMapManager* mManager = nullptr;
};
#endif
