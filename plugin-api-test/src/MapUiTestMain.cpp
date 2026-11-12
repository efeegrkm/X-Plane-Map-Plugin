// MapUiTestMain.cpp
// Prime Map API test eklentisi - ADIM 2: API baglandi.
//
// Bir X-Plane penceresi acar; ortada Prime Map haritasi, etrafinda 4 kaydirma dugmesi
// (yukari / asagi / sol / sag) ve altta 2 zoom dugmesi (+ / -) vardir.
//
// Harita prime::MapLibrary::Get().Init + MapLibrary::CreateMap ile harita alaninin icine Embedded stilinde gomulur;
// butun cagrilari src\MapApiCalls.cpp yapar. "SDK_BOUND" yorumu ile isaretli satirlar
// API baglantisi icin yapilan degisikliklerdir.
// Kod standardi ana projeyle ayni: m/p/t onekleri, suslu parantez alt satirda,
// Turkce ASCII yorumlar, arayuz metinleri Ingilizce.
#include <cstring>
#include <memory>
#include <string>

#include "XPLMDefs.h"
#include "XPLMDisplay.h"
#include "XPLMGraphics.h"
#include "XPLMProcessing.h"
#include "XPLMUtilities.h"

#include <GL/gl.h>

// SDK_BOUND: Prime Map API sarmalayicisi (butun harita cagrilari bu siniftan gecer)
#include "MapApiCalls.h"
#include "MultiMapSelfTest.h"

namespace
{

// --- Dugme kimlikleri ---------------------------------------------------------
enum class Button
{
    PanUp,
    PanDown,
    PanLeft,
    PanRight,
    ZoomIn,
    ZoomOut,
    Count,
};

constexpr int kButtonCount = static_cast<int>(Button::Count);

// Yerlesim sabitleri (boxel)
constexpr int kMargin = 14;      // pencere kenarlarindan bosluk
constexpr int kSideWidth = 54;   // sol / sag kaydirma sutunlarinin genisligi
constexpr int kTopHeight = 46;   // ust serit (baslik + yukari dugmesi)
constexpr int kBottomHeight = 72; // alt serit (zoom dugmeleri + durum satiri)
constexpr int kButtonHalfW = 23; // dugme boyutu 46 x 28
constexpr int kButtonHalfH = 14;
constexpr double kPressFlash = 0.3;  // basilinca vurgu suresi (saniye)

constexpr const char* kButtonLabels[static_cast<int>(Button::Count)] = {
    "^", "v", "<", ">", "+", "-",
};
constexpr const char* kButtonNames[static_cast<int>(Button::Count)] = {
    "Pan up", "Pan down", "Pan left", "Pan right", "Zoom in", "Zoom out",
};

struct Rect
{
    int mLeft = 0;
    int mTop = 0;    // ust kenarin y'si (y yukari artar)
    int mRight = 0;
    int mBottom = 0;
    bool Contains(int pX, int pY) const
    {
        return pX >= mLeft && pX < mRight && pY >= mBottom && pY < mTop;
    }
    int Width() const
    {
        return mRight - mLeft;
    }
    int Height() const
    {
        return mTop - mBottom;
    }
};

class UiPlugin
{
public:
    void Enable();
    void Disable();

private:
    // XPLM callback'leri (statik; refcon = this)
    static void DrawCallback(XPLMWindowID pWindow, void* pRefcon);
    static int ClickCallback(XPLMWindowID pWindow, int pX, int pY, XPLMMouseStatus pStatus, void* pRefcon);
    static void KeyCallback(XPLMWindowID pWindow, char pKey, XPLMKeyFlags pFlags, char pVirtualKey, void* pRefcon,
                            int pLosingFocus);
    static XPLMCursorStatus CursorCallback(XPLMWindowID pWindow, int pX, int pY, void* pRefcon);
    static int WheelCallback(XPLMWindowID pWindow, int pX, int pY, int pWheel, int pClicks, void* pRefcon);

    void ReadWindowRect(XPLMWindowID pWindow);
    void Layout();                        // dugme dikdortgenlerini pencereye gore hesaplar
    void Draw();
    void DrawButton(const Rect& pRect, const char* pLabel, bool pHighlighted);
    void FillRect(const Rect& pRect, float pR, float pG, float pB, float pA) const;
    void FrameRect(const Rect& pRect, float pR, float pG, float pB) const;
    void DrawCenteredText(int pCenterX, int pCenterY, const char* pText, float pColor[3]) const;
    int HandleClick(int pX, int pY, XPLMMouseStatus pStatus);
    void Press(Button pButton);           // dugmeye basildi (API'ye baglaninca islem buraya gelecek)

    XPLMWindowID mWindow = nullptr;
    Rect mWindowRect;                     // son okunan pencere yeri
    Rect mMapRect;                        // haritaya ayrilan alan
    Rect mButtons[static_cast<int>(Button::Count)];
    // SDK_BOUND: harita erisimi (Init / Open / Pan / Zoom ... hepsi bu siniftan)
    MapApiCalls mApi;
    // 0.23: coklu harita / fabrika ozelliklerinin X-Plane icindeki otomatik testi; sadece eklenti
    // klasorunde "selftest.flag" dosyasi varsa calisir (sonuclar Log.txt: "SELFTEST PASS / FAIL")
    std::unique_ptr<MultiMapSelfTest> mSelfTest;
    // SDK_BOUND: pencere boyutlanirsa haritayi yeniden yerlestirmek icin son gonderilen yer
    prime::Geometry mSentGeometry;
    bool mHasSentGeometry = false;
    Button mPressed = Button::PanUp;
    double mPressTime = -1000.0;          // son basma zamani (vurgu icin)
    std::string mStatus = "Starting map...";
};

// --- Yasam dongusu -------------------------------------------------------------

void UiPlugin::Enable()
{
    XPLMCreateWindow_t tParams;
    std::memset(&tParams, 0, sizeof(tParams));
    tParams.structSize = sizeof(tParams);
    // Pencere yeri: sol-ust kose + sag-alt kenar (boxel; y yukari artar)
    tParams.left = 80;
    tParams.top = 900;
    tParams.right = 880;
    tParams.bottom = 380;
    tParams.visible = 1;
    // SDK notu: tum callback'ler verilmeli, hicbiri null birakilmamali
    tParams.drawWindowFunc = DrawCallback;
    tParams.handleKeyFunc = KeyCallback;
    tParams.handleMouseClickFunc = ClickCallback;
    tParams.handleCursorFunc = CursorCallback;
    tParams.handleMouseWheelFunc = WheelCallback;
    tParams.refcon = this;
    // Kendi cercevemizi kendimiz ciziyoruz (xplm_WindowDecorationNone = cizimsiz kare)
    tParams.decorateAsFloatingWindow = xplm_WindowDecorationNone;
    tParams.layer = xplm_WindowLayerFloatingWindows;
    mWindow = XPLMCreateWindowEx(&tParams);
    if (!mWindow)
    {
        XPLMDebugString("[plugin-api-test] ERROR: could not create the window\n");
        return;
    }
    XPLMSetWindowPositioningMode(mWindow, xplm_WindowPositionFree, -1);
    XPLMSetWindowResizingLimits(mWindow, 560, 400, 4000, 4000);
    XPLMDebugString("[plugin-api-test] window created\n");

    // SDK_BOUND: kutuphaneyi baslat ve haritayi harita alaninin yerine ac.
    //   - InitLibrary: MapLibrary::Init; veri yuklemesi arka planda baslar.
    //     mLogPrefix: Log.txt satirlari "[plugin-api-test-lib]" ile baslar.
    //     mCacheDirectory: bos = bu eklentinin klasoru/cache/ (zemin onbellegi oraya yazilir).
    //   - OpenMap: Map::Open; Embedded stil = cercevesiz pencere, bizim harita alaninin
    //     ustune oturur. UI kapali (haritaya kendi paneli gelmez), fare kapali.
    if (mApi.InitLibrary("[plugin-api-test]"))
    {
        ReadWindowRect(mWindow);
        Layout();
        const prime::Geometry tGeometry{mMapRect.mLeft, mMapRect.mTop, mMapRect.Width(),
                                           mMapRect.Height()};
        if (mApi.OpenMap(tGeometry, prime::WindowStyle::Embedded, false, false))
        {
            // SDK_BOUND: baslangic gorunumu (KSEA)
            mApi.SetCenter(47.45, -122.30);
            mApi.SetZoom(11.0);
            mSentGeometry = tGeometry;
            mHasSentGeometry = true;
            mStatus = "Map embedded: Scenery mode, zoom 11, centered on KSEA";
        }
        // Veri klasoru = bu eklentinin klasoru (InitConfig::mDataDirectory bos birakildi)
        mSelfTest = std::make_unique<MultiMapSelfTest>();
        if (!mSelfTest->StartIfRequested(prime::MapLibrary::Get().GetDataDirectory()))
        {
            mSelfTest.reset();
        }
        else
        {
            mStatus = "Map open FAILED (see Log.txt)";
        }
    }
    else
    {
        mStatus = "PrimeMap Init FAILED (see Log.txt)";
    }
}

void UiPlugin::Disable()
{
    // SDK_BOUND: once harita pencereleri kapanir, sonra kutuphane durdurulur (siralama onemli)
    mSelfTest.reset();
    mApi.CloseMap();
    mApi.ShutdownLibrary();
    if (mWindow)
    {
        XPLMDestroyWindow(mWindow);
        mWindow = nullptr;
    }
}

// --- Yerlesim -------------------------------------------------------------------

void UiPlugin::ReadWindowRect(XPLMWindowID pWindow)
{
    XPLMGetWindowGeometry(pWindow, &mWindowRect.mLeft, &mWindowRect.mTop, &mWindowRect.mRight,
                          &mWindowRect.mBottom);
}

// Dugmeleri pencere boyutuna gore dizer: harita alani ortada, kaydirma dugmeleri
// etrafinda (yukarida / asagida / solda / sagda), zoom alt seritte, durum en sagda.
void UiPlugin::Layout()
{
    const int tWindowBottom = mWindowRect.mBottom;
    const int tWindowTop = mWindowRect.mTop;
    const int tWindowLeft = mWindowRect.mLeft;
    const int tWindowRight = mWindowRect.mRight;

    mMapRect.mLeft = tWindowLeft + kMargin + kSideWidth;
    mMapRect.mRight = tWindowRight - kMargin - kSideWidth;
    mMapRect.mTop = tWindowTop - kMargin - kTopHeight;
    mMapRect.mBottom = tWindowBottom + kMargin + kBottomHeight;

    const int tMapCenterX = (mMapRect.mLeft + mMapRect.mRight) / 2;
    const int tMapCenterY = (mMapRect.mTop + mMapRect.mBottom) / 2;
    const int tTopRowCenterY = (mMapRect.mTop + tWindowTop - kMargin) / 2;
    const int tBottomRowCenterY = (mMapRect.mBottom + tWindowBottom + kMargin) / 2;

    auto tSetButton = [this](Button pButton, int pCenterX, int pCenterY)
    {
        Rect& tRect = mButtons[static_cast<int>(pButton)];
        tRect.mLeft = pCenterX - kButtonHalfW;
        tRect.mRight = pCenterX + kButtonHalfW;
        tRect.mBottom = pCenterY - kButtonHalfH;
        tRect.mTop = pCenterY + kButtonHalfH;
    };

    tSetButton(Button::PanUp, tMapCenterX, tTopRowCenterY);
    tSetButton(Button::PanDown, tMapCenterX, tBottomRowCenterY);
    tSetButton(Button::PanLeft, tWindowLeft + kMargin + kSideWidth / 2, tMapCenterY);
    tSetButton(Button::PanRight, tWindowRight - kMargin - kSideWidth / 2, tMapCenterY);
    tSetButton(Button::ZoomIn, mMapRect.mLeft + 60, tBottomRowCenterY);
    tSetButton(Button::ZoomOut, mMapRect.mLeft + 180, tBottomRowCenterY);
}

// --- Cizim -----------------------------------------------------------------------

void UiPlugin::FillRect(const Rect& pRect, float pR, float pG, float pB, float pA) const
{
    glColor4f(pR, pG, pB, pA);
    glBegin(GL_QUADS);
    glVertex2i(pRect.mLeft, pRect.mBottom);
    glVertex2i(pRect.mRight, pRect.mBottom);
    glVertex2i(pRect.mRight, pRect.mTop);
    glVertex2i(pRect.mLeft, pRect.mTop);
    glEnd();
}

void UiPlugin::FrameRect(const Rect& pRect, float pR, float pG, float pB) const
{
    glColor4f(pR, pG, pB, 1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2i(pRect.mLeft + 1, pRect.mBottom + 1);
    glVertex2i(pRect.mRight - 1, pRect.mBottom + 1);
    glVertex2i(pRect.mRight - 1, pRect.mTop - 1);
    glVertex2i(pRect.mLeft + 1, pRect.mTop - 1);
    glEnd();
}

void UiPlugin::DrawCenteredText(int pCenterX, int pCenterY, const char* pText, float pColor[3]) const
{
    const float tWidth = XPLMMeasureString(xplmFont_Basic, pText, static_cast<int>(std::strlen(pText)));
    XPLMDrawString(pColor, pCenterX - static_cast<int>(tWidth / 2.0f), pCenterY - 6,
                   const_cast<char*>(pText), nullptr, xplmFont_Basic);
}

void UiPlugin::DrawButton(const Rect& pRect, const char* pLabel, bool pHighlighted)
{
    if (pHighlighted)
    {
        FillRect(pRect, 0.30f, 0.50f, 0.34f, 1.0f);
    }
    else
    {
        FillRect(pRect, 0.26f, 0.29f, 0.33f, 1.0f);
    }
    FrameRect(pRect, 0.55f, 0.58f, 0.62f);
    float tWhite[3] = {1.0f, 1.0f, 1.0f};
    DrawCenteredText((pRect.mLeft + pRect.mRight) / 2, (pRect.mTop + pRect.mBottom) / 2, pLabel, tWhite);
}

void UiPlugin::Draw()
{
    ReadWindowRect(mWindow);
    Layout();

    // SDK_BOUND: pencere boyutu degistiyse haritayi yeni harita alanina yerlestir.
    // (SetGeometry her karede cagrilmaz; sadece yer degistiginde cagrilir.)
    if (!mHasSentGeometry || mSentGeometry.mLeft != mMapRect.mLeft || mSentGeometry.mTop != mMapRect.mTop
        || mSentGeometry.mWidth != mMapRect.Width() || mSentGeometry.mHeight != mMapRect.Height())
    {
        mApi.SetGeometry(prime::Geometry{mMapRect.mLeft, mMapRect.mTop, mMapRect.Width(),
                                            mMapRect.Height()});
        mSentGeometry = prime::Geometry{mMapRect.mLeft, mMapRect.mTop, mMapRect.Width(),
                                           mMapRect.Height()};
        mHasSentGeometry = true;
    }

    // Durum kaydet: cull kapali (X-Plane acik birakabilir), cizgi kalinligi 1
    GLboolean tCullWasOn = GL_FALSE;
    glGetBooleanv(GL_CULL_FACE, &tCullWasOn);
    glDisable(GL_CULL_FACE);
    XPLMSetGraphicsState(0, 0, 0, 0, 1, 0, 0);

    // Pencere zemini
    XPLMDrawTranslucentDarkBox(mWindowRect.mLeft, mWindowRect.mTop, mWindowRect.mRight, mWindowRect.mBottom);

    // Harita alani: cerceve; harita aciksa ortasindaki "bos" yazisini cizme (harita ustte)
    FillRect(mMapRect, 0.09f, 0.11f, 0.13f, 1.0f);
    FrameRect(mMapRect, 0.45f, 0.48f, 0.52f);
    float tGray[3] = {0.70f, 0.72f, 0.75f};
    if (!mApi.IsOpen())
    {
        DrawCenteredText((mMapRect.mLeft + mMapRect.mRight) / 2, (mMapRect.mTop + mMapRect.mBottom) / 2,
                         "MAP AREA (empty for now)", tGray);
    }

    // Baslik: ust seritte yukari dugmesinin solunda
    float tWhite[3] = {1.0f, 1.0f, 1.0f};
    XPLMDrawString(tWhite, mWindowRect.mLeft + kMargin, mWindowRect.mTop - kMargin - 16,
                   const_cast<char*>("Map API UI Test - step 2 (API connected)"), nullptr, xplmFont_Basic);

    // Dugmeler
    const double tNow = XPLMGetElapsedTime();
    for (int tIndex = 0; tIndex < static_cast<int>(Button::Count); ++tIndex)
    {
        const bool tHighlighted =
            static_cast<Button>(tIndex) == mPressed && (tNow - mPressTime) < kPressFlash;
        DrawButton(mButtons[tIndex], kButtonLabels[tIndex], tHighlighted);
    }

    // Durum satiri: alt seritte zoom dugmelerinin saginda
    XPLMDrawString(tWhite, mMapRect.mLeft + 260, mWindowRect.mBottom + kMargin + 14,
                   const_cast<char*>(mStatus.c_str()), nullptr, xplmFont_Basic);

    if (tCullWasOn == GL_TRUE)
    {
        glEnable(GL_CULL_FACE);
    }
}

// --- Fare / klavye ----------------------------------------------------------------

void UiPlugin::Press(Button pButton)
{
    mPressed = pButton;
    mPressTime = XPLMGetElapsedTime();
    // SDK_BOUND: dugmeler artik Prime Map API cagrilari (MapApiCalls uzerinden).
    // Butun cagrilarin listesi: docs\API_LISTESI.md; sarmalayici: src\MapApiCalls.cpp
    switch (pButton)
    {
        case Button::PanUp:
        {
            mApi.PanUp();
            break;
        }
        case Button::PanDown:
        {
            mApi.PanDown();
            break;
        }
        case Button::PanLeft:
        {
            mApi.PanLeft();
            break;
        }
        case Button::PanRight:
        {
            mApi.PanRight();
            break;
        }
        case Button::ZoomIn:
        {
            mApi.ZoomIn();
            break;
        }
        case Button::ZoomOut:
        {
            mApi.ZoomOut();
            break;
        }
        default:
        {
            break;
        }
    }
    mStatus = std::string("Last action: ") + kButtonNames[static_cast<int>(pButton)];
    XPLMDebugString("[plugin-api-test] button pressed: ");
    XPLMDebugString(kButtonNames[static_cast<int>(pButton)]);
    XPLMDebugString("\n");
}

int UiPlugin::HandleClick(int pX, int pY, XPLMMouseStatus pStatus)
{
    if (pStatus == xplm_MouseDown)
    {
        for (int tIndex = 0; tIndex < static_cast<int>(Button::Count); ++tIndex)
        {
            if (mButtons[tIndex].Contains(pX, pY))
            {
                Press(static_cast<Button>(tIndex));
                return 1;
            }
        }
    }
    return 1;  // pencere icindeki tum tiklamalar bizde kalsin
}

// --- XPLM callback araci ----------------------------------------------------------

void UiPlugin::DrawCallback(XPLMWindowID /*pWindow*/, void* pRefcon)
{
    static_cast<UiPlugin*>(pRefcon)->Draw();
}

int UiPlugin::ClickCallback(XPLMWindowID /*pWindow*/, int pX, int pY, XPLMMouseStatus pStatus, void* pRefcon)
{
    return static_cast<UiPlugin*>(pRefcon)->HandleClick(pX, pY, pStatus);
}

void UiPlugin::KeyCallback(XPLMWindowID /*pWindow*/, char /*pKey*/, XPLMKeyFlags /*pFlags*/, char /*pVirtualKey*/,
                           void* /*pRefcon*/, int /*pLosingFocus*/)
{
}

XPLMCursorStatus UiPlugin::CursorCallback(XPLMWindowID /*pWindow*/, int /*pX*/, int /*pY*/, void* /*pRefcon*/)
{
    return xplm_CursorDefault;
}

int UiPlugin::WheelCallback(XPLMWindowID /*pWindow*/, int /*pX*/, int /*pY*/, int /*pWheel*/, int /*pClicks*/,
                            void* /*pRefcon*/)
{
    return 0;  // tekerlek su an kullanilmiyor; ileride zoom'a baglanabilir
}

std::unique_ptr<UiPlugin> gPlugin;

}  // namespace

// --- X-Plane eklenti girisleri ---------------------------------------------------

PLUGIN_API int XPluginStart(char* pOutName, char* pOutSignature, char* pOutDescription)
{
    std::strcpy(pOutName, "Plugin API Test");
    std::strcpy(pOutSignature, "prime.plugin_api_test");
    std::strcpy(pOutDescription, "UI skeleton for testing the Prime Map API");
    return 1;
}

PLUGIN_API void XPluginStop()
{
}

PLUGIN_API int XPluginEnable()
{
    gPlugin = std::make_unique<UiPlugin>();
    gPlugin->Enable();
    return 1;
}

PLUGIN_API void XPluginDisable()
{
    if (gPlugin)
    {
        gPlugin->Disable();
        gPlugin.reset();
    }
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID /*pFrom*/, int /*pMessage*/, void* /*pParam*/)
{
}
