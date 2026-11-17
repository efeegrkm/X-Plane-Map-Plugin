// Tum harita katmanlarinin ortak arayuzu. X-Plane'e bagimli DEGIL.
// Yeni katman: Layer'dan ture, draw()/drawLabels() icine ciz, LayerManager'a sirayla ekle.
#ifndef PRIME_MAP_SRC_LAYER_H
#define PRIME_MAP_SRC_LAYER_H

#include <string>
#include <string_view>
#include <vector>

#include "XMapTheme.h"
#include "ScreenMath.h"

class LabelPlacer;
class XMapView;

namespace Gfx
{
class Canvas;
}

// Bir katmanin cizim icin ihtiyac duydugu her sey (her karede yeniden doldurulur)
struct drawContext
{
    const XMapView* mView = nullptr;  // lat/lon <-> boxel
    Gfx::Canvas* mCanvas = nullptr;
    RectD mArea;                     // haritanin cizildigi alan (global boxel)
    RectD mClearArea;                // mArea'nin seritlerle ortulmeyen kismi
    double mPixelsPerBoxel = 1.0;    // UI olcek (%150 ise 1.5)
    float mNow = 0.0f;               // saniye (XPLMGetElapsedTime)

    // Imlec haritanin ustundeyse (kontrollerin ustunde degil) yeri (boxel)
    bool mHasCursor = false;
    double mCursorX = 0.0;
    double mCursorY = 0.0;

    // Tum katmanlarin ortak etiket yerlestiricisi (drawLabels icinde kullanilir)
    LabelPlacer* mLabels = nullptr;

    const XMapTheme* mTheme = &themeFor(XMapMode::Scenery);

    // Yazi golgesi: temanin halesi, tam opak
    Rgba labelShadow() const
    {
        return withAlpha(mTheme->mHalo, 1.0f);
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

    const std::string& id() const
    {
        return mId;
    }

    const std::string& name() const
    {
        return mName;
    }

    bool isVisible() const
    {
        return mVisible;
    }

    void setVisible(bool pVisible)
    {
        mVisible = pVisible;
    }

    bool defaultVisible() const
    {
        return mDefaultVisible;
    }

    // flight loop'tan her karede (katman kapaliyken bile): veri yenileme
    virtual void update(float /*pNow*/)
    {
    }

    // openGL baglami etkin, cizimden ONCE, sadece gorunurken. Harita goruntusunu
    // degistiren yeni veri geldiyse true: onbellek resmi (XMapImageCache) yenilenir.
    virtual bool prepareFrame()
    {
        return false;
    }

    // true: onbellek resme girmez, her karede ustune cizilir (ucak, ucus izi ...)
    virtual bool isOverlay() const
    {
        return false;
    }

    // Sadece gorunurken cagrilir
    virtual void draw(const drawContext& pContext) = 0;

    // Tum katmanlarin geometrisi bittikten sonra cagrilir; yazilar cizgilerin altinda kalmaz
    virtual void drawLabels(const drawContext& /*pContext*/)
    {
    }

    // Harita resminin USTUNE cizilen kisim: kenara yapisan seyler kaydirinca kaymasin
    virtual void drawOverlay(const drawContext& /*pContext*/)
    {
    }

    // imlec altindaki seyin kisa aciklamasi; yoksa bos
    virtual std::string describe(const drawContext& /*pContext*/, double /*pX*/, double /*pY*/) const
    {
        return "";
    }

    // kucuk sayi once yer bulur (orn. havalimani, hava yolundan once)
    virtual int labelPriority() const
    {
        return 50;
    }

    struct SubLayer
    {
        std::string mId;    // ayar anahtari parcasi (kucuk harf, ASCII)
        std::string mName;
        bool mVisible = true;
        bool mDefaultVisible = true;
        double mMinZoom = kNoMinZoom;
        double mMaxZoom = kNoMaxZoom;
    };

    int subLayerCount() const
    {
        return static_cast<int>(mSubLayers.size());
    }

    const SubLayer& subLayerAt(int pIndex) const
    {
        return mSubLayers[static_cast<size_t>(pIndex)];
    }

    // Gecersiz indeks: gorunur say (alt katmani olmayan cagiranlar bozulmasin)
    bool isSubLayerVisible(int pIndex) const    {
        if (pIndex < 0 || pIndex >= subLayerCount())
        {
            return true;
        }
        return mSubLayers[static_cast<size_t>(pIndex)].mVisible;
    }

    void setSubLayerVisible(int pIndex, bool pVisible)
    {
        if (pIndex >= 0 && pIndex < subLayerCount())
        {
            mSubLayers[static_cast<size_t>(pIndex)].mVisible = pVisible;
        }
    }

    int visibleSubLayerCount() const
    {
        int tCount = 0;
        for (const SubLayer& tSub : mSubLayers)
        {
            tCount += tSub.mVisible ? 1 : 0;
        }
        return tCount;
    }

    static constexpr double kNoMinZoom = 0.0;
    static constexpr double kNoMaxZoom = 99.0;

    void setZoomRange(double pMinZoom, double pMaxZoom)
    {
        mMinZoom = pMinZoom;
        mMaxZoom = pMaxZoom;
    }

    double minZoom() const
    {
        return mMinZoom;
    }

    double maxZoom() const
    {
        return mMaxZoom;
    }

    void setSubLayerZoomRange(int pIndex, double pMinZoom, double pMaxZoom)
    {
        if (pIndex >= 0 && pIndex < subLayerCount())
        {
            mSubLayers[static_cast<size_t>(pIndex)].mMinZoom = pMinZoom;
            mSubLayers[static_cast<size_t>(pIndex)].mMaxZoom = pMaxZoom;
        }
    }

    // LayerManager her cizimden once haritanin o anki zoom'unu verir
    void setCurrentZoom(double pZoom)
    {
        mCurrentZoom = pZoom;
    }

    // Katman su anki zoom'da ciziliyor mu (kendi kutusundan bagimsiz)
    bool isInZoomRange() const
    {
        return mCurrentZoom >= mMinZoom && mCurrentZoom <= mMaxZoom;
    }

    // Alt katmanin kutusu acik VE su anki zoom araliginda; gecersiz indeks: cizilir say
    bool isSubLayerDrawn(int pIndex) const
    {
        if (pIndex < 0 || pIndex >= subLayerCount())
        {
            return true;
        }
        const SubLayer& tSub = mSubLayers[static_cast<size_t>(pIndex)];
        return tSub.mVisible && mCurrentZoom >= tSub.mMinZoom && mCurrentZoom <= tSub.mMaxZoom;
    }

protected:
    // Yapicida sirayla cagrilir; donen indeks isSubLayerVisible icin
    int addSubLayer(std::string_view pId, std::string_view pName, bool pDefaultVisible = true)
    {
        SubLayer tSub;
        tSub.mId = pId;
        tSub.mName = pName;
        tSub.mVisible = pDefaultVisible;
        tSub.mDefaultVisible = pDefaultVisible;
        mSubLayers.push_back(tSub);
        return subLayerCount() - 1;
    }

private:
    std::string mId;
    std::string mName;
    bool mVisible;
    bool mDefaultVisible;
    double mMinZoom = kNoMinZoom;
    double mMaxZoom = kNoMaxZoom;
    double mCurrentZoom = 10.0;  // setCurrentZoom gelene kadar ortalama bir deger
    std::vector<SubLayer> mSubLayers;
};
#endif  // PRIME_MAP_SRC_LAYER_H
