// PreviewMain.cpp
// map_preview: eklentinin harita katmanlarini X-Plane OLMADAN, gercek X-Plane
// verisiyle bir resme cizer. Gelistirirken gorsel kontrol icin.
//
//   map_preview --lat 47.45 --lon -122.31 --zoom 13 --out kSEA.ppm [--size 1200x800]
//               [--xplane "C:/X-Plane 11/"] [--no-ui] [--demo-drawings] [--fixes]
//
// Cikti PPM (P6). PNG'ye cevirmek icin: magick kSEA.ppm kSEA.png
// Not: ucak ve FMS onizlemede yok (SDK yok); nav verisi dosyalardan okunur.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "Drawings.h"
#include "GLInclude.h"
#include "LabelPlacer.h"
#include "LayerManager.h"
#include "MapView.h"
#include "data/AircraftData.h"
#include "data/AirportGeometryStore.h"
#include "data/AirwayDatabase.h"
#include "data/AptDatReader.h"
#include "data/NavDatabase.h"
#include "layers/AircraftLayer.h"
#include "layers/AirportLayer.h"
#include "layers/AirwayLayer.h"
#include "layers/DrawingLayer.h"
#include "layers/GridLayer.h"
#include "layers/NavaidLayer.h"
#include "layers/RunwayLayer.h"
#include "tools/preview/FileNavLoader.h"
#include "tools/preview/XplmStubs.h"
#include "ui/StatusBar.h"
#include "ui/Toolbar.h"
#include "ui/UiDraw.h"

namespace
{

// --- OpenGL 3.0 framebuffer fonksiyonlari (opengl32.dll sadece 1.1 verir) ---
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

const Rgba kBackground = {0.13f, 0.15f, 0.18f, 1.0f};  // MapWindow ile ayni

struct Options
{
    double mLat = 47.45;
    double mLon = -122.31;
    double mZoom = 11.0;
    int mWidth = 1200;
    int mHeight = 800;
    std::string mOut = "preview.ppm";
    std::string mXPlane = "C:/X-Plane 11/";
    bool mUi = true;
    bool mDemoDrawings = false;
    bool mFixes = false;
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
        else if (tArg == "--no-ui")
        {
            pOut->mUi = false;
        }
        else if (tArg == "--demo-drawings")
        {
            pOut->mDemoDrawings = true;
        }
        else if (tArg == "--fixes")
        {
            pOut->mFixes = true;
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
    pDrawings.SetMode(Drawings::Mode::Point);
    pDrawings.AddVertex({pCenter.mLat + pSpan * 0.30, pCenter.mLon - pSpan * 0.60});
    pDrawings.SetMode(Drawings::Mode::Route);
    pDrawings.AddVertex({pCenter.mLat - pSpan * 0.30, pCenter.mLon - pSpan * 0.90});
    pDrawings.AddVertex({pCenter.mLat + pSpan * 0.05, pCenter.mLon - pSpan * 0.20});
    pDrawings.AddVertex({pCenter.mLat - pSpan * 0.20, pCenter.mLon + pSpan * 0.40});
    pDrawings.Finish();
    pDrawings.SetMode(Drawings::Mode::Region);
    pDrawings.AddVertex({pCenter.mLat + pSpan * 0.10, pCenter.mLon + pSpan * 0.50});
    pDrawings.AddVertex({pCenter.mLat + pSpan * 0.35, pCenter.mLon + pSpan * 0.55});
    pDrawings.AddVertex({pCenter.mLat + pSpan * 0.30, pCenter.mLon + pSpan * 0.85});
    pDrawings.AddVertex({pCenter.mLat + pSpan * 0.20, pCenter.mLon + pSpan * 0.70});
    pDrawings.AddVertex({pCenter.mLat + pSpan * 0.05, pCenter.mLon + pSpan * 0.80});
    pDrawings.Finish();
    // Yarim kalan rota (turuncu)
    pDrawings.SetMode(Drawings::Mode::Route);
    pDrawings.AddVertex({pCenter.mLat - pSpan * 0.35, pCenter.mLon + pSpan * 0.10});
    pDrawings.AddVertex({pCenter.mLat - pSpan * 0.25, pCenter.mLon + pSpan * 0.25});
}

}  // namespace

int main(int pArgc, char** pArgv)
{
    Options tOptions;
    if (!ParseOptions(pArgc, pArgv, &tOptions))
    {
        std::fprintf(stderr, "usage: map_preview --lat L --lon L --zoom Z --out file.ppm [--size WxH] "
                             "[--xplane DIR] [--no-ui] [--demo-drawings] [--fixes]\n");
        return 2;
    }
    HDC tDc = nullptr;
    if (!CreateGlContext(&tDc) || !CreateFramebuffer(tOptions.mWidth, tOptions.mHeight))
    {
        std::fprintf(stderr, "could not create an OpenGL context / framebuffer\n");
        return 1;
    }
    PreviewInitFont(tDc);

    // --- Veri ---
    const std::chrono::steady_clock::time_point tStart = std::chrono::steady_clock::now();
    NavDatabase tNav;
    const int tNavCount = LoadNavFromFiles(tOptions.mXPlane, tNav);
    AirportGeometryStore tRunways(std::make_unique<AptDatReader>());
    tRunways.StartLoading(tOptions.mXPlane);
    while (!tRunways.Poll())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    AirwayDatabase tAirways;
    tAirways.StartLoading({tOptions.mXPlane + "Custom Data/earth_awy.dat",
                           tOptions.mXPlane + "Resources/default data/earth_awy.dat"},
                          tNav);
    while (!tAirways.Poll())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    std::printf("airways: %s\n", tAirways.LastReport().c_str());
    std::printf("data: %d nav records, %d airports with runways (%.1f s)\n", tNavCount, tRunways.Count(),
                std::chrono::duration<double>(std::chrono::steady_clock::now() - tStart).count());

    // --- Katmanlar: eklentiyle (Plugin.cpp CreateLayers) ayni sira ---
    AircraftData tAircraft;  // onizlemede ucak yok (dataref yok)
    Drawings tDrawings;
    LayerManager tLayers;
    tLayers.Add(std::make_unique<GridLayer>());
    tLayers.Add(std::make_unique<NavaidLayer>(NavaidType::Ils, tNav, &tRunways));
    tLayers.Add(std::make_unique<RunwayLayer>(tRunways));
    tLayers.Add(std::make_unique<AirwayLayer>(tAirways, false));
    tLayers.Add(std::make_unique<AirwayLayer>(tAirways, true));
    tLayers.Add(std::make_unique<AirportLayer>(tNav));
    tLayers.Add(std::make_unique<NavaidLayer>(NavaidType::Vor, tNav));
    tLayers.Add(std::make_unique<NavaidLayer>(NavaidType::Ndb, tNav));
    Layer* tFixLayer = tLayers.Add(std::make_unique<NavaidLayer>(NavaidType::Fix, tNav));
    tFixLayer->SetVisible(tOptions.mFixes);
    tLayers.Add(std::make_unique<AircraftLayer>(tAircraft));
    tLayers.Add(std::make_unique<DrawingLayer>(tDrawings));

    // --- Gorunum: pencere = tum resim, boxel = piksel ---
    const int tWidth = tOptions.mWidth;
    const int tHeight = tOptions.mHeight;
    RectI tArea;
    tArea.mLeft = 0;
    tArea.mBottom = 0;
    tArea.mRight = tWidth;
    tArea.mTop = tHeight;
    MapView tView;
    tView.SetViewport(ToRectD(tArea));
    tView.SetCenter({tOptions.mLat, tOptions.mLon});
    tView.SetZoom(tOptions.mZoom);
    if (tOptions.mDemoDrawings)
    {
        const GeoBounds tBounds = tView.VisibleBounds();
        AddDemoDrawings(tDrawings, tView.Center(), (tBounds.mNorth - tBounds.mSouth) * 0.5);
    }

    glViewport(0, 0, tWidth, tHeight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, tWidth, 0, tHeight, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    Ui::FillRect(tArea, kBackground);

    Toolbar tToolbar;
    ToolbarState tToolbarState;
    tToolbarState.mMode = tDrawings.GetMode();
    tToolbarState.mCanFinish = tDrawings.Current() != nullptr;
    tToolbarState.mCanUndo = tDrawings.ShapeCount() > 0;
    if (tOptions.mUi)
    {
        tToolbar.Layout(tArea, tToolbarState);
    }

    LabelPlacer tLabels;
    DrawContext tContext;
    tContext.mView = &tView;
    tContext.mArea = ToRectD(tArea);
    tContext.mClearArea = tContext.mArea;
    if (tOptions.mUi)
    {
        tContext.mClearArea.mTop -= tToolbar.Height();
        tContext.mClearArea.mBottom += StatusBar::kHeight;
    }
    tContext.mHasCursor = tOptions.mDemoDrawings;  // lastik cizgiyi gostermek icin sabit bir imlec
    tContext.mCursorX = tWidth * 0.62;
    tContext.mCursorY = tHeight * 0.30;
    tLabels.Reset(tContext.mClearArea);
    tContext.mLabels = &tLabels;
    tLayers.DrawAll(tContext);

    if (tOptions.mUi)
    {
        StatusBar tStatus;
        tToolbar.Draw(-1, -1, tToolbarState);
        tStatus.Draw(tArea, tView, true, tView.BoxelToLatLon(tContext.mCursorX, tContext.mCursorY), "");
    }
    glFinish();

    if (!WritePpm(tOptions.mOut, tWidth, tHeight))
    {
        std::fprintf(stderr, "could not write %s\n", tOptions.mOut.c_str());
        return 1;
    }
    std::printf("written: %s (%d labels)\n", tOptions.mOut.c_str(), tLabels.PlacedCount());
    return 0;
}
