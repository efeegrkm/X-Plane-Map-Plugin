// Layer.h
// Tum harita katmanlarinin ortak arayuzu. X-Plane'e bagimli DEGIL.
//
// Yeni bir katman eklemek icin:
//   1. Layer'dan tureyen bir sinif yaz (ornek: layers/GridLayer.*)
//   2. Draw() icinde ciz; yazilari DrawLabels() icinde ciz
//   3. Plugin.cpp'de LayerManager'a dogru sirada ekle
// Gorunurluk, ac/kapa dugmesi ve ayar dosyasina kayit kendiliginden olur.
#pragma once

#include <string>

#include "ScreenMath.h"

class LabelPlacer;
class MapView;

// Bir katmanin cizim icin ihtiyac duydugu her sey (her karede yeniden doldurulur)
struct DrawContext
{
    const MapView* mView = nullptr;  // projeksiyon: lat/lon <-> boxel
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
};

class Layer
{
public:
    // pId  : ayar dosyasindaki anahtar (kucuk harf, bosluksuz, ASCII)
    // pName: katman panelinde gorunen ad (ASCII; XPLMDrawString Turkce harf cizemez)
    Layer(const char* pId, const char* pName, bool pDefaultVisible)
        : mId(pId)
        , mName(pName)
        , mVisible(pDefaultVisible)
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

    // Her karede flight loop'tan cagrilir (katman kapali olsa bile):
    // veri yenileme icin (ornek: ucus izi, FMS rotasi)
    virtual void Update(float /*pNow*/)
    {
    }

    // Geometri cizimi (cizgi, dolgu). Sadece gorunurken cagrilir.
    virtual void Draw(const DrawContext& pContext) = 0;

    // Yazilar. Tum katmanlarin geometrisi bittikten sonra cagrilir;
    // boylece yazilar hicbir cizginin altinda kalmaz.
    virtual void DrawLabels(const DrawContext& /*pContext*/)
    {
    }

    // Etiket onceligi: kucuk sayi once yer bulur (cizim sirasindan bagimsiz).
    // Ornek: havalimani adlari hava yolu adlarindan once yerlesir.
    virtual int LabelPriority() const
    {
        return 50;
    }

private:
    std::string mId;
    std::string mName;
    bool mVisible;
};
