// MapApiCalls.h
// SDK_BOUND: Prime Map API'nin her cagrisi ayri bir metotta. Test eklentisinin geri kalani
// (pencere, dugmeler) haritaya SADECE bu sinif uzerinden erisir. Bir host eklentisi kendi
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
    bool InitLibrary(const std::string& pLogPrefix);
    void ShutdownLibrary();
    bool IsInitialized() const;
    prime::Status GetStatus() const;
    std::string VersionString() const;

    // --- Harita penceresi ---
    bool OpenMap(const prime::Geometry& pGeometry, prime::WindowStyle pStyle, bool pUi, bool pMouse);
    // 0.23 fabrika ornegi: ikinci, bagimsiz bir harita (ayar klasoru maps/second/). Zaten aciksa
    // kapatir. Acildiysa true.
    bool OpenSecondMap();
    void CloseMap();
    bool IsOpen() const;
    void SetGeometry(const prime::Geometry& pGeometry);
    prime::Geometry GetGeometry() const;
    void SetVisible(bool pVisible);
    bool IsVisible() const;
    void BringToFront();
    void SetPoppedOut(bool pPoppedOut);
    bool IsPoppedOut() const;
    void SetUiVisible(bool pVisible);
    bool IsUiVisible() const;
    void SetMouseInput(bool pEnabled);
    bool IsMouseInputEnabled() const;

    // --- Gorunum ---
    void SetCenter(double pLat, double pLon);
    prime::LatLon GetCenter() const;
    void SetZoom(double pZoom);
    double GetZoom() const;
    double MinZoom() const;
    double MaxZoom() const;
    void ZoomIn();
    void ZoomOut();
    void SetZoomStep(double pStep);
    void PanLeft();
    void PanRight();
    void PanUp();
    void PanDown();
    void SetPanStep(double pFraction);
    void PanBy(double pDeltaX, double pDeltaY);
    void FitBounds(const prime::GeoBounds& pBounds);
    prime::GeoBounds GetVisibleBounds() const;
    double GetMetersPerBoxel() const;
    void SetFollowAircraft(bool pFollow);
    bool IsFollowingAircraft() const;
    void CenterOnAircraft();
    bool GoToAirport(const std::string& pIdent);
    std::vector<prime::AirportMatch> FindAirports(const std::string& pQuery, int pMax) const;
    prime::LatLon ScreenToGeo(int pX, int pY) const;
    bool GeoToScreen(double pLat, double pLon, int* pX, int* pY) const;

    // --- Mod ---
    void SetMode(prime::MapMode pMode);
    prime::MapMode GetMode() const;
    void NextMode();

    // --- Katmanlar ---
    void SetLayerVisible(prime::MapLayer pLayer, bool pVisible);
    bool IsLayerVisible(prime::MapLayer pLayer) const;
    void ToggleLayer(prime::MapLayer pLayer);
    void SetLayersVisible(std::initializer_list<prime::MapLayer> pLayers, bool pVisible);
    void ShowOnlyLayers(std::initializer_list<prime::MapLayer> pLayers);
    void ResetLayersToDefaults();
    prime::LayerState GetLayerState(prime::MapLayer pLayer) const;
    std::optional<prime::MapLayer> ParentOf(prime::MapLayer pLayer) const;
    std::vector<prime::MapLayer> ChildrenOf(prime::MapLayer pLayer) const;
    std::vector<prime::MapLayer> TopLevelLayers() const;
    prime::LayerKind KindOf(prime::MapLayer pLayer) const;
    std::string LayerName(prime::MapLayer pLayer) const;
    std::string LayerKey(prime::MapLayer pLayer) const;
    std::optional<prime::MapLayer> LayerFromKey(const std::string& pKey) const;
    bool IsLayerDefaultVisible(prime::MapLayer pLayer, prime::MapMode pMode) const;

    // --- Cizimler ---
    void SetDrawTool(prime::DrawTool pTool);
    prime::DrawTool GetDrawTool() const;
    bool AddDrawingPoint(double pLat, double pLon);
    bool FinishDrawing();
    bool UndoDrawing();
    void ClearDrawings();
    bool HasUnfinishedDrawing() const;
    std::vector<prime::Shape> GetShapes() const;
    bool RemoveShape(int pId);
    std::string ExportDrawings() const;
    bool ImportDrawings(const std::string& pText);

    // --- Ucak, iz, rota ---
    void SetAircraftSource(prime::DataSource pSource);
    prime::DataSource GetAircraftSource() const;
    void SetAircraft(const prime::AircraftState& pState);
    prime::AircraftState GetAircraft() const;
    void ClearTrack();
    void SetRouteSource(prime::DataSource pSource);
    prime::DataSource GetRouteSource() const;
    void SetRoute(const std::vector<prime::RouteWaypoint>& pWaypoints, int pActive);

    // --- Sorgu ---
    std::vector<prime::MapObject> QueryAt(double pLat, double pLon) const;
    std::vector<prime::MapObject> QueryAtScreen(int pX, int pY) const;

    // --- Ayarlar ---
    std::string ExportSettings() const;
    bool ImportSettings(const std::string& pText);
    prime::TerrainSettings GetTerrainSettings() const;
    void SetTerrainSettings(const prime::TerrainSettings& pSettings);
    std::string ExportTerrainSettings() const;
    bool ImportTerrainSettings(const std::string& pText);
    uint64_t GetSettingsRevision() const;
    uint64_t GetDrawingsRevision() const;

    // --- On onbellek, teshis ---
    bool PrecacheVisibleArea();
    bool PrecacheArea(const prime::GeoBounds& pArea);
    void CancelPrecache();
    void SetDiagnostics(bool pOn);
    bool IsDiagnosticsOn() const;

private:
    std::unique_ptr<prime::Map> mMap;
    std::unique_ptr<prime::Map> mSecondMap;  // OpenSecondMap
};
