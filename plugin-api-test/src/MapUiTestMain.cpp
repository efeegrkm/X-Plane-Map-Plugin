// MapUiTestMain.cpp
// Prime XMap API test eklentisi - ADIM 2: API baglandi.
//
// Bir X-Plane penceresi acar; ortada Prime XMap haritasi, etrafinda 4 kaydirma dugmesi
// (yukari / asagi / sol / sag) ve altta 2 zoom dugmesi (+ / -) vardir.
//
// Harita XMapManagerFactory + XMapManager::addMap ile harita alaninin icine Embedded stilinde gomulur;
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

// SDK_BOUND: Prime XMap API sarmalayicisi (butun harita cagrilari bu siniftan gecer)
#include "MapApiCalls.h"
#include "MultiMapSelfTest.h"

namespace
{

// --- Dugme kimlikleri ---------------------------------------------------------
enum class Button
{
    panUp,
    panDown,
    panLeft,
    panRight,
    zoomIn,
    zoomOut,
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
    bool contains(int pX, int pY) const
    {
        return pX >= mLeft && pX < mRight && pY >= mBottom && pY < mTop;
    }
    int width() const
    {
        return mRight - mLeft;
    }
    int height() const
    {
        return mTop - mBottom;
    }
};

class UiPlugin
{
public:
    void enable();
    void disable();

private:
    // XPLM callback'leri (statik; refcon = this)
    static void drawCallback(XPLMWindowID pWindow, void* pRefcon);
    static int clickCallback(XPLMWindowID pWindow, int pX, int pY, XPLMMouseStatus pStatus, void* pRefcon);
    static void keyCallback(XPLMWindowID pWindow, char pKey, XPLMKeyFlags pFlags, char pVirtualKey, void* pRefcon,
                            int pLosingFocus);
    static XPLMCursorStatus cursorCallback(XPLMWindowID pWindow, int pX, int pY, void* pRefcon);
    static int wheelCallback(XPLMWindowID pWindow, int pX, int pY, int pWheel, int pClicks, void* pRefcon);

    void readWindowRect(XPLMWindowID pWindow);
    void layout();                        // dugme dikdortgenlerini pencereye gore hesaplar
    void draw();
    void drawButton(const Rect& pRect, const char* pLabel, bool pHighlighted);
    void fillRect(const Rect& pRect, float pR, float pG, float pB, float pA) const;
    void frameRect(const Rect& pRect, float pR, float pG, float pB) const;
    void drawCenteredText(int pCenterX, int pCenterY, const char* pText, float pColor[3]) const;
    int handleClick(int pX, int pY, XPLMMouseStatus pStatus);
    void press(Button pButton);           // dugmeye basildi (API'ye baglaninca islem buraya gelecek)

    XPLMWindowID mWindow = nullptr;
    Rect mWindowRect;                     // son okunan pencere yeri
    Rect mMapRect;                        // haritaya ayrilan alan
    Rect mButtons[static_cast<int>(Button::Count)];
    // SDK_BOUND: harita erisimi (Init / Open / Pan / Zoom ... hepsi bu siniftan)
    MapApiCalls mApi;
    // 0.24: coklu harita / fabrika ozelliklerinin X-Plane icindeki otomatik testi; sadece eklenti
    // klasorunde "selftest.flag" dosyasi varsa calisir (sonuclar Log.txt: "SELFTEST PASS / FAIL")
    std::unique_ptr<MultiMapSelfTest> mSelfTest;
    // SDK_BOUND: pencere boyutlanirsa haritayi yeniden yerlestirmek icin son gonderilen yer
    prime::Geometry mSentGeometry;
    bool mHasSentGeometry = false;
    Button mPressed = Button::panUp;
    double mPressTime = -1000.0;          // son basma zamani (vurgu icin)
    std::string mStatus = "Starting map...";
};

// --- Yasam dongusu -------------------------------------------------------------

void UiPlugin::enable()
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
    tParams.drawWindowFunc = drawCallback;
    tParams.handleKeyFunc = keyCallback;
    tParams.handleMouseClickFunc = clickCallback;
    tParams.handleCursorFunc = cursorCallback;
    tParams.handleMouseWheelFunc = wheelCallback;
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
    //   - initLibrary: XMapManager::Init; veri yuklemesi arka planda baslar.
    //     mLogPrefix: Log.txt satirlari "[plugin-api-test-lib]" ile baslar.
    //     mCacheDirectory: bos = bu eklentinin klasoru/cache/ (zemin onbellegi oraya yazilir).
    //   - openMap: XMap::Open; Embedded stil = cercevesiz pencere, bizim harita alaninin
    //     ustune oturur. UI kapali (haritaya kendi paneli gelmez), fare kapali.
    if (mApi.initLibrary("[plugin-api-test]"))
    {
        readWindowRect(mWindow);
        layout();
        const prime::Geometry tGeometry{mMapRect.mLeft, mMapRect.mTop, mMapRect.width(),
                                           mMapRect.height()};
        if (mApi.openMap(tGeometry, prime::WindowStyle::Embedded, false, false))
        {
            // SDK_BOUND: baslangic gorunumu (KSEA)
            mApi.setCenter(47.45, -122.30);
            mApi.setZoom(11.0);
            mSentGeometry = tGeometry;
            mHasSentGeometry = true;
            mStatus = "XMap embedded: Scenery mode, zoom 11, centered on KSEA";
        }
        else
        {
            mStatus = "XMap open FAILED (see Log.txt)";
        }
        // Veri klasoru: kutuphane varsayilani (%LOCALAPPDATA%/prime/prime-xmap); bayrak dosyasi orada aranir
        mSelfTest = std::make_unique<MultiMapSelfTest>();
        if (!mSelfTest->startIfRequested(mApi.manager()->getDataDirectory(), mApi.manager()))
        {
            mSelfTest.reset();
        }
    }
    else
    {
        mStatus = "PrimeMap Init FAILED (see Log.txt)";
    }
}

void UiPlugin::disable()
{
    // SDK_BOUND: once harita pencereleri kapanir, sonra kutuphane durdurulur (siralama onemli)
    mSelfTest.reset();
    mApi.closeMap();
    mApi.shutdownLibrary();
    if (mWindow)
    {
        XPLMDestroyWindow(mWindow);
        mWindow = nullptr;
    }
}

// --- Yerlesim -------------------------------------------------------------------

void UiPlugin::readWindowRect(XPLMWindowID pWindow)
{
    XPLMGetWindowGeometry(pWindow, &mWindowRect.mLeft, &mWindowRect.mTop, &mWindowRect.mRight,
                          &mWindowRect.mBottom);
}

// Dugmeleri pencere boyutuna gore dizer: harita alani ortada, kaydirma dugmeleri
// etrafinda (yukarida / asagida / solda / sagda), zoom alt seritte, durum en sagda.
void UiPlugin::layout()
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

    tSetButton(Button::panUp, tMapCenterX, tTopRowCenterY);
    tSetButton(Button::panDown, tMapCenterX, tBottomRowCenterY);
    tSetButton(Button::panLeft, tWindowLeft + kMargin + kSideWidth / 2, tMapCenterY);
    tSetButton(Button::panRight, tWindowRight - kMargin - kSideWidth / 2, tMapCenterY);
    tSetButton(Button::zoomIn, mMapRect.mLeft + 60, tBottomRowCenterY);
    tSetButton(Button::zoomOut, mMapRect.mLeft + 180, tBottomRowCenterY);
}

// --- Cizim -----------------------------------------------------------------------

void UiPlugin::fillRect(const Rect& pRect, float pR, float pG, float pB, float pA) const
{
    glColor4f(pR, pG, pB, pA);
    glBegin(GL_QUADS);
    glVertex2i(pRect.mLeft, pRect.mBottom);
    glVertex2i(pRect.mRight, pRect.mBottom);
    glVertex2i(pRect.mRight, pRect.mTop);
    glVertex2i(pRect.mLeft, pRect.mTop);
    glEnd();
}

void UiPlugin::frameRect(const Rect& pRect, float pR, float pG, float pB) const
{
    glColor4f(pR, pG, pB, 1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2i(pRect.mLeft + 1, pRect.mBottom + 1);
    glVertex2i(pRect.mRight - 1, pRect.mBottom + 1);
    glVertex2i(pRect.mRight - 1, pRect.mTop - 1);
    glVertex2i(pRect.mLeft + 1, pRect.mTop - 1);
    glEnd();
}

void UiPlugin::drawCenteredText(int pCenterX, int pCenterY, const char* pText, float pColor[3]) const
{
    const float tWidth = XPLMMeasureString(xplmFont_Basic, pText, static_cast<int>(std::strlen(pText)));
    XPLMDrawString(pColor, pCenterX - static_cast<int>(tWidth / 2.0f), pCenterY - 6,
                   const_cast<char*>(pText), nullptr, xplmFont_Basic);
}

void UiPlugin::drawButton(const Rect& pRect, const char* pLabel, bool pHighlighted)
{
    if (pHighlighted)
    {
        fillRect(pRect, 0.30f, 0.50f, 0.34f, 1.0f);
    }
    else
    {
        fillRect(pRect, 0.26f, 0.29f, 0.33f, 1.0f);
    }
    frameRect(pRect, 0.55f, 0.58f, 0.62f);
    float tWhite[3] = {1.0f, 1.0f, 1.0f};
    drawCenteredText((pRect.mLeft + pRect.mRight) / 2, (pRect.mTop + pRect.mBottom) / 2, pLabel, tWhite);
}

void UiPlugin::draw()
{
    readWindowRect(mWindow);
    layout();

    // SDK_BOUND: pencere boyutu degistiyse haritayi yeni harita alanina yerlestir.
    // (setGeometry her karede cagrilmaz; sadece yer degistiginde cagrilir.)
    if (!mHasSentGeometry || mSentGeometry.mLeft != mMapRect.mLeft || mSentGeometry.mTop != mMapRect.mTop
        || mSentGeometry.mWidth != mMapRect.width() || mSentGeometry.mHeight != mMapRect.height())
    {
        mApi.setGeometry(prime::Geometry{mMapRect.mLeft, mMapRect.mTop, mMapRect.width(),
                                            mMapRect.height()});
        mSentGeometry = prime::Geometry{mMapRect.mLeft, mMapRect.mTop, mMapRect.width(),
                                           mMapRect.height()};
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
    fillRect(mMapRect, 0.09f, 0.11f, 0.13f, 1.0f);
    frameRect(mMapRect, 0.45f, 0.48f, 0.52f);
    float tGray[3] = {0.70f, 0.72f, 0.75f};
    if (!mApi.isOpen())
    {
        drawCenteredText((mMapRect.mLeft + mMapRect.mRight) / 2, (mMapRect.mTop + mMapRect.mBottom) / 2,
                         "MAP AREA (empty for now)", tGray);
    }

    // Baslik: ust seritte yukari dugmesinin solunda
    float tWhite[3] = {1.0f, 1.0f, 1.0f};
    XPLMDrawString(tWhite, mWindowRect.mLeft + kMargin, mWindowRect.mTop - kMargin - 16,
                   const_cast<char*>("XMap API UI Test - step 2 (API connected)"), nullptr, xplmFont_Basic);

    // Dugmeler
    const double tNow = XPLMGetElapsedTime();
    for (int tIndex = 0; tIndex < static_cast<int>(Button::Count); ++tIndex)
    {
        const bool tHighlighted =
            static_cast<Button>(tIndex) == mPressed && (tNow - mPressTime) < kPressFlash;
        drawButton(mButtons[tIndex], kButtonLabels[tIndex], tHighlighted);
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

void UiPlugin::press(Button pButton)
{
    mPressed = pButton;
    mPressTime = XPLMGetElapsedTime();
    // SDK_BOUND: dugmeler artik Prime XMap API cagrilari (MapApiCalls uzerinden).
    // Butun cagrilarin listesi: docs\API_LISTESI.md; sarmalayici: src\MapApiCalls.cpp
    switch (pButton)
    {
        case Button::panUp:
        {
            mApi.panUp();
            break;
        }
        case Button::panDown:
        {
            mApi.panDown();
            break;
        }
        case Button::panLeft:
        {
            mApi.panLeft();
            break;
        }
        case Button::panRight:
        {
            mApi.panRight();
            break;
        }
        case Button::zoomIn:
        {
            mApi.zoomIn();
            break;
        }
        case Button::zoomOut:
        {
            mApi.zoomOut();
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

int UiPlugin::handleClick(int pX, int pY, XPLMMouseStatus pStatus)
{
    if (pStatus == xplm_MouseDown)
    {
        for (int tIndex = 0; tIndex < static_cast<int>(Button::Count); ++tIndex)
        {
            if (mButtons[tIndex].contains(pX, pY))
            {
                press(static_cast<Button>(tIndex));
                return 1;
            }
        }
    }
    return 1;  // pencere icindeki tum tiklamalar bizde kalsin
}

// --- XPLM callback araci ----------------------------------------------------------

void UiPlugin::drawCallback(XPLMWindowID /*pWindow*/, void* pRefcon)
{
    static_cast<UiPlugin*>(pRefcon)->draw();
}

int UiPlugin::clickCallback(XPLMWindowID /*pWindow*/, int pX, int pY, XPLMMouseStatus pStatus, void* pRefcon)
{
    return static_cast<UiPlugin*>(pRefcon)->handleClick(pX, pY, pStatus);
}

void UiPlugin::keyCallback(XPLMWindowID /*pWindow*/, char /*pKey*/, XPLMKeyFlags /*pFlags*/, char /*pVirtualKey*/,
                           void* /*pRefcon*/, int /*pLosingFocus*/)
{
}

XPLMCursorStatus UiPlugin::cursorCallback(XPLMWindowID /*pWindow*/, int /*pX*/, int /*pY*/, void* /*pRefcon*/)
{
    return xplm_CursorDefault;
}

int UiPlugin::wheelCallback(XPLMWindowID /*pWindow*/, int /*pX*/, int /*pY*/, int /*pWheel*/, int /*pClicks*/,
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
    std::strcpy(pOutDescription, "UI skeleton for testing the Prime XMap API");
    return 1;
}

PLUGIN_API void XPluginStop()
{
}

PLUGIN_API int XPluginEnable()
{
    gPlugin = std::make_unique<UiPlugin>();
    gPlugin->enable();
    return 1;
}

PLUGIN_API void XPluginDisable()
{
    if (gPlugin)
    {
        gPlugin->disable();
        gPlugin.reset();
    }
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID /*pFrom*/, int /*pMessage*/, void* /*pParam*/)
{
}
