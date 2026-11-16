// TextureHost.cpp
// Prime XMap API'nin en kucuk tam ornegi: bir X-Plane eklentisi kendi penceresinin icine haritayi
// openGL dokusu olarak (WindowStyle::Texture) yerlestirir, fareyi / tekerlegi / klavyeyi haritaya iletir
// ve haritayi API ile ozellestirir (mod, katman, zoom araligi, renk, zemin).
//
// Akis:  XPluginEnable  -> pencere + XMapManager (initialize) + XMap (addXMap -> ayarlar -> initialize)
//        cizim callback -> setTextureSize + renderTexture + dokuyu dortgene bas
//        girdi callback -> doku koordinatina cevir -> inject*
//        XPluginDisable -> removeXMap -> shutdown -> destroyXMapManager -> pencere
// Kod standardi: m/p/t/k/g onekleri, suslu parantez alt satirda, Turkce ASCII yorumlar, arayuz Ingilizce.
#include <array>
#include <cstring>
#include <memory>
#include <string>

#include "XPLMDefs.h"
#include "XPLMDisplay.h"
#include "XPLMGraphics.h"
#include "XPLMPlugin.h"
#include "XPLMUtilities.h"

#include <GL/gl.h>

#include <prime_map_api/PrimeMap.h>

namespace
{

constexpr int kMargin = 8;  // pencere kenari ile harita arasi (boxel)

// Bu eklentinin klasoru: .../Resources/plugins/texture-host/ (sonunda ayirici ile)
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

class TextureHost
{
public:
    bool enable();
    void disable();

private:
    static void drawCallback(XPLMWindowID pWindow, void* pRefcon);
    static int clickCallback(XPLMWindowID pWindow, int pX, int pY, XPLMMouseStatus pStatus, void* pRefcon);
    static int rightClickCallback(XPLMWindowID pWindow, int pX, int pY, XPLMMouseStatus pStatus, void* pRefcon);
    static void keyCallback(XPLMWindowID pWindow, char pKey, XPLMKeyFlags pFlags, char pVirtualKey, void* pRefcon,
                            int pLosingFocus);
    static XPLMCursorStatus cursorCallback(XPLMWindowID pWindow, int pX, int pY, void* pRefcon);
    static int wheelCallback(XPLMWindowID pWindow, int pX, int pY, int pWheel, int pClicks, void* pRefcon);

    void customizeMap();                     // ozellestirme ornekleri
    void updateMapRect();                    // pencereden harita dikdortgeni
    void draw();
    int forwardClick(int pX, int pY, XPLMMouseStatus pStatus, prime::MouseButton pButton, bool* pPressed);
    void syncKeyboardFocus(bool pTake);

    XPLMWindowID mWindow = nullptr;
    prime::XMapManager* mManager = nullptr;  // sahibi biz: destroyXMapManager ile birakilir
    prime::XMap* mXMap = nullptr;            // sahibi mManager: removeXMap ile birakilir
    int mMapLeft = 0;                        // haritanin pencere icindeki yeri (global boxel)
    int mMapBottom = 0;
    int mMapWidth = 0;
    int mMapHeight = 0;
    bool mLeftPressed = false;               // basma haritada basladi: surukleme / birakma da haritaya
    bool mRightPressed = false;
};

bool TextureHost::enable()
{
    // 1) Host'un kendi penceresi: X-Plane cercevesi (tasinir, boyutlanir)
    XPLMCreateWindow_t tParams;
    std::memset(&tParams, 0, sizeof(tParams));
    tParams.structSize = sizeof(tParams);
    tParams.left = 100;
    tParams.top = 800;
    tParams.right = 900;
    tParams.bottom = 250;
    tParams.visible = 1;
    tParams.drawWindowFunc = drawCallback;  // SDK: butun callback'ler verilmeli
    tParams.handleMouseClickFunc = clickCallback;
    tParams.handleRightClickFunc = rightClickCallback;
    tParams.handleKeyFunc = keyCallback;
    tParams.handleCursorFunc = cursorCallback;
    tParams.handleMouseWheelFunc = wheelCallback;
    tParams.refcon = this;
    tParams.decorateAsFloatingWindow = xplm_WindowDecorationRoundRectangle;
    tParams.layer = xplm_WindowLayerFloatingWindows;
    mWindow = XPLMCreateWindowEx(&tParams);
    if (!mWindow)
    {
        return false;
    }
    XPLMSetWindowPositioningMode(mWindow, xplm_WindowPositionFree, -1);
    XPLMSetWindowResizingLimits(mWindow, 300, 200, 4000, 4000);
    XPLMSetWindowTitle(mWindow, const_cast<char*>("Texture Host"));

    // 2) Yonetici: ortak veri (nav, havalimanlari, zemin) ve onbellek. Veri klasoru bu eklentinin
    //    klasoru: baska bir host (ornegin resmi Prime XMap eklentisi) ile dosya / onbellek paylasilmaz.
    mManager = prime::XMapManagerFactory::createXMapManager();
    mManager->setLogPrefix("[texture-host]");
    mManager->setDataDirectory(findPluginDirectory());
    if (!mManager->initialize())
    {
        return false;
    }

    // 3) Harita: once nesne ve ayarlar, en son initialize. Texture: pencere acilmaz, dokuya cizilir.
    prime::XMapConfig tConfig;
    tConfig.mName = "main";                        // ayarlar <veri>/maps/main/
    tConfig.mStyle = prime::WindowStyle::Texture;
    tConfig.mMode = prime::XMapMode::Vfr;
    tConfig.mUiVisible = true;                     // haritanin kendi seritleri ve panelleri (dokunun icinde)
    tConfig.mMouseInput = true;                    // fare / tekerlek bizden inject* ile gelir
    mXMap = mManager->addXMap(tConfig);
    if (!mXMap)
    {
        return false;
    }
    updateMapRect();
    mXMap->setTextureSize(mMapWidth, mMapHeight);  // boxel; doku pikseli UI olcegine gore otomatik
    mXMap->initialize();                           // ayar dosyasi varsa burada okunur (LoadAndSave)
    customizeMap();                                // initialize'dan SONRA: her acilista koddaki deger gecerli
    return true;
}

void TextureHost::customizeMap()
{
    mXMap->setFollowAircraft(false);
    mXMap->setCenter(47.45, -122.30);              // KSEA
    mXMap->setZoom(10.0);
    mXMap->setZoomLimits(6.0, 15.0);               // bu harita 6'dan uzaga, 15'ten yakina gitmez

    // Katman gorunurlugu (bu mod icin; her modun durumu ayri saklanir)
    mXMap->setIsLayerVisible(prime::XMapLayer::Roads, false);
    mXMap->setIsLayerVisible(prime::XMapLayer::Fixes, true);

    // Zoom araligi: fix'ler ancak 10.5'ten yakinda; hava yollari 8'den yakinda
    mXMap->setLayerZoomRange(prime::XMapLayer::Fixes, 10.5, 99.0);
    mXMap->setLayerZoomRange(prime::XMapLayer::AirwaysGroup, 8.0, 99.0);

    // Renk: alcak hava yollari turuncu, VOR'lar kirmizi (renk verilebilen katmanlar: API belgesi bolum 9)
    mXMap->setColor(prime::XMapLayer::AirwaysLow, prime::Rgb{255, 140, 0});
    mXMap->setColor(prime::XMapLayer::Vor, prime::Rgb{220, 40, 40});

    // Zemin: daha guclu kabartma, VFR bant sinirlarinda kontur
    prime::TerrainSettings tTerrain = mXMap->getTerrainSettings();
    tTerrain.mShadingStrength = 0.8;
    tTerrain.mVfrContourLines = true;
    mXMap->setTerrainSettings(tTerrain);
}

void TextureHost::disable()
{
    // Siralama: once haritalar (ayarlari yazilir), sonra yonetici, en son pencere
    if (mWindow && XPLMHasKeyboardFocus(mWindow))
    {
        XPLMTakeKeyboardFocus(nullptr);
    }
    if (mManager)
    {
        if (mXMap)
        {
            mManager->removeXMap(mXMap);
            mXMap = nullptr;
        }
        mManager->shutdown();
        prime::XMapManagerFactory::destroyXMapManager(mManager);
        mManager = nullptr;
    }
    if (mWindow)
    {
        XPLMDestroyWindow(mWindow);
        mWindow = nullptr;
    }
}

void TextureHost::updateMapRect()
{
    int tLeft = 0;
    int tTop = 0;
    int tRight = 0;
    int tBottom = 0;
    XPLMGetWindowGeometry(mWindow, &tLeft, &tTop, &tRight, &tBottom);
    mMapLeft = tLeft + kMargin;
    mMapBottom = tBottom + kMargin;
    mMapWidth = (tRight - tLeft) - 2 * kMargin;
    mMapHeight = (tTop - tBottom) - 2 * kMargin;
}

void TextureHost::draw()
{
    if (!mXMap)
    {
        return;
    }
    // Pencere tasindi / boyutlandi: ayni kare icinde haritaya yeni boyut (ayni boyutta is yapilmaz)
    updateMapRect();
    mXMap->setTextureSize(mMapWidth, mMapHeight);
    syncKeyboardFocus(false);

    // Doku gerekiyorsa yeniden cizilir (degisiklik, girdi ya da max_fps); false: basacak doku yok
    if (!mXMap->renderTexture() || mXMap->getTextureId() == 0)
    {
        return;
    }
    XPLMSetGraphicsState(0, 1, 0, 0, 0, 0, 0);  // 1 doku birimi, saydamlik yok (harita opak)
    XPLMBindTexture2d(static_cast<int>(mXMap->getTextureId()), 0);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);  // doku: (0,0) sol-alt, (1,1) sag-ust
    glTexCoord2f(0.0f, 0.0f);
    glVertex2i(mMapLeft, mMapBottom);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2i(mMapLeft + mMapWidth, mMapBottom);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2i(mMapLeft + mMapWidth, mMapBottom + mMapHeight);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2i(mMapLeft, mMapBottom + mMapHeight);
    glEnd();
}

int TextureHost::forwardClick(int pX, int pY, XPLMMouseStatus pStatus, prime::MouseButton pButton, bool* pPressed)
{
    // Basma haritanin icinde baslamissa surukleme ve birakma (disari tassa da) haritaya gider
    const int tX = pX - mMapLeft;  // doku koordinati: orijin sol-alt, birim boxel
    const int tY = pY - mMapBottom;
    if (pStatus == xplm_MouseDown)
    {
        *pPressed = mXMap && tX >= 0 && tY >= 0 && tX < mMapWidth && tY < mMapHeight;
    }
    if (!*pPressed)
    {
        return 1;
    }
    prime::MouseAction tAction = prime::MouseAction::Down;
    if (pStatus == xplm_MouseDrag)
    {
        tAction = prime::MouseAction::Drag;
    }
    else if (pStatus == xplm_MouseUp)
    {
        tAction = prime::MouseAction::Up;
        *pPressed = false;
    }
    mXMap->injectMouse(tX, tY, tAction, pButton);
    syncKeyboardFocus(tAction == prime::MouseAction::Down);  // "Go to airport" kutusuna tiklandiysa yaz
    return 1;
}

void TextureHost::syncKeyboardFocus(bool pTake)
{
    // Klavye odagini sadece harita istediginde (arama kutusu) ve haritaya tiklaninca al; istemeyince birak
    const bool tWants = mXMap && mXMap->wantsKeyboard();
    const bool tHas = XPLMHasKeyboardFocus(mWindow) != 0;
    if (pTake && tWants && !tHas)
    {
        XPLMTakeKeyboardFocus(mWindow);
    }
    else if (!tWants && tHas)
    {
        XPLMTakeKeyboardFocus(nullptr);
    }
}

// --- XPLM callback'leri: refcon = this -------------------------------------------

void TextureHost::drawCallback(XPLMWindowID /*pWindow*/, void* pRefcon)
{
    static_cast<TextureHost*>(pRefcon)->draw();
}

int TextureHost::clickCallback(XPLMWindowID /*pWindow*/, int pX, int pY, XPLMMouseStatus pStatus, void* pRefcon)
{
    TextureHost* tHost = static_cast<TextureHost*>(pRefcon);
    return tHost->forwardClick(pX, pY, pStatus, prime::MouseButton::Left, &tHost->mLeftPressed);
}

int TextureHost::rightClickCallback(XPLMWindowID /*pWindow*/, int pX, int pY, XPLMMouseStatus pStatus,
                                    void* pRefcon)
{
    TextureHost* tHost = static_cast<TextureHost*>(pRefcon);
    return tHost->forwardClick(pX, pY, pStatus, prime::MouseButton::Right, &tHost->mRightPressed);
}

void TextureHost::keyCallback(XPLMWindowID /*pWindow*/, char pKey, XPLMKeyFlags pFlags, char pVirtualKey,
                              void* pRefcon, int pLosingFocus)
{
    TextureHost* tHost = static_cast<TextureHost*>(pRefcon);
    if (pLosingFocus == 0 && tHost->mXMap)
    {
        tHost->mXMap->injectKey(pKey, static_cast<int>(pFlags), pVirtualKey);
        tHost->syncKeyboardFocus(false);  // Esc / Enter kutuyu kapattiysa klavye X-Plane'e doner
    }
}

XPLMCursorStatus TextureHost::cursorCallback(XPLMWindowID /*pWindow*/, int pX, int pY, void* pRefcon)
{
    // Imlec haritanin ustundeyken her karede: tooltip ve vurgu
    TextureHost* tHost = static_cast<TextureHost*>(pRefcon);
    if (tHost->mXMap)
    {
        tHost->mXMap->injectMouseMove(pX - tHost->mMapLeft, pY - tHost->mMapBottom);
    }
    return xplm_CursorDefault;
}

int TextureHost::wheelCallback(XPLMWindowID /*pWindow*/, int pX, int pY, int pWheel, int pClicks, void* pRefcon)
{
    TextureHost* tHost = static_cast<TextureHost*>(pRefcon);
    if (pWheel != 0 || !tHost->mXMap)
    {
        return 0;
    }
    tHost->mXMap->injectWheel(pX - tHost->mMapLeft, pY - tHost->mMapBottom, pClicks);  // imlec altinda zoom
    return 1;
}

std::unique_ptr<TextureHost> gHost;

}  // namespace

PLUGIN_API int XPluginStart(char* pOutName, char* pOutSignature, char* pOutDescription)
{
    std::strcpy(pOutName, "Texture Host");
    std::strcpy(pOutSignature, "example.prime_xmap.texture_host");
    std::strcpy(pOutDescription, "Minimal host embedding a Prime XMap texture");
    return 1;
}

PLUGIN_API int XPluginEnable()
{
    gHost = std::make_unique<TextureHost>();
    if (!gHost->enable())
    {
        gHost->disable();
        gHost.reset();
        return 0;
    }
    return 1;
}

PLUGIN_API void XPluginDisable()
{
    if (gHost)
    {
        gHost->disable();
        gHost.reset();
    }
}

PLUGIN_API void XPluginStop()
{
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID /*pFrom*/, int /*pMessage*/, void* /*pParam*/)
{
}
