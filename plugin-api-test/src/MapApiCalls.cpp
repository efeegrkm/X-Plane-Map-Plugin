// MapApiCalls.cpp
// SDK_BOUND: her metot tek bir Prime Map API cagrisidir. Harita acik degilse (mMap bos) bir sey
// yapmaz / varsayilan doner; API'nin kendisi de ayni kurali uygular.
#include "MapApiCalls.h"

bool MapApiCalls::InitLibrary(const std::string& pLogPrefix)
{
    prime::InitConfig tConfig;
    tConfig.mLogPrefix = pLogPrefix;  // onbellek: bos = bu eklentinin klasoru/cache/
    return prime::MapLibrary::Get().Init(tConfig);
}

void MapApiCalls::ShutdownLibrary()
{
    prime::MapLibrary::Get().Shutdown();
}

bool MapApiCalls::OpenMap(const prime::Geometry& pGeometry, prime::WindowStyle pStyle, bool pUi, bool pMouse)
{
    prime::OpenConfig tConfig;
    tConfig.mGeometry = pGeometry;
    tConfig.mStyle = pStyle;
    tConfig.mUiVisible = pUi;
    tConfig.mMouseInput = pMouse;
    mMap = prime::Map::Open(tConfig);
    return mMap != nullptr;
}

bool MapApiCalls::OpenSecondMap()
{
    prime::OpenConfig tConfig;
    tConfig.mGeometry = {10, 300, 200, 150};
    std::unique_ptr<prime::Map> tSecond = prime::Map::Open(tConfig);
    return tSecond != nullptr;
}

void MapApiCalls::CloseMap()
{
    mMap.reset();
}

bool MapApiCalls::IsOpen() const
{
    return mMap != nullptr;
}

bool MapApiCalls::IsInitialized() const
{
    return prime::MapLibrary::Get().IsInitialized();
}

prime::Status MapApiCalls::GetStatus() const
{
    return prime::MapLibrary::Get().GetStatus();
}

std::string MapApiCalls::VersionString() const
{
    return prime::VersionString();
}

double MapApiCalls::MinZoom() const
{
    return prime::Map::MinZoom();
}

double MapApiCalls::MaxZoom() const
{
    return prime::Map::MaxZoom();
}

std::optional<prime::MapLayer> MapApiCalls::ParentOf(prime::MapLayer pLayer) const
{
    return prime::ParentOf(pLayer);
}

std::vector<prime::MapLayer> MapApiCalls::ChildrenOf(prime::MapLayer pLayer) const
{
    return prime::ChildrenOf(pLayer);
}

std::vector<prime::MapLayer> MapApiCalls::TopLevelLayers() const
{
    return prime::TopLevelLayers();
}

prime::LayerKind MapApiCalls::KindOf(prime::MapLayer pLayer) const
{
    return prime::KindOf(pLayer);
}

std::string MapApiCalls::LayerName(prime::MapLayer pLayer) const
{
    return prime::LayerName(pLayer);
}

std::string MapApiCalls::LayerKey(prime::MapLayer pLayer) const
{
    return prime::LayerKey(pLayer);
}

std::optional<prime::MapLayer> MapApiCalls::LayerFromKey(const std::string& pKey) const
{
    return prime::LayerFromKey(pKey);
}

bool MapApiCalls::IsLayerDefaultVisible(prime::MapLayer pLayer, prime::MapMode pMode) const
{
    return prime::IsLayerDefaultVisible(pLayer, pMode);
}

void MapApiCalls::SetGeometry(const prime::Geometry& pGeometry)
{
    if (mMap)
    {
        mMap->SetGeometry(pGeometry);
    }
}

prime::Geometry MapApiCalls::GetGeometry() const
{
    if (!mMap)
    {
        return prime::Geometry();
    }
    return mMap->GetGeometry();
}

void MapApiCalls::SetVisible(bool pVisible)
{
    if (mMap)
    {
        mMap->SetVisible(pVisible);
    }
}

bool MapApiCalls::IsVisible() const
{
    if (!mMap)
    {
        return false;
    }
    return mMap->IsVisible();
}

void MapApiCalls::BringToFront()
{
    if (mMap)
    {
        mMap->BringToFront();
    }
}

void MapApiCalls::SetPoppedOut(bool pPoppedOut)
{
    if (mMap)
    {
        mMap->SetPoppedOut(pPoppedOut);
    }
}

bool MapApiCalls::IsPoppedOut() const
{
    if (!mMap)
    {
        return false;
    }
    return mMap->IsPoppedOut();
}

void MapApiCalls::SetUiVisible(bool pVisible)
{
    if (mMap)
    {
        mMap->SetUiVisible(pVisible);
    }
}

bool MapApiCalls::IsUiVisible() const
{
    if (!mMap)
    {
        return false;
    }
    return mMap->IsUiVisible();
}

void MapApiCalls::SetMouseInput(bool pEnabled)
{
    if (mMap)
    {
        mMap->SetMouseInput(pEnabled);
    }
}

bool MapApiCalls::IsMouseInputEnabled() const
{
    if (!mMap)
    {
        return false;
    }
    return mMap->IsMouseInputEnabled();
}

void MapApiCalls::SetCenter(double pLat, double pLon)
{
    if (mMap)
    {
        mMap->SetCenter(pLat, pLon);
    }
}

prime::LatLon MapApiCalls::GetCenter() const
{
    if (!mMap)
    {
        return prime::LatLon();
    }
    return mMap->GetCenter();
}

void MapApiCalls::SetZoom(double pZoom)
{
    if (mMap)
    {
        mMap->SetZoom(pZoom);
    }
}

double MapApiCalls::GetZoom() const
{
    if (!mMap)
    {
        return 0.0;
    }
    return mMap->GetZoom();
}

void MapApiCalls::ZoomIn()
{
    if (mMap)
    {
        mMap->ZoomIn();
    }
}

void MapApiCalls::ZoomOut()
{
    if (mMap)
    {
        mMap->ZoomOut();
    }
}

void MapApiCalls::SetZoomStep(double pStep)
{
    if (mMap)
    {
        mMap->SetZoomStep(pStep);
    }
}

void MapApiCalls::PanLeft()
{
    if (mMap)
    {
        mMap->PanLeft();
    }
}

void MapApiCalls::PanRight()
{
    if (mMap)
    {
        mMap->PanRight();
    }
}

void MapApiCalls::PanUp()
{
    if (mMap)
    {
        mMap->PanUp();
    }
}

void MapApiCalls::PanDown()
{
    if (mMap)
    {
        mMap->PanDown();
    }
}

void MapApiCalls::SetPanStep(double pFraction)
{
    if (mMap)
    {
        mMap->SetPanStep(pFraction);
    }
}

void MapApiCalls::PanBy(double pDeltaX, double pDeltaY)
{
    if (mMap)
    {
        mMap->PanBy(pDeltaX, pDeltaY);
    }
}

void MapApiCalls::FitBounds(const prime::GeoBounds& pBounds)
{
    if (mMap)
    {
        mMap->FitBounds(pBounds);
    }
}

prime::GeoBounds MapApiCalls::GetVisibleBounds() const
{
    if (!mMap)
    {
        return prime::GeoBounds();
    }
    return mMap->GetVisibleBounds();
}

double MapApiCalls::GetMetersPerBoxel() const
{
    if (!mMap)
    {
        return 0.0;
    }
    return mMap->GetMetersPerBoxel();
}

void MapApiCalls::SetFollowAircraft(bool pFollow)
{
    if (mMap)
    {
        mMap->SetFollowAircraft(pFollow);
    }
}

bool MapApiCalls::IsFollowingAircraft() const
{
    if (!mMap)
    {
        return false;
    }
    return mMap->IsFollowingAircraft();
}

void MapApiCalls::CenterOnAircraft()
{
    if (mMap)
    {
        mMap->CenterOnAircraft();
    }
}

bool MapApiCalls::GoToAirport(const std::string& pIdent)
{
    if (!mMap)
    {
        return false;
    }
    return mMap->GoToAirport(pIdent);
}

std::vector<prime::AirportMatch> MapApiCalls::FindAirports(const std::string& pQuery, int pMax) const
{
    if (!mMap)
    {
        return {};
    }
    return mMap->FindAirports(pQuery, pMax);
}

prime::LatLon MapApiCalls::ScreenToGeo(int pX, int pY) const
{
    if (!mMap)
    {
        return prime::LatLon();
    }
    return mMap->ScreenToGeo(pX, pY);
}

bool MapApiCalls::GeoToScreen(double pLat, double pLon, int* pX, int* pY) const
{
    if (!mMap)
    {
        return false;
    }
    return mMap->GeoToScreen(pLat, pLon, pX, pY);
}

void MapApiCalls::SetMode(prime::MapMode pMode)
{
    if (mMap)
    {
        mMap->SetMode(pMode);
    }
}

prime::MapMode MapApiCalls::GetMode() const
{
    if (!mMap)
    {
        return prime::MapMode::Scenery;
    }
    return mMap->GetMode();
}

void MapApiCalls::NextMode()
{
    if (mMap)
    {
        mMap->NextMode();
    }
}

void MapApiCalls::SetLayerVisible(prime::MapLayer pLayer, bool pVisible)
{
    if (mMap)
    {
        mMap->SetLayerVisible(pLayer, pVisible);
    }
}

bool MapApiCalls::IsLayerVisible(prime::MapLayer pLayer) const
{
    if (!mMap)
    {
        return false;
    }
    return mMap->IsLayerVisible(pLayer);
}

void MapApiCalls::ToggleLayer(prime::MapLayer pLayer)
{
    if (mMap)
    {
        mMap->ToggleLayer(pLayer);
    }
}

void MapApiCalls::SetLayersVisible(std::initializer_list<prime::MapLayer> pLayers, bool pVisible)
{
    if (mMap)
    {
        mMap->SetLayersVisible(pLayers, pVisible);
    }
}

void MapApiCalls::ShowOnlyLayers(std::initializer_list<prime::MapLayer> pLayers)
{
    if (mMap)
    {
        mMap->ShowOnlyLayers(pLayers);
    }
}

void MapApiCalls::ResetLayersToDefaults()
{
    if (mMap)
    {
        mMap->ResetLayersToDefaults();
    }
}

prime::LayerState MapApiCalls::GetLayerState(prime::MapLayer pLayer) const
{
    if (!mMap)
    {
        return prime::LayerState::Off;
    }
    return mMap->GetLayerState(pLayer);
}

void MapApiCalls::SetDrawTool(prime::DrawTool pTool)
{
    if (mMap)
    {
        mMap->SetDrawTool(pTool);
    }
}

prime::DrawTool MapApiCalls::GetDrawTool() const
{
    if (!mMap)
    {
        return prime::DrawTool::Pan;
    }
    return mMap->GetDrawTool();
}

bool MapApiCalls::AddDrawingPoint(double pLat, double pLon)
{
    if (!mMap)
    {
        return false;
    }
    return mMap->AddDrawingPoint(pLat, pLon);
}

bool MapApiCalls::FinishDrawing()
{
    if (!mMap)
    {
        return false;
    }
    return mMap->FinishDrawing();
}

bool MapApiCalls::UndoDrawing()
{
    if (!mMap)
    {
        return false;
    }
    return mMap->UndoDrawing();
}

void MapApiCalls::ClearDrawings()
{
    if (mMap)
    {
        mMap->ClearDrawings();
    }
}

bool MapApiCalls::HasUnfinishedDrawing() const
{
    if (!mMap)
    {
        return false;
    }
    return mMap->HasUnfinishedDrawing();
}

std::vector<prime::Shape> MapApiCalls::GetShapes() const
{
    if (!mMap)
    {
        return {};
    }
    return mMap->GetShapes();
}

bool MapApiCalls::RemoveShape(int pId)
{
    if (!mMap)
    {
        return false;
    }
    return mMap->RemoveShape(pId);
}

std::string MapApiCalls::ExportDrawings() const
{
    if (!mMap)
    {
        return {};
    }
    return mMap->ExportDrawings();
}

bool MapApiCalls::ImportDrawings(const std::string& pText)
{
    if (!mMap)
    {
        return false;
    }
    return mMap->ImportDrawings(pText);
}

void MapApiCalls::SetAircraftSource(prime::DataSource pSource)
{
    if (mMap)
    {
        mMap->SetAircraftSource(pSource);
    }
}

prime::DataSource MapApiCalls::GetAircraftSource() const
{
    if (!mMap)
    {
        return prime::DataSource::XPlane;
    }
    return mMap->GetAircraftSource();
}

void MapApiCalls::SetAircraft(const prime::AircraftState& pState)
{
    if (mMap)
    {
        mMap->SetAircraft(pState);
    }
}

prime::AircraftState MapApiCalls::GetAircraft() const
{
    if (!mMap)
    {
        return prime::AircraftState();
    }
    return mMap->GetAircraft();
}

void MapApiCalls::ClearTrack()
{
    if (mMap)
    {
        mMap->ClearTrack();
    }
}

void MapApiCalls::SetRouteSource(prime::DataSource pSource)
{
    if (mMap)
    {
        mMap->SetRouteSource(pSource);
    }
}

prime::DataSource MapApiCalls::GetRouteSource() const
{
    if (!mMap)
    {
        return prime::DataSource::XPlane;
    }
    return mMap->GetRouteSource();
}

void MapApiCalls::SetRoute(const std::vector<prime::RouteWaypoint>& pWaypoints, int pActive)
{
    if (mMap)
    {
        mMap->SetRoute(pWaypoints, pActive);
    }
}

std::vector<prime::MapObject> MapApiCalls::QueryAt(double pLat, double pLon) const
{
    if (!mMap)
    {
        return {};
    }
    return mMap->QueryAt(pLat, pLon);
}

std::vector<prime::MapObject> MapApiCalls::QueryAtScreen(int pX, int pY) const
{
    if (!mMap)
    {
        return {};
    }
    return mMap->QueryAtScreen(pX, pY);
}

std::string MapApiCalls::ExportSettings() const
{
    if (!mMap)
    {
        return {};
    }
    return mMap->ExportSettings();
}

bool MapApiCalls::ImportSettings(const std::string& pText)
{
    if (!mMap)
    {
        return false;
    }
    return mMap->ImportSettings(pText);
}

prime::TerrainSettings MapApiCalls::GetTerrainSettings() const
{
    if (!mMap)
    {
        return prime::TerrainSettings();
    }
    return mMap->GetTerrainSettings();
}

void MapApiCalls::SetTerrainSettings(const prime::TerrainSettings& pSettings)
{
    if (mMap)
    {
        mMap->SetTerrainSettings(pSettings);
    }
}

std::string MapApiCalls::ExportTerrainSettings() const
{
    if (!mMap)
    {
        return {};
    }
    return mMap->ExportTerrainSettings();
}

bool MapApiCalls::ImportTerrainSettings(const std::string& pText)
{
    if (!mMap)
    {
        return false;
    }
    return mMap->ImportTerrainSettings(pText);
}

uint64_t MapApiCalls::GetSettingsRevision() const
{
    if (!mMap)
    {
        return 0;
    }
    return mMap->GetSettingsRevision();
}

uint64_t MapApiCalls::GetDrawingsRevision() const
{
    if (!mMap)
    {
        return 0;
    }
    return mMap->GetDrawingsRevision();
}

bool MapApiCalls::PrecacheVisibleArea()
{
    if (!mMap)
    {
        return false;
    }
    return mMap->PrecacheVisibleArea();
}

bool MapApiCalls::PrecacheArea(const prime::GeoBounds& pArea)
{
    if (!mMap)
    {
        return false;
    }
    return mMap->PrecacheArea(pArea);
}

void MapApiCalls::CancelPrecache()
{
    if (mMap)
    {
        mMap->CancelPrecache();
    }
}

void MapApiCalls::SetDiagnostics(bool pOn)
{
    if (mMap)
    {
        mMap->SetDiagnostics(pOn);
    }
}

bool MapApiCalls::IsDiagnosticsOn() const
{
    if (!mMap)
    {
        return false;
    }
    return mMap->IsDiagnosticsOn();
}
