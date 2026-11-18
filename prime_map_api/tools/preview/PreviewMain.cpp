#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
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
#include "engine/LayerTable.h"
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
    bool mAtlasText = true;
    bool mImageCache = false;
    bool mUi = true;
    bool mDemoDrawings = false;
    bool mFixes = false;
    std::string mCache = "terrain_cache/";
    std::string mTerrainColors;
    std::vector<std::string> mHidden;
    std::vector<std::string> mLayerZooms;
    bool mNoHardZoom = false;
    std::string mOnly;
    std::string mBenchFile;
    XMapMode mMode = XMapMode::Scenery;
    std::vector<std::string> mShown;
    bool mPaletteCollapsed = false;
    std::vector<std::string> mPanelGroups;
    bool mPanel = false;
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
            pOut->mImageCache = true;
        }
        else if (tArg == "--xplm-text")
        {
            pOut->mAtlasText = false;
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
        else if (tArg == "--bench" && tHasValue)
        {
            pOut->mBenchFile = pArgv[++tIndex];
        }
        else if (tArg == "--only" && tHasValue)
        {
            pOut->mOnly = pArgv[++tIndex];
        }
        else if (tArg == "--no-hard-zoom")
        {
            pOut->mNoHardZoom = true;
        }
        else if (tArg == "--layer-zoom" && tHasValue)
        {
            pOut->mLayerZooms.push_back(pArgv[++tIndex]);
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
    for (int tRow = pHeight - 1; tRow >= 0; --tRow)
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
    pDrawings.setMode(Drawings::Mode::Route);
    pDrawings.addVertex({pCenter.mLat - pSpan * 0.35, pCenter.mLon + pSpan * 0.10});
    pDrawings.addVertex({pCenter.mLat - pSpan * 0.25, pCenter.mLon + pSpan * 0.25});
}
}

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

    AircraftData tAircraft;
    Drawings tDrawings;
    TerrainPalette tPalette;
    if (!tOptions.mTerrainColors.empty())
    {
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
    LayerRules::applyBuiltInZoomRanges(tLayers);
    const XMapTheme& tTheme = themeFor(tOptions.mMode);
    if (tOptions.mMode != XMapMode::Scenery)
    {
        tLayers.loadVisibility(Settings(), tTheme);
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
    if (!tOptions.mOnly.empty())
    {
        for (int tIndex = 0; tIndex < tLayers.count(); ++tIndex)
        {
            Layer& tLayer = *tLayers.at(tIndex);
            const bool tKeep = tLayer.id() == tOptions.mOnly;
            tLayer.setVisible(tKeep);
            for (int tSub = 0; tKeep && tSub < tLayer.subLayerCount(); ++tSub)
            {
                tLayer.setSubLayerVisible(tSub, true);
            }
        }
    }
    if (tOptions.mNoHardZoom)
    {
        for (int tIndex = 0; tIndex < tLayers.count(); ++tIndex)
        {
            Layer& tLayer = *tLayers.at(tIndex);
            tLayer.setBuiltInZoomRange(tLayer.minZoomEff(), tLayer.maxZoomEff(), Layer::kNoMinZoom);
            for (int tSub = 0; tSub < tLayer.subLayerCount(); ++tSub)
            {
                tLayer.setSubLayerBuiltInZoomRange(tSub, tLayer.subMinZoomEff(tSub), tLayer.subMaxZoomEff(tSub),
                                                   Layer::kNoMinZoom);
            }
        }
    }
    for (const std::string& tSpec : tOptions.mLayerZooms)
    {
        const size_t tColon = tSpec.find(':');
        if (tColon == std::string::npos)
        {
            std::fprintf(stderr, "cannot parse --layer-zoom: %s (expected <key>:<min>[-<max>])\n", tSpec.c_str());
            continue;
        }
        const std::string tKey = tSpec.substr(0, tColon);
        const std::string tRange = tSpec.substr(tColon + 1);
        const size_t tDash = tRange.find('-');
        const double tMin = std::atof(tRange.substr(0, tDash).c_str());
        const double tMax = tDash == std::string::npos ? 99.0 : std::atof(tRange.substr(tDash + 1).c_str());
        const std::optional<prime::XMapLayer> tLayer = LayerRules::layerFromKey(tKey);
        if (!tLayer)
        {
            std::fprintf(stderr, "unknown layer key: %s\n", tKey.c_str());
            continue;
        }
        Layer* tHost = LayerRules::findLayer(tLayers, *tLayer);
        const int tSub = LayerRules::subLayerIndex(tLayers, *tLayer);
        if (!tHost)
        {
            continue;
        }
        if (tSub >= 0)
        {
            tHost->setSubLayerZoomRange(tSub, tMin, tMax);
        }
        else
        {
            tHost->setZoomRange(tMin, tMax);
        }
    }

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

    auto tWarmUp = [&]()
    {
        for (int tRound = 0; tRound < 600; ++tRound)
        {
            drawContext tWarmup;
            tWarmup.mView = &tView;
            tWarmup.mCanvas = &tCanvas;
            tWarmup.mTheme = &tTheme;
            tWarmup.mArea = toRectD(tArea);
            tWarmup.mClearArea = tWarmup.mArea;
            tTerrainHub.beginFrame();
            for (Layer* tWarm : {tLayers.at(0), tRoadLayer, tSurfaceLayer, tObjectsLayer})
            {
                if (tWarm->isVisible())
                {
                    tWarm->setCurrentZoom(tView.zoom());
                    tWarm->draw(tWarmup);
                }
            }
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
    };
    tWarmUp();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
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
    if (!tOptions.mBenchFile.empty())
    {
        std::ifstream tSpec(tOptions.mBenchFile);
        std::string tLine;
        while (std::getline(tSpec, tLine))
        {
            std::istringstream tFields(tLine);
            std::string tPlace;
            double tLat = 0.0;
            double tLon = 0.0;
            std::string tOnly;
            if (!(tFields >> tPlace >> tLat >> tLon >> tOnly))
            {
                continue;
            }
            for (int tIndex = 0; tIndex < tLayers.count(); ++tIndex)
            {
                Layer& tLayer = *tLayers.at(tIndex);
                const bool tKeep = tLayer.id() == tOnly;
                tLayer.setVisible(tKeep);
                tLayer.setBuiltInZoomRange(Layer::kNoMinZoom, Layer::kNoMaxZoom, Layer::kNoMinZoom);
                tLayer.clearZoomRange();
                for (int tSub = 0; tSub < tLayer.subLayerCount(); ++tSub)
                {
                    tLayer.setSubLayerVisible(tSub, tKeep || tLayer.isSubLayerVisible(tSub));
                    tLayer.setSubLayerBuiltInZoomRange(tSub, Layer::kNoMinZoom, Layer::kNoMaxZoom, Layer::kNoMinZoom);
                    tLayer.clearSubLayerZoomRange(tSub);
                }
            }
            double tZoom = 0.0;
            while (tFields >> tZoom)
            {
                tView.setCenter({tLat, tLon});
                tView.setZoom(tZoom);
                tWarmUp();
                double tBest = 1e9;
                for (int tRepeat = 0; tRepeat < 3; ++tRepeat)
                {
                    glClear(GL_COLOR_BUFFER_BIT);
                    tCanvas.beginFrame();
                    LabelPlacer tBenchLabels;
                    drawContext tBench;
                    tBench.mView = &tView;
                    tBench.mCanvas = &tCanvas;
                    tBench.mTheme = &tTheme;
                    tBench.mArea = toRectD(tArea);
                    tBench.mClearArea = tBench.mArea;
                    tBenchLabels.reset(tBench.mClearArea);
                    tBench.mLabels = &tBenchLabels;
                    glFinish();
                    const std::chrono::steady_clock::time_point tBegin = std::chrono::steady_clock::now();
                    tLayers.drawAll(tBench);
                    glFinish();
                    tBest = (std::min)(tBest, std::chrono::duration<double, std::milli>(
                                                  std::chrono::steady_clock::now() - tBegin).count());
                }
                std::printf("bench %s %s %.1f %.2f\n", tPlace.c_str(), tOnly.c_str(), tZoom, tBest);
                std::fflush(stdout);
            }
        }
        return 0;
    }

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
    tContext.mHasCursor = tOptions.mDemoDrawings;
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
        const std::chrono::steady_clock::time_point tDrawStart = std::chrono::steady_clock::now();
        tLayers.drawAll(tContext);
        const double tDrawMs =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tDrawStart).count();
        std::printf("draw: %.2f ms\n", tDrawMs);
    }
    for (const auto& tPoint : tOptions.mDescribe)
    {
        const double tY = tHeight - tPoint.second;
        std::printf("describe %.0f,%.0f: %s\n", tPoint.first, tPoint.second,
                    tLayers.describeAt(tContext, tPoint.first, tY).c_str());
    }

    if (tOptions.mUi)
    {
        StatusBar tStatus;
        tToolbar.draw(tCanvas, -1, -1, tToolbarState);
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
