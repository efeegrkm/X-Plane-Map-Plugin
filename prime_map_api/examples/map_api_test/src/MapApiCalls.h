// MapApiCalls.h
// Prime Map API'nin her cagrisi ayri bir metotta. Test eklentisinin geri kalani (otomatik test,
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
    bool InitLibrary(const std::string& pLogPrefix);
    void ShutdownLibrary();
    bool IsInitialized() const;
    PrimeMap::Status GetStatus() const;
    std::string VersionString() const;

    // --- Harita penceresi ---
    bool OpenMap(const PrimeMap::Geometry& pGeometry, PrimeMap::WindowStyle pStyle, bool pUi, bool pMouse);
    bool OpenSecondMap();  // tek harita kurali: false beklenir
    void CloseMap();
    bool IsOpen() const;
    void SetGeometry(const PrimeMap::Geometry& pGeometry);
    PrimeMap::Geometry GetGeometry() const;
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
    PrimeMap::LatLon GetCenter() const;
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
    void FitBounds(const PrimeMap::GeoBounds& pBounds);
    PrimeMap::GeoBounds GetVisibleBounds() const;
    double GetMetersPerBoxel() const;
    void SetFollowAircraft(bool pFollow);
    bool IsFollowingAircraft() const;
    void CenterOnAircraft();
    bool GoToAirport(const std::string& pIdent);
    std::vector<PrimeMap::AirportMatch> FindAirports(const std::string& pQuery, int pMax) const;
    PrimeMap::LatLon ScreenToGeo(int pX, int pY) const;
    bool GeoToScreen(double pLat, double pLon, int* pX, int* pY) const;

    // --- Mod ---
    void SetMode(PrimeMap::MapMode pMode);
    PrimeMap::MapMode GetMode() const;
    void NextMode();

    // --- Katmanlar ---
    void SetLayerVisible(PrimeMap::MapLayer pLayer, bool pVisible);
    bool IsLayerVisible(PrimeMap::MapLayer pLayer) const;
    void ToggleLayer(PrimeMap::MapLayer pLayer);
    void SetLayersVisible(std::initializer_list<PrimeMap::MapLayer> pLayers, bool pVisible);
    void ShowOnlyLayers(std::initializer_list<PrimeMap::MapLayer> pLayers);
    void ResetLayersToDefaults();
    PrimeMap::LayerState GetLayerState(PrimeMap::MapLayer pLayer) const;
    std::optional<PrimeMap::MapLayer> ParentOf(PrimeMap::MapLayer pLayer) const;
    std::vector<PrimeMap::MapLayer> ChildrenOf(PrimeMap::MapLayer pLayer) const;
    std::vector<PrimeMap::MapLayer> TopLevelLayers() const;
    PrimeMap::LayerKind KindOf(PrimeMap::MapLayer pLayer) const;
    std::string LayerName(PrimeMap::MapLayer pLayer) const;
    std::string LayerKey(PrimeMap::MapLayer pLayer) const;
    std::optional<PrimeMap::MapLayer> LayerFromKey(const std::string& pKey) const;
    bool IsLayerDefaultVisible(PrimeMap::MapLayer pLayer, PrimeMap::MapMode pMode) const;

    // --- Cizimler ---
    void SetDrawTool(PrimeMap::DrawTool pTool);
    PrimeMap::DrawTool GetDrawTool() const;
    bool AddDrawingPoint(double pLat, double pLon);
    bool FinishDrawing();
    bool UndoDrawing();
    void ClearDrawings();
    bool HasUnfinishedDrawing() const;
    std::vector<PrimeMap::Shape> GetShapes() const;
    bool RemoveShape(int pId);
    std::string ExportDrawings() const;
    bool ImportDrawings(const std::string& pText);

    // --- Ucak, iz, rota ---
    void SetAircraftSource(PrimeMap::DataSource pSource);
    PrimeMap::DataSource GetAircraftSource() const;
    void SetAircraft(const PrimeMap::AircraftState& pState);
    PrimeMap::AircraftState GetAircraft() const;
    void ClearTrack();
    void SetRouteSource(PrimeMap::DataSource pSource);
    PrimeMap::DataSource GetRouteSource() const;
    void SetRoute(const std::vector<PrimeMap::RouteWaypoint>& pWaypoints, int pActive);

    // --- Sorgu ---
    std::vector<PrimeMap::MapObject> QueryAt(double pLat, double pLon) const;
    std::vector<PrimeMap::MapObject> QueryAtScreen(int pX, int pY) const;

    // --- Ayarlar ---
    std::string ExportSettings() const;
    bool ImportSettings(const std::string& pText);
    PrimeMap::TerrainSettings GetTerrainSettings() const;
    void SetTerrainSettings(const PrimeMap::TerrainSettings& pSettings);
    std::string ExportTerrainSettings() const;
    bool ImportTerrainSettings(const std::string& pText);
    uint64_t GetSettingsRevision() const;
    uint64_t GetDrawingsRevision() const;

    // --- On onbellek, teshis ---
    bool PrecacheVisibleArea();
    bool PrecacheArea(const PrimeMap::GeoBounds& pArea);
    void CancelPrecache();
    void SetDiagnostics(bool pOn);
    bool IsDiagnosticsOn() const;

private:
    std::unique_ptr<PrimeMap::Map> mMap;
};
