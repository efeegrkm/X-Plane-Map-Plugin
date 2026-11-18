// Bir haritanin pencere ya da doku olarak cizimi, kendi arayuzu ve fare/klavye girdileri.
#ifndef PRIME_MAP_SRC_XMAPWINDOW_H
#define PRIME_MAP_SRC_XMAPWINDOW_H

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "XPLMDataAccess.h"
#include "XPLMDisplay.h"

#include "Color.h"
#include "Geo.h"
#include "LabelPlacer.h"
#include "XMapImageCache.h"
#include "XMapView.h"
#include "api/XMapTypes.h"
#include "ScreenMath.h"
#include "gfx/Canvas.h"
#include "gfx/RenderTarget.h"
#include "XMapTheme.h"
#include "Settings.h"
#include "ui/AirportSearchPanel.h"
#include "ui/AtlasTextRenderer.h"
#include "ui/LayerPanel.h"
#include "ui/ModePanel.h"
#include "ui/StatusBar.h"
#include "ui/ToolPalette.h"
#include "ui/Toolbar.h"

class AircraftData;
class AirportGeometryStore;
class Drawings;
class LayerManager;

struct drawContext;

class XMapWindow
{
public:
    XMapWindow(LayerManager& pLayers, AircraftData& pAircraft, Drawings& pDrawings,
              const AirportGeometryStore& pAirports);
    ~XMapWindow();

    XMapWindow(const XMapWindow&) = delete;
    XMapWindow& operator=(const XMapWindow&) = delete;

    void create();
    void destroy();
    bool isCreated() const;

    void setEmbedded(bool pEmbedded);
    bool isEmbedded() const;
    bool wantsVisible() const;
    void setTitle(const std::string& pTitle);
    const std::string& title() const;
    void setImageCacheEnabled(bool pEnabled);
    bool isImageCacheEnabled() const;
    void setMaxRenderFps(int pFps);
    int maxRenderFps() const;
    void setXPlaneFont(bool pXPlaneFont);
    bool usesXPlaneFont() const;
    void setZoomStep(double pStep);
    double zoomStep() const;
    void setPanStep(double pFraction);
    double panStep() const;
    void setZoomLimits(double pMinZoom, double pMaxZoom);
    Settings& settingsStore();
    void invalidateImage();

    bool isVisible() const;
    void setVisible(bool pVisible);
    void toggleVisible();
    void toggleControls();
    void setDiagnostics(bool pOn);

    void setMapMode(XMapMode pMode);
    void nextMapMode();
    XMapMode getMapMode() const;

    GeoBounds visibleBounds() const;

    void setStatusMessage(const std::string& pMessage);

    void setDataBusy(bool pBusy);

    void update(float pNow);

    void saveSettings();

    XMapView& view();
    const XMapView& view() const;
    void setGeometry(const RectI& pGeometry);
    RectI getGeometry() const;
    void bringToFront();
    void setPoppedOut(bool pPoppedOut);
    bool isPoppedOut() const;
    void setUiVisible(bool pVisible);
    bool isUiVisible() const;
    void setMouseInput(bool pEnabled);
    bool isMouseInputEnabled() const;
    void setFollowAircraft(bool pFollow);
    bool isFollowingAircraft() const;
    void centerOnAircraft();
    void goToAirport(int pAirport);
    bool isDiagnosticsOn() const;
    void resetLayers();
    void drawingsChanged();
    std::string exportSettings();
    void importSettings(const std::string& pText);
    std::vector<std::pair<std::string, std::string>> describeAt(int pX, int pY);
    std::string performanceText() const;
    uint64_t settingsRevision() const;
    uint64_t drawingsRevision() const;

    bool setLayerColor(prime::XMapLayer pLayer, const Rgba& pColor);
    Rgba layerColor(prime::XMapLayer pLayer) const;
    void clearLayerColor(prime::XMapLayer pLayer);
    void clearLayerColors();
    void setLayerColors(const std::map<prime::XMapLayer, Rgba>& pColors);
    const std::map<prime::XMapLayer, Rgba>& layerColors() const;

    void render();

    void setTextureMode(bool pTexture);
    bool isTextureMode() const;
    void setTextureSize(int pWidth, int pHeight, double pPixelsPerBoxel);
    int textureWidth() const;
    int textureHeight() const;
    bool renderTexture();
    unsigned int textureId() const;
    int injectMouse(int pX, int pY, XPLMMouseStatus pStatus, bool pRight);
    void injectMouseMove(int pX, int pY);
    int injectWheel(int pX, int pY, int pClicks);
    bool injectKey(char pKey, XPLMKeyFlags pFlags, char pVirtualKey);
    bool wantsKeyboard() const;

private:
    void applyLayerColors();

    static void drawCallback(XPLMWindowID pWindow, void* pRefcon);
    static int clickCallback(XPLMWindowID pWindow, int pX, int pY, XPLMMouseStatus pStatus, void* pRefcon);
    static int rightClickCallback(XPLMWindowID pWindow, int pX, int pY, XPLMMouseStatus pStatus,
                                  void* pRefcon);
    static void keyCallback(XPLMWindowID pWindow, char pKey, XPLMKeyFlags pFlags, char pVirtualKey,
                            void* pRefcon, int pLosingFocus);
    static XPLMCursorStatus cursorCallback(XPLMWindowID pWindow, int pX, int pY, void* pRefcon);
    static int wheelCallback(XPLMWindowID pWindow, int pX, int pY, int pWheel, int pClicks, void* pRefcon);

    void onDraw();
    void prepareFrame();
    bool beginCanvasFrame();
    void drawFrame(float pNow, const std::optional<Affine2D>& pBoxelToPixel, bool pUseImageCache,
                   const RectD& pScissorPixels);
    RectI textureArea() const;
    bool prepareCanvas();
    void createTextRenderer();
    void drawTooltip(const drawContext& pContext, const std::string& pText);
    int onClick(int pX, int pY, XPLMMouseStatus pStatus);
    int onRightClick(int pX, int pY, XPLMMouseStatus pStatus);
    int onWheel(int pX, int pY, int pWheel, int pClicks);
    void rememberMouse(int pX, int pY);
    void onKey(char pKey, XPLMKeyFlags pFlags, char pVirtualKey, bool pLosingFocus);

    void drawControls(int pMouseX, int pMouseY, bool pMouseInside);
    bool handleControlClick(int pX, int pY);
    void handleMapClick(int pX, int pY);
    void performToolbarAction(ToolbarAction pAction);
    void applyLayerPanelHit(const LayerPanel::Hit& pHit);
    RectI paletteBounds() const;
    void togglePopOut();
    void setSearchOpen(bool pOpen);
    void setKeyboardFocus(bool pFocus);
    bool hasKeyboardFocus() const;
    ToolbarState currentToolbarState() const;
    RectI showControlsButtonRect() const;
    bool mouseInside(int* pX, int* pY) const;
    bool isOverControls(int pX, int pY) const;
    RectD clearArea() const;
    std::string hintText() const;
    void logDiagnostics(float pNow, double pPixelsPerBoxel, const RectD& pScissor);
    uint64_t sceneSignature() const;

    void applySettings();
    void saveDrawings();
    void useOwnText();

    LayerManager& mLayers;
    AircraftData& mAircraft;
    Settings mSettings;
    Drawings& mDrawings;
    uint64_t mSettingsRevision = 0;
    uint64_t mDrawingsRevision = 0;
    bool mEmbedded = true;
    bool mMouseInput = false;
    bool mXPlaneFont = false;
    bool mTextPending = false;
    bool mWantVisible = true;
    std::string mTitle = "XMap";
    int mMaxRenderFps = 15;
    double mZoomStep = 0.5;
    double mPanStep = 0.25;
    bool mPassClick = false;
    double mLastPixelsPerBoxel = 1.0;

    XPLMWindowID mWindow = nullptr;
    XMapView mView;
    Gfx::Canvas mCanvas;
    bool mCanvasChecked = false;
    std::unique_ptr<AtlasTextRenderer> mText;
    bool mContextWarned = false;
    Toolbar mToolbar;
    LayerPanel mLayerPanel;
    ModePanel mModePanel;
    const AirportGeometryStore& mAirports;
    AirportSearchPanel mSearchPanel;
    ToolPalette mToolPalette;
    XMapMode mMapMode = XMapMode::Dark;
    XMapTheme mTheme = themeFor(XMapMode::Dark);
    std::map<prime::XMapLayer, Rgba> mLayerColors;
    StatusBar mStatusBar;
    LabelPlacer mLabelPlacer;
    XMapImageCache mImageCache;
    bool mUseImageCache = true;
    bool mImageCacheWarned = false;
    uint64_t mLastSignature = 0;
    bool mDataBusy = false;
    bool mWasBusy = false;
    std::string mStatusMessage;

    XPLMDataRef mFramePeriodRef = nullptr;
    double mDrawMs = 0.0;

    RectI mArea;
    RectI mFloatingGeometry;
    bool mHasFloatingGeometry = false;
    bool mWasVisible = false;

    bool mFollowAircraft = true;
    bool mShowControls = true;
    bool mDiagnostics = false;
    float mLastDiagnosticsTime = -10.0f;

    bool mPressOnMap = false;
    bool mPressMoved = false;
    bool mPaletteDrag = false;
    int mPressX = 0;
    int mPressY = 0;
    int mDragLastX = 0;
    int mDragLastY = 0;

    bool mTextureMode = false;
    bool mTextureOpen = false;
    int mTextureWidth = 400;
    int mTextureHeight = 300;
    double mTexturePixelsPerBoxel = 0.0;
    Gfx::renderTarget mTextureTarget;
    bool mTextureDirty = true;
    bool mTextureKeyboard = false;
    bool mTextureFontWarned = false;
    float mLastTextureRender = -100.0f;
    double mTextureCenterX = 0.0;
    double mTextureCenterY = 0.0;
    double mTextureZoom = -1.0;

    int mMouseX = 0;
    int mMouseY = 0;
    float mMouseTime = -10.0f;
};
#endif
