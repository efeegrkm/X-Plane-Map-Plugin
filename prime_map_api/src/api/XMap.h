// Tek bir haritanin genel arayuzu: gorunum, mod, katmanlar, cizimler, veri kaynaklari ve ayarlar.
#ifndef PRIME_MAP_SRC_API_XMAP_H
#define PRIME_MAP_SRC_API_XMAP_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Drawings.h"
#include "LayerManager.h"
#include "XMapWindow.h"
#include "ScreenMath.h"
#include "XMapTypes.h"
#include "data/AircraftData.h"
#include "data/TerrainClasses.h"

class FmsLayer;
class Settings;
class TerrainLayer;
class TrackLayer;
class XMapData;

namespace prime
{
class XMapManager;

class PRIME_MAP_EXPORT XMap
{
public:
    ~XMap();

    bool initialize();
    void close();
    bool isInitialized() const;
    bool isDataLoaded() const;
    void onRender();

    int getId() const;
    bool setName(const std::string& pName);
    std::string getName() const;
    std::string getSettingsDirectory() const;
    void setPersistence(Persistence pPersistence);
    Persistence getPersistence() const;
    bool saveSettings();

    void setStyle(WindowStyle pStyle);
    WindowStyle getStyle() const;
    void setTitle(const std::string& pTitle);
    std::string getTitle() const;
    void setGeometry(const Geometry& pGeometry);
    Geometry getGeometry() const;
    void setVisible(bool pVisible);
    bool isVisible() const;
    void bringToFront();
    void setPoppedOut(bool pPoppedOut);
    bool isPoppedOut() const;
    void setIsUiVisible(bool pVisible);
    bool isUiVisible() const;
    void setIsMouseInputEnabled(bool pEnabled);
    bool isMouseInputEnabled() const;

    void setCenter(double pLat, double pLon);
    LatLon getCenter() const;
    void setZoom(double pZoom);
    double getZoom() const;
    static double minZoom();
    static double maxZoom();
    static double hardMaxZoom();
    bool setZoomLimits(double pMinZoom, double pMaxZoom);
    double getMinZoomLimit() const;
    double getMaxZoomLimit() const;
    void zoomIn();
    void zoomOut();
    void setZoomStep(double pStep);
    double getZoomStep() const;
    void panLeft();
    void panRight();
    void panUp();
    void panDown();
    void setPanStep(double pFractionOfView);
    double getPanStep() const;
    void panBy(double pDeltaX, double pDeltaY);
    void fitBounds(const GeoBounds& pBounds);
    GeoBounds getVisibleBounds() const;
    double getMetersPerBoxel() const;
    void setFollowAircraft(bool pFollow);
    bool isFollowingAircraft() const;
    void centerOnAircraft();
    bool goToAirport(const std::string& pIdent);
    std::vector<AirportMatch> findAirports(const std::string& pQuery, int pMaxResults) const;
    LatLon screenToGeo(int pX, int pY) const;
    bool geoToScreen(double pLat, double pLon, int* pX, int* pY) const;

    void setMode(XMapMode pMode);
    XMapMode getMode() const;
    void nextMode();

    void setIsLayerVisible(XMapLayer pLayer, bool pVisible);
    bool isLayerVisible(XMapLayer pLayer) const;
    void toggleLayer(XMapLayer pLayer);
    void setLayersVisible(std::initializer_list<XMapLayer> pLayers, bool pVisible);
    void showOnlyLayers(std::initializer_list<XMapLayer> pLayers);
    void resetLayersToDefaults();
    LayerState getLayerState(XMapLayer pLayer) const;
    bool setLayerZoomRange(XMapLayer pLayer, double pMinZoom, double pMaxZoom);
    void getLayerZoomRange(XMapLayer pLayer, double* pMinZoom, double* pMaxZoom) const;
    void clearLayerZoomRange(XMapLayer pLayer);
    void clearAllLayerZoomRanges();
    void getDefaultLayerZoomRange(XMapLayer pLayer, double* pMinZoom, double* pMaxZoom) const;
    double getLayerHardMinZoom(XMapLayer pLayer) const;
    bool setColor(XMapLayer pLayer, const Rgb& pColor);
    Rgb getColor(XMapLayer pLayer) const;
    void clearColor(XMapLayer pLayer);
    void clearAllColors();

    void setDrawTool(DrawTool pTool);
    DrawTool getDrawTool() const;
    bool addDrawingPoint(double pLat, double pLon);
    bool finishDrawing();
    bool undoDrawing();
    void clearDrawings();
    bool hasUnfinishedDrawing() const;
    std::vector<Shape> getShapes() const;
    bool removeShape(int pId);
    std::string exportDrawings() const;
    bool importDrawings(const std::string& pText);

    void setAircraftSource(DataSource pSource);
    DataSource getAircraftSource() const;
    void setAircraft(const AircraftState& pState);
    AircraftState getAircraft() const;
    void clearTrack();
    void setRouteSource(DataSource pSource);
    DataSource getRouteSource() const;
    void setRoute(const std::vector<RouteWaypoint>& pWaypoints, int pActiveIndex);

    std::vector<XMapObject> queryAt(double pLat, double pLon) const;
    std::vector<XMapObject> queryAtScreen(int pX, int pY) const;

    void setImageCache(bool pEnabled);
    bool isImageCacheEnabled() const;
    void setMaxRenderFps(int pFps);
    int getMaxRenderFps() const;
    void setXPlaneFont(bool pXPlaneFont);
    bool isXPlaneFont() const;

    std::string exportSettings() const;
    bool importSettings(const std::string& pText);
    TerrainSettings getTerrainSettings() const;
    void setTerrainSettings(const TerrainSettings& pSettings);
    std::string exportTerrainSettings() const;
    bool importTerrainSettings(const std::string& pText);
    uint64_t getSettingsRevision() const;
    uint64_t getDrawingsRevision() const;

    bool setTextureSize(int pWidth, int pHeight, double pPixelsPerUnit = 0.0);
    int getTextureWidth() const;
    int getTextureHeight() const;
    bool renderTexture();
    unsigned int getTextureId() const;
    bool injectMouse(int pX, int pY, MouseAction pAction, MouseButton pButton = MouseButton::Left);
    void injectMouseMove(int pX, int pY);
    bool injectWheel(int pX, int pY, int pClicks);
    bool injectKey(char pKey, int pFlags, char pVirtualKey);
    bool wantsKeyboard() const;

    std::string getStatusText() const;
    int getTerrainPendingImages() const;

    bool precacheVisibleArea();
    bool precacheArea(const GeoBounds& pArea);
    void cancelPrecache();
    void setDiagnostics(bool pOn);
    bool isDiagnosticsOn() const;

private:
    friend class XMapManager;
    friend class ::XMapData;

    XMap(XMapData& pData, int pId);
    XMap(const XMap&) = delete;
    XMap& operator=(const XMap&) = delete;

    void createLayers();
    void loadFiles();
    bool writeSettingsFile();
    bool writeDrawingsFile();
    bool writeTerrainFile();
    void applyWindowSection();
    void applySourcesSection();
    void applyLayerColorsSection(const Settings& pStore);
    void storeExtraSections() const;
    RectI savedGeometry() const;
    uint64_t stateSignature() const;
    void teardown();
    bool alive(const char* pWho) const;
    void update(float pNow);
    std::string folderName() const;
    static std::string safeFolderName(const std::string& pName);
    bool saveNow();
    void markChanged();
    void applyBuiltUpPalette();

    XMapData& mData;
    std::string mMapsDirectory;
    int mId = 0;
    std::string mName;
    Persistence mPersistence = Persistence::LoadAndSave;

    std::unique_ptr<AircraftData> mAircraft;
    std::unique_ptr<Drawings> mDrawings;
    std::unique_ptr<TerrainPalette> mPalette;
    int mPaletteId = 0;
    std::unique_ptr<LayerManager> mLayers;
    TerrainLayer* mTerrainLayer = nullptr;
    FmsLayer* mFmsLayer = nullptr;
    TrackLayer* mTrackLayer = nullptr;
    std::unique_ptr<XMapWindow> mWindow;

    uint64_t mSavedSettings = 0;
    uint64_t mSavedDrawings = 0;
    uint64_t mSavedState = 0;
    uint64_t mExtraRevision = 0;
    uint64_t mSavedExtra = 0;
    float mLastSaveTime = -100.0f;
    RectI mFloatingGeometry;
    std::string mStatusText;
    std::atomic<bool>* mDataLoadedFlag = nullptr;

    std::atomic<bool> mIsDataLoaded{false};
    bool mTextureWarned = false;
    mutable bool mShutdownWarned = false;
    XMapManager* mManager = nullptr;
};
}

#endif
