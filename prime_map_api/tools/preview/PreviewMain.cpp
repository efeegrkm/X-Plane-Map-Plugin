
// map_preview: eklentinin harita katmanlarini X-Plane OLMADAN, gercek X-Plane
// verisiyle bir resme cizer. Gelistirirken gorsel kontrol icin.

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "Drawings.h"
#include "gfx/Canvas.h"
#include "ui/AtlasTextRenderer.h"
#include "XMapImageCache.h"
#include "gfx/GlScreen.h"
#include "XPLMGraphics.h"
#include "gfx/GlInclude.h"
#include "LabelPlacer.h"
#include "LayerManager.h"
#include "XMapTheme.h"
#include "XMapView.h"
#include "Settings.h"
#include "data/AircraftData.h"
#include "data/AirportGeometryStore.h"
#include "data/AirwayDatabase.h"
#include "data/AirportDetailStore.h"
#include "data/AptDatIndexer.h"
#include "data/NavDatabase.h"
#include "data/TerrainStore.h"
#include "layers/AircraftLayer.h"
#include "layers/AirportLayer.h"
#include "layers/AirportObjectsLayer.h"
#include "layers/TaxiRouteLayer.h"
#include "layers/AirportSurfaceLayer.h"
#include "layers/AirwayLayer.h"
#include "layers/DrawingLayer.h"
#include "layers/GridLayer.h"
#include "layers/NavaidLayer.h"
#include "layers/RunwayLayer.h"
#include "layers/TerrainHub.h"
#include "layers/TerrainLayer.h"
#include "layers/RoadLayer.h"
#include "data/TerrainPaletteSource.h"
#include "tools/preview/FileNavLoader.h"
#include "tools/preview/XplmStubs.h"
#include "ui/LayerPanel.h"
#include "ui/StatusBar.h"
#include "ui/ToolPalette.h"
#include "ui/Toolbar.h"
#include "ui/UiDraw.h"

namespace
{

// OpenGL 3.0 framebuffer fonksiyonlari (opengl32.dll sadece 1.1 verir)
typedef void(APIENTRY* GenFramebuffersFn)(GLsizei, GLuint*);
typedef void(APIENTRY* BindFramebufferFn)(GLenum, GLuint);
typedef void(APIENTRY* GenRenderbuffersFn)(GLsizei, GLuint*);
typedef void(APIENTRY* BindRenderbufferFn)(GLenum, GLuint);
typedef void(APIENTRY* RenderbufferStorageFn)(GLenum, GLenum, GLsizei, GLsizei);
typedef void(APIENTRY* FramebufferRenderbufferFn)(GLenum, GLenum, GLenum, GLuint);
typedef GLenum(APIENTRY* CheckFramebufferStatusFn)(GLenum);

const GLenum kGlFramebuffer = 0x8D40;
const GLenum kGlRenderbuffer = 0x8D41;
const GLenum kGlColorAttachment0 = 0x8CE0;
const GLenum kGlRgba8 = 0x8058;
const GLenum kGlFramebufferComplete = 0x8CD5;

struct Options
{
    double mLat = 47.45;
    double mLon = -122.31;
    double mZoom = 11.0;
    int mWidth = 1200;
    int mHeight = 800;
    std::string mOut = "preview.ppm";
    std::string mXPlane = "C:/X-Plane 11/";
    bool mAtlasText = true;  // --xplm-text: kendi yazi atlasimiz yerine XPLMDrawString
    bool mImageCache = false;  // --fbo: eklentideki gibi FBO resmi + ustune overlay katmanlari
    bool mUi = true;
    bool mDemoDrawings = false;
    bool mFixes = false;
    std::string mCache = "terrain_cache/";
    std::string mTerrainColors;  // --terrain-colors <ini>: eklentinin terrain_colors.ini'si (bossa varsayilan)
    std::vector<std::string> mHidden;  // --hide <katman kimligi> (birden fazla verilebilir)
    XMapMode mMode = XMapMode::Scenery;  // --mode scenery|vfr|ifr_low|ifr_high
    std::vector<std::string> mShown;   // --show <katman kimligi>: varsayilan kapali katmani ac
    bool mPaletteCollapsed = false;          // --palette-collapsed: cizim paleti katlanmis
    std::vector<std::string> mPanelGroups;  // --panel <grup>: katman panelini bu grup acik ciz
    bool mPanel = false;
    // --describe x,y: o noktadaki fare aciklamasini yazdir (resim pikseli, ust = 0)
    std::vector<std::pair<double, double>> mDescribe;
};

bool ParseOptions(int pArgc, char** pArgv, Options* pOut)
{
    for (int tIndex = 1; tIndex < pArgc; ++tIndex)
    {
        const std::string tArg = pArgv[tIndex];
        const bool tHasValue = tIndex + 1 < pArgc;
        if (tArg == "--lat" && tHasValue)
        {
            pOut->mLat = std::atof(pArgv[++tIndex]);
        }
        else if (tArg == "--lon" && tHasValue)
        {
            pOut->mLon = std::atof(pArgv[++tIndex]);
        }
        else if (tArg == "--zoom" && tHasValue)
        {
            pOut->mZoom = std::atof(pArgv[++tIndex]);
        }
        else if (tArg == "--size" && tHasValue)
        {
            if (std::sscanf(pArgv[++tIndex], "%dx%d", &pOut->mWidth, &pOut->mHeight) != 2)
            {
                return false;
            }
        }
        else if (tArg == "--out" && tHasValue)
        {
            pOut->mOut = pArgv[++tIndex];
        }
        else if (tArg == "--xplane" && tHasValue)
        {
            pOut->mXPlane = pArgv[++tIndex];
        }
        else if (tArg == "--fbo")
        {
            pOut->mImageCache = true;  // harita resmi onbellegi (XMapImageCache) uzerinden ciz
        }
        else if (tArg == "--xplm-text")
        {
            pOut->mAtlasText = false;  // eski yol: XPLMDrawString (onizlemede Windows yazi tipi)
        }
        else if (tArg == "--no-ui")
        {
            pOut->mUi = false;
        }
        else if (tArg == "--demo-drawings")
        {
            pOut->mDemoDrawings = true;
        }
        else if (tArg == "--cache" && tHasValue)
        {
            pOut->mCache = pArgv[++tIndex];
        }
        else if (tArg == "--terrain-colors" && tHasValue)
        {
            pOut->mTerrainColors = pArgv[++tIndex];
        }
        else if (tArg == "--fixes")
        {
            pOut->mFixes = true;
        }
        else if (tArg == "--mode" && tHasValue)
        {
            pOut->mMode = modeFromId(pArgv[++tIndex], XMapMode::Scenery);
        }
        else if (tArg == "--hide" && tHasValue)
        {
            pOut->mHidden.push_back(pArgv[++tIndex]);
        }
        else if (tArg == "--show" && tHasValue)
        {
            pOut->mShown.push_back(pArgv[++tIndex]);
        }
        else if (tArg == "--palette-collapsed")
        {
            pOut->mPaletteCollapsed = true;
        }
        else if (tArg == "--panel" && tHasValue)
        {
            pOut->mPanel = true;
            pOut->mPanelGroups.push_back(pArgv[++tIndex]);
        }
        else if (tArg == "--describe" && tHasValue)
        {
            double tX = 0.0;
            double tY = 0.0;
            if (std::sscanf(pArgv[++tIndex], "%lf,%lf", &tX, &tY) != 2)
            {
                std::fprintf(stderr, "--describe expects x,y\n");
                return false;
            }
            pOut->mDescribe.emplace_back(tX, tY);
        }
        else
        {
            std::fprintf(stderr, "unknown option: %s\n", tArg.c_str());
            return false;
        }
    }
    return true;
}

// Gizli pencere + OpenGL baglami (cizim pencereye degil FBO'ya yapilir)
bool CreateGlContext(HDC* pOutDc)
{
    WNDCLASSA tClass = {};
    tClass.lpfnWndProc = DefWindowProcA;
    tClass.hInstance = GetModuleHandleA(nullptr);
    tClass.lpszClassName = "MapPreviewWindow";
    RegisterClassA(&tClass);
    HWND tWindow = CreateWindowA("MapPreviewWindow", "Map preview", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr,
                                 nullptr, tClass.hInstance, nullptr);
    if (!tWindow)
    {
        return false;
    }
    HDC tDc = GetDC(tWindow);
    PIXELFORMATDESCRIPTOR tFormat = {};
    tFormat.nSize = sizeof(tFormat);
    tFormat.nVersion = 1;
    tFormat.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    tFormat.iPixelType = PFD_TYPE_RGBA;
    tFormat.cColorBits = 32;
    tFormat.cAlphaBits = 8;
    if (!SetPixelFormat(tDc, ChoosePixelFormat(tDc, &tFormat), &tFormat))
    {
        return false;
    }
    HGLRC tContext = wglCreateContext(tDc);
    if (!tContext || !wglMakeCurrent(tDc, tContext))
    {
        return false;
    }
    *pOutDc = tDc;
    return true;
}

bool CreateFramebuffer(int pWidth, int pHeight)
{
    GenFramebuffersFn tGenFramebuffers = reinterpret_cast<GenFramebuffersFn>(wglGetProcAddress("glGenFramebuffers"));
    BindFramebufferFn tBindFramebuffer = reinterpret_cast<BindFramebufferFn>(wglGetProcAddress("glBindFramebuffer"));
    GenRenderbuffersFn tGenRenderbuffers =
        reinterpret_cast<GenRenderbuffersFn>(wglGetProcAddress("glGenRenderbuffers"));
    BindRenderbufferFn tBindRenderbuffer =
        reinterpret_cast<BindRenderbufferFn>(wglGetProcAddress("glBindRenderbuffer"));
    RenderbufferStorageFn tRenderbufferStorage =
        reinterpret_cast<RenderbufferStorageFn>(wglGetProcAddress("glRenderbufferStorage"));
    FramebufferRenderbufferFn tFramebufferRenderbuffer =
        reinterpret_cast<FramebufferRenderbufferFn>(wglGetProcAddress("glFramebufferRenderbuffer"));
    CheckFramebufferStatusFn tCheckStatus =
        reinterpret_cast<CheckFramebufferStatusFn>(wglGetProcAddress("glCheckFramebufferStatus"));
    if (!tGenFramebuffers || !tBindFramebuffer || !tGenRenderbuffers || !tBindRenderbuffer ||
        !tRenderbufferStorage || !tFramebufferRenderbuffer || !tCheckStatus)
    {
        return false;
    }
    GLuint tFramebuffer = 0;
    GLuint tColor = 0;
    tGenFramebuffers(1, &tFramebuffer);
    tBindFramebuffer(kGlFramebuffer, tFramebuffer);
    tGenRenderbuffers(1, &tColor);
    tBindRenderbuffer(kGlRenderbuffer, tColor);
    tRenderbufferStorage(kGlRenderbuffer, kGlRgba8, pWidth, pHeight);
    tFramebufferRenderbuffer(kGlFramebuffer, kGlColorAttachment0, kGlRenderbuffer, tColor);
    return tCheckStatus(kGlFramebuffer) == kGlFramebufferComplete;
}

bool WritePpm(const std::string& pPath, int pWidth, int pHeight)
{
    std::vector<unsigned char> tPixels(static_cast<size_t>(pWidth) * pHeight * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, pWidth, pHeight, GL_RGB, GL_UNSIGNED_BYTE, tPixels.data());
    FILE* tFile = std::fopen(pPath.c_str(), "wb");
    if (!tFile)
    {
        return false;
    }
    std::fprintf(tFile, "P6\n%d %d\n255\n", pWidth, pHeight);
    for (int tRow = pHeight - 1; tRow >= 0; --tRow)  // OpenGL alttan baslar, resim ustten
    {
        std::fwrite(tPixels.data() + static_cast<size_t>(tRow) * pWidth * 3, 1, static_cast<size_t>(pWidth) * 3,
                    tFile);
    }
    std::fclose(tFile);
    return true;
}

void AddDemoDrawings(Drawings& pDrawings, const LatLon& pCenter, double pSpan)
{
    pDrawings.setMode(Drawings::Mode::Point);
    pDrawings.addVertex({pCenter.mLat + pSpan * 0.30, pCenter.mLon - pSpan * 0.60});
    pDrawings.setMode(Drawings::Mode::Route);
    pDrawings.addVertex({pCenter.mLat - pSpan * 0.30, pCenter.mLon - pSpan * 0.90});
    pDrawings.addVertex({pCenter.mLat + pSpan * 0.05, pCenter.mLon - pSpan * 0.20});
    pDrawings.addVertex({pCenter.mLat - pSpan * 0.20, pCenter.mLon + pSpan * 0.40});
    pDrawings.finish();
    pDrawings.setMode(Drawings::Mode::Region);
    pDrawings.addVertex({pCenter.mLat + pSpan * 0.10, pCenter.mLon + pSpan * 0.50});
    pDrawings.addVertex({pCenter.mLat + pSpan * 0.35, pCenter.mLon + pSpan * 0.55});
    pDrawings.addVertex({pCenter.mLat + pSpan * 0.30, pCenter.mLon + pSpan * 0.85});
    pDrawings.addVertex({pCenter.mLat + pSpan * 0.20, pCenter.mLon + pSpan * 0.70});
    pDrawings.addVertex({pCenter.mLat + pSpan * 0.05, pCenter.mLon + pSpan * 0.80});
    pDrawings.finish();
    // Yarim kalan rota (turuncu)
    pDrawings.setMode(Drawings::Mode::Route);
    pDrawings.addVertex({pCenter.mLat - pSpan * 0.35, pCenter.mLon + pSpan * 0.10});
    pDrawings.addVertex({pCenter.mLat - pSpan * 0.25, pCenter.mLon + pSpan * 0.25});
}

}  // namespace

int main(int pArgc, char** pArgv)
{
    Options tOptions;
    if (!ParseOptions(pArgc, pArgv, &tOptions))
    {
        std::fprintf(stderr, "usage: map_preview --lat L --lon L --zoom Z --out file.ppm [--size WxH] "
                             "[--xplane DIR] [--no-ui] [--demo-drawings] [--fixes] [--hide LAYER_ID]\n");
        return 2;
    }
    HDC tDc = nullptr;
    if (!CreateGlContext(&tDc) || !CreateFramebuffer(tOptions.mWidth, tOptions.mHeight))
    {
        std::fprintf(stderr, "could not create an OpenGL context / framebuffer\n");
        return 1;
    }
    PreviewInitFont(tDc);
    Gfx::Canvas tCanvas;
    if (!tCanvas.init())
    {
        std::fprintf(stderr, "OpenGL: %s\n", tCanvas.errorText().c_str());
        return 1;
    }
    std::printf("OpenGL: %s\n", tCanvas.contextText().c_str());
    std::unique_ptr<AtlasTextRenderer> tText;
    if (tOptions.mAtlasText)
    {
        int tFontHeight = 10;
        XPLMGetFontDimensions(xplmFont_Proportional, nullptr, &tFontHeight, nullptr);
        tText = std::make_unique<AtlasTextRenderer>(tCanvas, tFontHeight);
        std::string tError;
        if (tText->load(tOptions.mXPlane + "Resources/fonts/DejaVuSans.ttf", &tError))
        {
            Ui::setTextRenderer(tText.get());
        }
        else
        {
            std::printf("text atlas: %s (using stub font)\n", tError.c_str());
            tText.reset();
        }
    }

    // Veri
    const std::chrono::steady_clock::time_point tStart = std::chrono::steady_clock::now();
    NavDatabase tNav;
    const int tNavCount = LoadNavFromFiles(tOptions.mXPlane, tNav);
    AirportGeometryStore tRunways(std::make_unique<AptDatIndexer>());
    tRunways.startLoading(tOptions.mXPlane, tOptions.mCache + "airports/");
    while (!tRunways.poll())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    AirwayDatabase tAirways;
    tAirways.startLoading({tOptions.mXPlane + "Custom Data/earth_awy.dat",
                           tOptions.mXPlane + "Resources/default data/earth_awy.dat"},
                          tNav);
    while (!tAirways.poll())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    std::printf("airways: %s\n", tAirways.lastReport().c_str());
    std::printf("airport index:\n%s", tRunways.lastReport().c_str());
    std::printf("data: %d nav records, %d airports with runways (%.1f s)\n", tNavCount, tRunways.count(),
                std::chrono::duration<double>(std::chrono::steady_clock::now() - tStart).count());

    // Katmanlar: eklentiyle (Plugin.cpp CreateLayers) ayni sira
    AircraftData tAircraft;  // onizlemede ucak yok (dataref yok)
    Drawings tDrawings;
    TerrainPalette tPalette;
    if (!tOptions.mTerrainColors.empty())
    {
        // Ayni ini dosyasiyla eklentinin goreceginin aynisi (ayarlari derlemeden karsilastirmak icin)
        Settings tColors;
        if (tColors.load(tOptions.mTerrainColors))
        {
            tPalette.load(tColors);
        }
        else
        {
            std::fprintf(stderr, "cannot read terrain colors: %s\n", tOptions.mTerrainColors.c_str());
        }
    }
    if (tPalette.usesXPlaneColors())
    {
        // Eklentiyle ayni: sinif renkleri X-Plane'in arazi dokularindan
        XPlaneClassColors tDerived;
        std::string tReport;
        if (loadXPlaneClassColors(tOptions.mXPlane, tOptions.mCache + "xplane_palette.txt", &tDerived, &tReport))
        {
            tPalette.applyDerivedColors(tDerived.mColors, tDerived.mTextureCounts);
        }
        std::printf("%s\n", tReport.c_str());
    }
    TerrainStore tTerrain;
    tTerrain.start(tOptions.mXPlane, tOptions.mCache, tPalette, allTerrainStyles());
    // Dokular / vektorler haritalarin ortak merkezinde. Katmanlardan ONCE tanimli olmali
    // (katman yikicilari merkeze haber verir: merkez onlardan sonra yok olmali).
    TerrainHub tTerrainHub(tTerrain);
    LayerManager tLayers;
    tLayers.add(std::make_unique<TerrainLayer>(tTerrainHub, tPalette));
    Layer* tRoadLayer = tLayers.add(std::make_unique<RoadLayer>(tTerrainHub));
    AirportDetailStore tAirportDetails(tRunways);
    tAirportDetails.start();
    Layer* tSurfaceLayer = tLayers.add(std::make_unique<AirportSurfaceLayer>(tRunways, tAirportDetails));
    tLayers.add(std::make_unique<GridLayer>());
    tLayers.add(std::make_unique<NavaidLayer>(NavaidType::Ils, tNav, &tRunways));
    tLayers.add(std::make_unique<RunwayLayer>(tRunways));
    tLayers.add(std::make_unique<TaxiRouteLayer>(tRunways, tAirportDetails));
    Layer* tObjectsLayer = tLayers.add(std::make_unique<AirportObjectsLayer>(tRunways, tAirportDetails));
    tLayers.add(std::make_unique<AirwayLayer>(tAirways, false));
    tLayers.add(std::make_unique<AirwayLayer>(tAirways, true));
    tLayers.add(std::make_unique<AirportLayer>(tNav, &tRunways));
    tLayers.add(std::make_unique<NavaidLayer>(NavaidType::Vor, tNav));
    tLayers.add(std::make_unique<NavaidLayer>(NavaidType::Ndb, tNav));
    Layer* tFixLayer = tLayers.add(std::make_unique<NavaidLayer>(NavaidType::Fix, tNav));
    tFixLayer->setVisible(tOptions.mFixes);
    tLayers.add(std::make_unique<AircraftLayer>(tAircraft));
    tLayers.add(std::make_unique<DrawingLayer>(tDrawings));
    const XMapTheme& tTheme = themeFor(tOptions.mMode);
    if (tOptions.mMode != XMapMode::Scenery)
    {
        tLayers.loadVisibility(Settings(), tTheme);  // modun varsayilan katmanlari
    }
    for (int tIndex = 0; tIndex < tLayers.count(); ++tIndex)
    {
        Layer* tLayer = tLayers.at(tIndex);
        if (std::find(tOptions.mHidden.begin(), tOptions.mHidden.end(), tLayer->id()) != tOptions.mHidden.end())
        {
            tLayer->setVisible(false);
        }
        if (std::find(tOptions.mShown.begin(), tOptions.mShown.end(), tLayer->id()) != tOptions.mShown.end())
        {
            tLayer->setVisible(true);
        }
    }

    // Gorunum: pencere = tum resim, boxel = piksel
    const int tWidth = tOptions.mWidth;
    const int tHeight = tOptions.mHeight;
    RectI tArea;
    tArea.mLeft = 0;
    tArea.mBottom = 0;
    tArea.mRight = tWidth;
    tArea.mTop = tHeight;
    XMapView tView;
    tView.setViewport(toRectD(tArea));
    tView.setCenter({tOptions.mLat, tOptions.mLon});
    tView.setZoom(tOptions.mZoom);
    if (tOptions.mDemoDrawings)
    {
        const GeoBounds tBounds = tView.visibleBounds();
        AddDemoDrawings(tDrawings, tView.center(), (tBounds.mNorth - tBounds.mSouth) * 0.5);
    }

    // Zemin arka planda uretilir: hazir olana kadar birkac kez "ciz" (istek gonder,
    // gelen goruntuleri yukle). Sonra son kare asagida cizilir.
    for (int tRound = 0; tRound < 600; ++tRound)
    {
        drawContext tWarmup;
        tWarmup.mView = &tView;
        tWarmup.mCanvas = &tCanvas;
        tWarmup.mTheme = &tTheme;
        tWarmup.mArea = toRectD(tArea);
        tWarmup.mClearArea = tWarmup.mArea;
        tTerrainHub.beginFrame();  // karedeki doku yukleme hakki (eklentide flight loop yapar)
        tLayers.at(0)->draw(tWarmup);
        tRoadLayer->draw(tWarmup);  // yol vektorlerini de iste
        tSurfaceLayer->draw(tWarmup);  // havalimani ayrintilarini da iste
        tObjectsLayer->draw(tWarmup);  // dugumsuz heliport'larinki de
        tAirportDetails.update();
        const std::string tLog = tTerrain.takeLogLines() + tAirportDetails.takeLogLines();
        if (!tLog.empty())
        {
            std::printf("%s", tLog.c_str());
        }
        if (tRound > 2 && tTerrain.isIdle() && tAirportDetails.pendingCount() == 0)
        {
            std::printf("terrain ready after %d rounds\n", tRound);
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));  // iscinin son ozet satiri (goruntu sureleri)
    std::printf("%s", tTerrain.takeLogLines().c_str());

    glViewport(0, 0, tWidth, tHeight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, tWidth, 0, tHeight, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    tCanvas.beginFrame();

    Ui::fillRect(tCanvas, tArea, tTheme.mBackground);

    Toolbar tToolbar;
    ToolbarState tToolbarState;
    tToolbarState.mMode = tDrawings.getMode();
    const std::string tModeLabel = std::string("Map: ") + tTheme.mName;
    tToolbarState.mModeLabel = tModeLabel.c_str();
    tToolbarState.mToolsOpen = true;
    tToolbarState.mCanFinish = tDrawings.current() != nullptr;
    tToolbarState.mCanUndo = tDrawings.shapeCount() > 0;
    if (tOptions.mUi)
    {
        tToolbar.layout(tArea, tToolbarState);
    }

    LabelPlacer tLabels;
    drawContext tContext;
    tContext.mView = &tView;
    tContext.mCanvas = &tCanvas;
    tContext.mTheme = &tTheme;
    tContext.mArea = toRectD(tArea);
    tContext.mClearArea = tContext.mArea;
    if (tOptions.mUi)
    {
        tContext.mClearArea.mTop -= tToolbar.height();
        tContext.mClearArea.mBottom += StatusBar::kHeight;
    }
    tContext.mHasCursor = tOptions.mDemoDrawings;  // lastik cizgiyi gostermek icin sabit bir imlec
    tContext.mCursorX = tWidth * 0.62;
    tContext.mCursorY = tHeight * 0.30;
    tLabels.reset(tContext.mClearArea);
    tContext.mLabels = &tLabels;
    bool tCachedDraw = false;
    const std::optional<Affine2D> tBoxelToPixel = readBoxelToPixel();
    if (tOptions.mImageCache && tBoxelToPixel)
    {
        XMapImageCache tImageCache;
        tLayers.prepareAll();
        tCachedDraw = tImageCache.draw(tCanvas, tView, *tBoxelToPixel, 0.0f, true, false, false,
                                       [&](const XMapView& pView, const RectD& pArea)
                                       {
                                           drawContext tImage = tContext;
                                           tImage.mView = &pView;
                                           tImage.mArea = pArea;
                                           tImage.mHasCursor = false;
                                           Gfx::Mesh tBackground;
                                           tBackground.addRect(pArea, tTheme.mBackground);
                                           tCanvas.draw(tBackground);
                                           tLabels.reset(tImage.mClearArea);
                                           tLayers.drawAll(tImage, LayerPass::Cached);
                                       });
        std::printf("image cache: %s\n", tCachedDraw ? "used" : "NOT available");
        if (tCachedDraw)
        {
            tLabels.reset(tContext.mClearArea);
            tLayers.drawAll(tContext, LayerPass::Overlay);
        }
    }
    if (!tCachedDraw)
    {
        tLayers.drawAll(tContext);
    }
    for (const auto& tPoint : tOptions.mDescribe)
    {
        // Resim pikseli (ust = 0) -> boxel (alt = 0)
        const double tY = tHeight - tPoint.second;
        std::printf("describe %.0f,%.0f: %s\n", tPoint.first, tPoint.second,
                    tLayers.describeAt(tContext, tPoint.first, tY).c_str());
    }

    if (tOptions.mUi)
    {
        StatusBar tStatus;
        tToolbar.draw(tCanvas, -1, -1, tToolbarState);
        // Cizim paleti: varsayilan yerinde (sag ust); --palette-collapsed ile katlanmis
        ToolPalette tToolPalette;
        tToolPalette.setCollapsed(tOptions.mPaletteCollapsed);
        RectI tPaletteBounds = tArea;
        tPaletteBounds.mTop -= tToolbar.height() + 4;
        tPaletteBounds.mBottom += StatusBar::kHeight + 4;
        tPaletteBounds.mLeft += 4;
        tPaletteBounds.mRight -= 4;
        tToolPalette.layout(tPaletteBounds, tToolbarState);
        tToolPalette.draw(tCanvas, -1, -1, tToolbarState);
        if (tOptions.mPanel)
        {
            LayerPanel tPanel;
            tPanel.setOpen(true);
            // "--panel Airports" grubu, "--panel layer:runways" katmanin alt listesini acar
            for (const std::string& tKey : tOptions.mPanelGroups)
            {
                tPanel.toggleExpanded(tKey.find(':') == std::string::npos ? "group:" + tKey : tKey);
            }
            tPanel.layout(tToolbar.buttonRect(ToolbarAction::Layers), tArea.mBottom + StatusBar::kHeight + 4, tLayers);
            tPanel.draw(tCanvas, -1, -1, tLayers);
        }
        tStatus.draw(tCanvas, tArea, tView, tView.boxelToLatLon(tContext.mCursorX, tContext.mCursorY), "");
    }
    tCanvas.flushText();
    glFinish();

    if (!WritePpm(tOptions.mOut, tWidth, tHeight))
    {
        std::fprintf(stderr, "could not write %s\n", tOptions.mOut.c_str());
        return 1;
    }
    std::printf("written: %s (%d labels)\n", tOptions.mOut.c_str(), tLabels.placedCount());
    return 0;
}
