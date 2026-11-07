// Layer.h
// Tum harita katmanlarinin ortak arayuzu. X-Plane'e bagimli DEGIL.
//
// Yeni bir katman eklemek icin:
//   1. Layer'dan tureyen bir sinif yaz (ornek: layers/GridLayer.*)
//   2. Draw() icinde ciz; yazilari DrawLabels() icinde ciz
//   3. Plugin.cpp'de LayerManager'a dogru sirada ekle
// Gorunurluk, ac/kapa dugmesi ve ayar dosyasina kayit kendiliginden olur.
//
// Alt katmanlar (istege bagli): bir katman yapicisinda AddSubLayer ile
// parcalarini (ornek: "boundary", "pavement") tanitir; Draw icinde
// IsSubLayerVisible(indeks) ile o parcayi cizip cizmeyecegine bakar.
// Panelde katmanin altinda acilir liste olarak cikar, ayar dosyasina
// "<katman>.<alt katman>" anahtariyla kaydedilir.
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "MapTheme.h"
#include "ScreenMath.h"

class LabelPlacer;
class MapView;

namespace Gfx
{
class Canvas;
}

// Bir katmanin cizim icin ihtiyac duydugu her sey (her karede yeniden doldurulur)
struct DrawContext
{
    const MapView* mView = nullptr;  // projeksiyon: lat/lon <-> boxel
    Gfx::Canvas* mCanvas = nullptr;  // cizim kapisi (Mesh -> OpenGL)
    RectD mArea;                     // haritanin cizildigi alan (global boxel)
    RectD mClearArea;                // mArea'nin kontrollerle (ust/alt serit) ortulmeyen kismi
    double mPixelsPerBoxel = 1.0;    // UI olcek (%150 ise 1.5)
    float mNow = 0.0f;               // gecen sure, saniye (XPLMGetElapsedTime)

    // Imlec haritanin ustundeyse (kontrollerin ustunde degil) yeri
    bool mHasCursor = false;
    double mCursorX = 0.0;           // boxel
    double mCursorY = 0.0;

    // Tum katmanlarin ortak etiket yerlestiricisi (DrawLabels icinde kullanilir).
    // Katmanlar sirayla cizildigi icin alttaki katmanin etiketi onceliklidir.
    LabelPlacer* mLabels = nullptr;

    // Harita modunun renkleri (hic bos degil; varsayilan Scenery)
    const MapTheme* mTheme = &ThemeFor(MapMode::Scenery);

    // Yazi golgesi: temanin halesi, tam opak
    Rgba LabelShadow() const
    {
        return WithAlpha(mTheme->mHalo, 1.0f);
    }
};

class Layer
{
public:
    // pId  : ayar dosyasindaki anahtar (kucuk harf, bosluksuz, ASCII)
    // pName: katman panelinde gorunen ad (ASCII; XPLMDrawString Turkce harf cizemez)
    Layer(std::string_view pId, std::string_view pName, bool pDefaultVisible)
        : mId(pId)
        , mName(pName)
        , mVisible(pDefaultVisible)
        , mDefaultVisible(pDefaultVisible)
    {
    }

    virtual ~Layer() = default;

    Layer(const Layer&) = delete;
    Layer& operator=(const Layer&) = delete;

    const std::string& Id() const
    {
        return mId;
    }

    const std::string& Name() const
    {
        return mName;
    }

    bool IsVisible() const
    {
        return mVisible;
    }

    void SetVisible(bool pVisible)
    {
        mVisible = pVisible;
    }

    bool DefaultVisible() const
    {
        return mDefaultVisible;
    }

    // Her karede flight loop'tan cagrilir (katman kapali olsa bile):
    // veri yenileme icin (ornek: ucus izi, FMS rotasi)
    virtual void Update(float /*pNow*/)
    {
    }

    // 0.18: her karede, pencere cizilirken (OpenGL baglami etkin), cizimden ONCE; sadece
    // gorunurken. Hazir gelen veriyi alir (doku yukleme gibi). Harita goruntusunu degistiren
    // yeni bir sey geldiyse true: onbellekteki harita resmi (MapImageCache) yeniden cizilir.
    virtual bool PrepareFrame()
    {
        return false;
    }

    // 0.18: true ise katman onbellekteki harita resmine girmez, her karede onun USTUNE
    // ayrica cizilir (ucak, ucus izi, cizim sirasinda imlece giden kesikli cizgi).
    virtual bool IsOverlay() const
    {
        return false;
    }

    // Geometri cizimi (cizgi, dolgu). Sadece gorunurken cagrilir.
    virtual void Draw(const DrawContext& pContext) = 0;

    // Yazilar. Tum katmanlarin geometrisi bittikten sonra cagrilir;
    // boylece yazilar hicbir cizginin altinda kalmaz.
    virtual void DrawLabels(const DrawContext& /*pContext*/)
    {
    }

    // Fare (pX, pY) boxel noktasinin ustundeyse ne oldugunu anlatan kisa
    // metin ("Runway 16L/34R ..."); yoksa bos. Son cizimdeki konumlara gore.
    virtual std::string Describe(const DrawContext& /*pContext*/, double /*pX*/, double /*pY*/) const
    {
        return "";
    }

    // Etiket onceligi: kucuk sayi once yer bulur (cizim sirasindan bagimsiz).
    // Ornek: havalimani adlari hava yolu adlarindan once yerlesir.
    virtual int LabelPriority() const
    {
        return 50;
    }

    // --- Alt katmanlar ---
    struct SubLayer
    {
        std::string mId;    // ayar anahtari parcasi (kucuk harf, ASCII)
        std::string mName;  // panelde gorunen ad
        bool mVisible = true;
        bool mDefaultVisible = true;
    };

    int SubLayerCount() const
    {
        return static_cast<int>(mSubLayers.size());
    }

    const SubLayer& SubLayerAt(int pIndex) const
    {
        return mSubLayers[static_cast<size_t>(pIndex)];
    }

    // Gecersiz indeks: gorunur say (alt katmani olmayan cagiranlar bozulmasin)
    bool IsSubLayerVisible(int pIndex) const
    {
        if (pIndex < 0 || pIndex >= SubLayerCount())
        {
            return true;
        }
        return mSubLayers[static_cast<size_t>(pIndex)].mVisible;
    }

    void SetSubLayerVisible(int pIndex, bool pVisible)
    {
        if (pIndex >= 0 && pIndex < SubLayerCount())
        {
            mSubLayers[static_cast<size_t>(pIndex)].mVisible = pVisible;
        }
    }

    int VisibleSubLayerCount() const
    {
        int tCount = 0;
        for (const SubLayer& tSub : mSubLayers)
        {
            tCount += tSub.mVisible ? 1 : 0;
        }
        return tCount;
    }

protected:
    // Yapicida sirayla cagrilir; donen indeks IsSubLayerVisible icin
    int AddSubLayer(std::string_view pId, std::string_view pName, bool pDefaultVisible = true)
    {
        SubLayer tSub;
        tSub.mId = pId;
        tSub.mName = pName;
        tSub.mVisible = pDefaultVisible;
        tSub.mDefaultVisible = pDefaultVisible;
        mSubLayers.push_back(tSub);
        return SubLayerCount() - 1;
    }

private:
    std::string mId;
    std::string mName;
    bool mVisible;
    bool mDefaultVisible;
    std::vector<SubLayer> mSubLayers;
};
