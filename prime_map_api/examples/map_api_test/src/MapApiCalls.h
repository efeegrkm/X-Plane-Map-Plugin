// MapApiCalls.h
// Prime XMap API'nin her cagrisi ayri bir metotta. Test eklentisinin geri kalani (otomatik test,
// menu komutlari) haritaya SADECE bu sinif uzerinden erisir. Bir host eklentisi kendi
// dugmelerini bu metotlara (ya da dogrudan API'ye) baglar.
#pragma once

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <prime_map_api/PrimeMap.h>

class MapApiCalls
{
public:
    // --- Kutuphane ---
    bool initLibrary(const std::string& pLogPrefix);
    void shutdownLibrary();
    bool isInitialized() const;
    prime::Status getStatus() const;
    prime::XMapManager* manager() const;  // initLibrary ile yaratilan yonetici (yoksa nullptr)
    std::string versionString() const;

    // --- Harita penceresi ---
    bool openMap(const prime::Geometry& pGeometry, prime::WindowStyle pStyle, bool pUi, bool pMouse);
    // 0.23 fabrika ornegi: ikinci, bagimsiz harita (ayar klasoru maps/second/). Aciksa kapatir.
    bool openSecondMap();
    void closeMap();
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

    // --- Gorunum ---
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

    // --- Mod ---
    void setMode(prime::MapMode pMode);
    prime::MapMode getMode() const;
    void nextMode();

    // --- Katmanlar ---
    void setLayerVisible(prime::MapLayer pLayer, bool pVisible);
    bool isLayerVisible(prime::MapLayer pLayer) const;
    void toggleLayer(prime::MapLayer pLayer);
    void setLayersVisible(std::initializer_list<prime::MapLayer> pLayers, bool pVisible);
    void showOnlyLayers(std::initializer_list<prime::MapLayer> pLayers);
    void resetLayersToDefaults();
    prime::LayerState getLayerState(prime::MapLayer pLayer) const;
    std::optional<prime::MapLayer> parentOf(prime::MapLayer pLayer) const;
    std::vector<prime::MapLayer> childrenOf(prime::MapLayer pLayer) const;
    std::vector<prime::MapLayer> topLevelLayers() const;
    prime::LayerKind kindOf(prime::MapLayer pLayer) const;
    std::string layerName(prime::MapLayer pLayer) const;
    std::string layerKey(prime::MapLayer pLayer) const;
    std::optional<prime::MapLayer> layerFromKey(const std::string& pKey) const;
    bool isLayerDefaultVisible(prime::MapLayer pLayer, prime::MapMode pMode) const;

    // --- Cizimler ---
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

    // --- Ucak, iz, rota ---
    void setAircraftSource(prime::DataSource pSource);
    prime::DataSource getAircraftSource() const;
    void setAircraft(const prime::AircraftState& pState);
    prime::AircraftState getAircraft() const;
    void clearTrack();
    void setRouteSource(prime::DataSource pSource);
    prime::DataSource getRouteSource() const;
    void setRoute(const std::vector<prime::RouteWaypoint>& pWaypoints, int pActive);

    // --- Sorgu ---
    std::vector<prime::MapObject> queryAt(double pLat, double pLon) const;
    std::vector<prime::MapObject> queryAtScreen(int pX, int pY) const;

    // --- Ayarlar ---
    std::string exportSettings() const;
    bool importSettings(const std::string& pText);
    prime::TerrainSettings getTerrainSettings() const;
    void setTerrainSettings(const prime::TerrainSettings& pSettings);
    std::string exportTerrainSettings() const;
    bool importTerrainSettings(const std::string& pText);
    uint64_t getSettingsRevision() const;
    uint64_t getDrawingsRevision() const;

    // --- On onbellek, teshis ---
    bool precacheVisibleArea();
    bool precacheArea(const prime::GeoBounds& pArea);
    void cancelPrecache();
    void setDiagnostics(bool pOn);
    bool isDiagnosticsOn() const;

private:
    prime::XMap* mMap = nullptr;
    prime::XMap* mSecondMap = nullptr;
    prime::XMapManager* mManager = nullptr;  // sahibi; destroyXMapManager ile birakilir
};
