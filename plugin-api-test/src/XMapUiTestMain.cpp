// XMapUiTestMain.cpp
// Prime XMap API test eklentisi - ADIM 3: harita bizim penceremizin icinde, openGL dokusu olarak.
//
// Bir X-Plane penceresi acar (X-Plane cercevesi: basliktan tasinir, kenarlardan boyutlanir); ortada
// Prime XMap haritasi, etrafinda 4 kaydirma dugmesi ve altta 2 zoom dugmesi var.
// Harita WindowStyle::Texture ile acilir: ayri bir pencere YOKTUR; kutuphane haritayi bir openGL dokusuna
// cizer (renderTexture), biz kendi cizimimizde harita alanina basariz. Pencere tasininca / boyutlaninca
// harita ayni karede onunla gelir. Haritanin fare / tekerlek / klavye girdisi doku koordinatina cevrilip
// API'ye iletilir (inject*).
// Butun cagrilari src\XMapApiCalls.cpp yapar. "SDK_BOUND" yorumu ile isaretli satirlar
// API baglantisi icin yapilan degisikliklerdir.
// Kod standardi ana projeyle ayni: m/p/t onekleri, suslu parantez alt satirda,
// Turkce ASCII yorumlar, arayuz metinleri Ingilizce.
#include <array>
#include <cstring>
#include <memory>
#include <string>

#include "XPLMDefs.h"
#include "XPLMDisplay.h"
#include "XPLMGraphics.h"
#include "XPLMPlugin.h"
#include "XPLMProcessing.h"
#include "XPLMUtilities.h"

#include <GL/gl.h>

// SDK_BOUND: Prime XMap API sarmalayicisi (butun harita cagrilari bu siniftan gecer)
#include "XMapApiCalls.h"
#include "MultiXMapSelfTest.h"

namespace
{

// Bu eklentinin klasoru: .../Resources/plugins/plugin-api-test/ (sonunda ayirici ile)
std::string findPluginDirectory()
{
    std::array<char, 2048> tXplPath = {};  // SDK: en az 256 bayt
    XPLMGetPluginInfo(XPLMGetMyID(), nullptr, tXplPath.data(), nullptr, nullptr);
    std::string tPath = tXplPath.data();
    for (int tLevel = 0; tLevel < 2; ++tLevel)  // dosya adini ve "64" klasorunu at
    {
        const size_t tCut = tPath.find_last_of("/\\");
        if (tCut != std::string::npos)
        {
            tPath.erase(tCut);
        }
    }
    return tPath + XPLMGetDirectorySeparator();
}

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
constexpr double kStartLat = 47.45;  // baslangic gorunumu: KSEA
constexpr double kStartLon = -122.30;
constexpr double kStartZoom = 11.0;

constexpr const char* kButtonLabels[kButtonCount] = {
    "^", "v", "<", ">", "+", "-",
};
constexpr const char* kButtonNames[kButtonCount] = {
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
    static int rightClickCallback(XPLMWindowID pWindow, int pX, int pY, XPLMMouseStatus pStatus, void* pRefcon);
    static void keyCallback(XPLMWindowID pWindow, char pKey, XPLMKeyFlags pFlags, char pVirtualKey, void* pRefcon,
                            int pLosingFocus);
    static XPLMCursorStatus cursorCallback(XPLMWindowID pWindow, int pX, int pY, void* pRefcon);
    static int wheelCallback(XPLMWindowID pWindow, int pX, int pY, int pWheel, int pClicks, void* pRefcon);

    void readWindowRect(XPLMWindowID pWindow);
    void layout();                        // dugme dikdortgenlerini pencereye gore hesaplar
    void draw();
    void drawMapTexture();                // harita dokusunu harita alanina basar
    void drawButton(const Rect& pRect, const char* pLabel, bool pHighlighted);
    void fillRect(const Rect& pRect, float pR, float pG, float pB, float pA) const;
    void frameRect(const Rect& pRect, float pR, float pG, float pB) const;
    void drawCenteredText(int pCenterX, int pCenterY, const char* pText, float pColor[3]) const;
    int handleClick(int pX, int pY, XPLMMouseStatus pStatus);
    int handleRightClick(int pX, int pY, XPLMMouseStatus pStatus);
    int handleWheel(int pX, int pY, int pClicks);
    void handleCursor(int pX, int pY);
    void handleKey(char pKey, XPLMKeyFlags pFlags, char pVirtualKey, bool pLosingFocus);
    // Harita klavye istemiyorsa odagi birakir; pTake: istiyorsa al (sadece haritaya tiklaninca; her karede
    // alinirsa kullanici X-Plane'e tiklayinca klavye geri donmez)
    void syncKeyboardFocus(bool pTake);
    void press(Button pButton);           // dugmeye basildi: API cagrisi

    XPLMWindowID mWindow = nullptr;
    Rect mWindowRect;                     // son okunan pencere yeri
    Rect mMapRect;                        // haritaya ayrilan alan
    Rect mButtons[kButtonCount];
    // SDK_BOUND: harita erisimi (Init / Open / Pan / Zoom ... hepsi bu siniftan)
    XMapApiCalls mApi;
    // 0.25: coklu harita / fabrika ozelliklerinin X-Plane icindeki otomatik testi; sadece eklenti
    // klasorunde "selftest.flag" dosyasi varsa calisir (sonuclar Log.txt: "SELFTEST PASS / FAIL")
    std::unique_ptr<MultiXMapSelfTest> mSelfTest;
    bool mMapPress = false;               // basma haritada basladi (surukleme / birakma da haritaya)
    bool mMapRightPress = false;
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
    tParams.handleRightClickFunc = rightClickCallback;
    tParams.handleCursorFunc = cursorCallback;
    tParams.handleMouseWheelFunc = wheelCallback;
    tParams.refcon = this;
    // X-Plane cercevesi: kullanici basliktan tasir, kenarlardan boyutlandirir
    tParams.decorateAsFloatingWindow = xplm_WindowDecorationRoundRectangle;
    tParams.layer = xplm_WindowLayerFloatingWindows;
    mWindow = XPLMCreateWindowEx(&tParams);
    if (!mWindow)
    {
        XPLMDebugString("[plugin-api-test] ERROR: could not create the window\n");
        return;
    }
    XPLMSetWindowPositioningMode(mWindow, xplm_WindowPositionFree, -1);
    XPLMSetWindowResizingLimits(mWindow, 560, 400, 4000, 4000);
    XPLMSetWindowTitle(mWindow, const_cast<char*>("XMap API UI Test"));
    XPLMDebugString("[plugin-api-test] window created\n");

    // SDK_BOUND: kutuphaneyi baslat ve haritayi dokuya cizilecek sekilde ac.
    //   - initLibrary: XMapManager::initialize; veri yuklemesi arka planda baslar. Veri klasoru bu
    //     eklentinin klasoru: ayarlar (prime_map.json, maps/) ve zemin onbellegi (cache/) resmi eklentininkinden
    //     (%LOCALAPPDATA%) ayri kalir; selftest.flag de burada aranir.
    //   - openXMap: WindowStyle::Texture; haritanin kendi arayuzu ve faresi acik (girdi bizden iletilir).
    if (!mApi.initLibrary("[plugin-api-test]", findPluginDirectory()))
    {
        mStatus = "PrimeMap Init FAILED (see Log.txt)";
        return;
    }
    readWindowRect(mWindow);
    layout();
    const prime::Geometry tGeometry{mMapRect.mLeft, mMapRect.mTop, mMapRect.width(), mMapRect.height()};
    if (mApi.openXMap(tGeometry, prime::WindowStyle::Texture, true, true))
    {
        // SDK_BOUND: doku boyutu (boxel; piksel UI olcegine gore otomatik) ve baslangic gorunumu (KSEA)
        mApi.setTextureSize(mMapRect.width(), mMapRect.height());
        mApi.setFollowAircraft(false);
        mApi.setCenter(kStartLat, kStartLon);
        mApi.setZoom(kStartZoom);
        mStatus = "XMap drawn into a texture inside this window";
    }
    else
    {
        mStatus = "XMap open FAILED (see Log.txt)";
    }
    mSelfTest = std::make_unique<MultiXMapSelfTest>();
    if (!mSelfTest->startIfRequested(mApi.manager()->getDataDirectory(), mApi.manager()))
    {
        mSelfTest.reset();
    }
}

void UiPlugin::disable()
{
    // SDK_BOUND: once haritalar kapanir, sonra kutuphane durdurulur (siralama onemli)
    mSelfTest.reset();
    if (mWindow && XPLMHasKeyboardFocus(mWindow))
    {
        XPLMTakeKeyboardFocus(nullptr);
    }
    mApi.closeXMap();
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
    // Yazi (XPLMDrawString) grafik durumunu degistirir: her dugmenin dolgusundan once dokusuz duruma don
    XPLMSetGraphicsState(0, 0, 0, 0, 1, 0, 0);
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

void UiPlugin::drawMapTexture()
{
    // SDK_BOUND: harita dokusu bizim cizim callback'imiz icinde guncellenir (gerekiyorsa) ve basilir
    if (!mApi.renderTexture())
    {
        return;
    }
    const unsigned int tTexture = mApi.getTextureId();
    if (tTexture == 0)
    {
        return;
    }
    XPLMSetGraphicsState(0, 1, 0, 0, 0, 0, 0);  // 1 doku birimi, saydamlik yok (harita opak)
    XPLMBindTexture2d(static_cast<int>(tTexture), 0);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);  // doku: v = 0 alt kenar
    glTexCoord2f(0.0f, 0.0f);
    glVertex2i(mMapRect.mLeft, mMapRect.mBottom);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2i(mMapRect.mRight, mMapRect.mBottom);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2i(mMapRect.mRight, mMapRect.mTop);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2i(mMapRect.mLeft, mMapRect.mTop);
    glEnd();
}

void UiPlugin::draw()
{
    readWindowRect(mWindow);
    layout();
    // SDK_BOUND: harita alaninin boyutu (yer bizim isimiz; ayni boyutta kutuphane bir sey yapmaz)
    mApi.setTextureSize(mMapRect.width(), mMapRect.height());
    syncKeyboardFocus(false);

    // Durum kaydet: cull kapali (X-Plane acik birakabilir), cizgi kalinligi 1
    GLboolean tCullWasOn = GL_FALSE;
    glGetBooleanv(GL_CULL_FACE, &tCullWasOn);
    glDisable(GL_CULL_FACE);

    // Pencere zemini (X-Plane cercevesinin ici)
    XPLMDrawTranslucentDarkBox(mWindowRect.mLeft, mWindowRect.mTop, mWindowRect.mRight, mWindowRect.mBottom);

    // Harita alani: zemin, harita dokusu, en ustte cerceve. XPLMDrawTranslucentDarkBox doku kullanir:
    // duz renk dolgudan once dokusuz duruma donulmezse dolgu siyah cikar.
    XPLMSetGraphicsState(0, 0, 0, 0, 1, 0, 0);
    fillRect(mMapRect, 0.09f, 0.11f, 0.13f, 1.0f);
    float tGray[3] = {0.70f, 0.72f, 0.75f};
    if (mApi.isOpen())
    {
        drawMapTexture();
    }
    else
    {
        drawCenteredText((mMapRect.mLeft + mMapRect.mRight) / 2, (mMapRect.mTop + mMapRect.mBottom) / 2,
                         "MAP AREA (map not open)", tGray);
    }
    XPLMSetGraphicsState(0, 0, 0, 0, 1, 0, 0);
    frameRect(mMapRect, 0.45f, 0.48f, 0.52f);

    // Baslik: ust seritte yukari dugmesinin solunda
    float tWhite[3] = {1.0f, 1.0f, 1.0f};
    XPLMDrawString(tWhite, mWindowRect.mLeft + kMargin, mWindowRect.mTop - kMargin - 16,
                   const_cast<char*>("XMap API UI Test - step 3 (map texture)"), nullptr, xplmFont_Basic);

    // Dugmeler
    const double tNow = XPLMGetElapsedTime();
    for (int tIndex = 0; tIndex < kButtonCount; ++tIndex)
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
    // SDK_BOUND: dugmeler Prime XMap API cagrilari (XMapApiCalls uzerinden).
    // Butun cagrilarin listesi: docs\API_LISTESI.md; sarmalayici: src\XMapApiCalls.cpp
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
        for (int tIndex = 0; tIndex < kButtonCount; ++tIndex)
        {
            if (mButtons[tIndex].contains(pX, pY))
            {
                press(static_cast<Button>(tIndex));
                return 1;
            }
        }
        mMapPress = mApi.isOpen() && mMapRect.contains(pX, pY);
    }
    if (mMapPress)
    {
        // SDK_BOUND: haritada baslayan basma / surukleme / birakma doku koordinatiyla iletilir
        // (surukleme harita alanindan tassa da devam eder)
        prime::MouseAction tAction = prime::MouseAction::Down;
        if (pStatus == xplm_MouseDrag)
        {
            tAction = prime::MouseAction::Drag;
        }
        else if (pStatus == xplm_MouseUp)
        {
            tAction = prime::MouseAction::Up;
            mMapPress = false;
        }
        mApi.injectMouse(pX - mMapRect.mLeft, pY - mMapRect.mBottom, tAction, prime::MouseButton::Left);
        syncKeyboardFocus(tAction == prime::MouseAction::Down);  // arama paneline tiklandiysa yazmaya hazir
    }
    return 1;  // pencere icindeki tum tiklamalar bizde kalsin
}

int UiPlugin::handleRightClick(int pX, int pY, XPLMMouseStatus pStatus)
{
    if (pStatus == xplm_MouseDown)
    {
        mMapRightPress = mApi.isOpen() && mMapRect.contains(pX, pY);
    }
    if (mMapRightPress)
    {
        // SDK_BOUND: sag tik (rota / bolge bitirme)
        prime::MouseAction tAction = prime::MouseAction::Down;
        if (pStatus == xplm_MouseDrag)
        {
            tAction = prime::MouseAction::Drag;
        }
        else if (pStatus == xplm_MouseUp)
        {
            tAction = prime::MouseAction::Up;
            mMapRightPress = false;
        }
        mApi.injectMouse(pX - mMapRect.mLeft, pY - mMapRect.mBottom, tAction, prime::MouseButton::Right);
    }
    return 1;
}

int UiPlugin::handleWheel(int pX, int pY, int pClicks)
{
    if (!mApi.isOpen() || !mMapRect.contains(pX, pY))
    {
        return 0;
    }
    // SDK_BOUND: imlecin altindaki nokta yerinde kalir (kutuphane zoomAt)
    mApi.injectWheel(pX - mMapRect.mLeft, pY - mMapRect.mBottom, pClicks);
    return 1;
}

void UiPlugin::handleCursor(int pX, int pY)
{
    if (mApi.isOpen() && mMapRect.contains(pX, pY))
    {
        // SDK_BOUND: imlec haritanin ustunde: tooltip ve vurgu icin her karede
        mApi.injectMouseMove(pX - mMapRect.mLeft, pY - mMapRect.mBottom);
    }
}

void UiPlugin::handleKey(char pKey, XPLMKeyFlags pFlags, char pVirtualKey, bool pLosingFocus)
{
    if (pLosingFocus)
    {
        return;
    }
    // SDK_BOUND: harita klavye istiyorsa (arama paneli) tuslar aynen iletilir
    mApi.injectKey(pKey, static_cast<int>(pFlags), pVirtualKey);
    syncKeyboardFocus(false);  // Esc / Enter paneli kapattiysa odak X-Plane'e doner
}

void UiPlugin::syncKeyboardFocus(bool pTake)
{
    // SDK_BOUND: klavye odagi bizim penceremizde; harita istedigi surece (arama paneli) tutariz
    const bool tWants = mApi.wantsKeyboard();
    const bool tHas = XPLMHasKeyboardFocus(mWindow) != 0;
    if (pTake && tWants && !tHas)
    {
        XPLMTakeKeyboardFocus(mWindow);
    }
    else if (!tWants && tHas)
    {
        XPLMTakeKeyboardFocus(nullptr);  // tuslar X-Plane'e geri
    }
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

int UiPlugin::rightClickCallback(XPLMWindowID /*pWindow*/, int pX, int pY, XPLMMouseStatus pStatus, void* pRefcon)
{
    return static_cast<UiPlugin*>(pRefcon)->handleRightClick(pX, pY, pStatus);
}

void UiPlugin::keyCallback(XPLMWindowID /*pWindow*/, char pKey, XPLMKeyFlags pFlags, char pVirtualKey, void* pRefcon,
                           int pLosingFocus)
{
    static_cast<UiPlugin*>(pRefcon)->handleKey(pKey, pFlags, pVirtualKey, pLosingFocus != 0);
}

XPLMCursorStatus UiPlugin::cursorCallback(XPLMWindowID /*pWindow*/, int pX, int pY, void* pRefcon)
{
    static_cast<UiPlugin*>(pRefcon)->handleCursor(pX, pY);
    return xplm_CursorDefault;
}

int UiPlugin::wheelCallback(XPLMWindowID /*pWindow*/, int pX, int pY, int pWheel, int pClicks, void* pRefcon)
{
    if (pWheel != 0)
    {
        return 0;  // yatay tekerlek: kullanilmiyor
    }
    return static_cast<UiPlugin*>(pRefcon)->handleWheel(pX, pY, pClicks);
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
