// AirportSearchPanel.h
// "Go to airport" dugmesinin altinda acilan arama paneli (0.18). Ucagin konumuna
// dokunmaz: sadece haritayi (kamerayi) secilen havalimanina goturur.
//
//   +--------------------------------------+
//   | GO TO AIRPORT                        |
//   | [ksea_                             ] |   ICAO / kimlik / ad yazilir
//   |  KSEA  Seattle-Tacoma Intl           |   en iyi 8 eslesme (AirportSearch)
//   |  KSEE  Gillespie Fld                 |   tiklama veya Enter: git
//   +--------------------------------------+   yukari/asagi: sec, Esc: kapat
//
// Panel acilinca klavye odagini MapWindow alir (XPLMTakeKeyboardFocus) ve tuslari
// buraya verir (TypeCharacter / Backspace / MoveSelection). Panel sadece cizer,
// metni ve sonuclari tutar; haritayi tasimak MapWindow'un isi.
#pragma once

#include <string>
#include <vector>

#include "ScreenMath.h"

class AirportGeometryStore;

namespace Gfx
{
class Canvas;
}

class AirportSearchPanel
{
public:
    struct Hit
    {
        bool mInside = false;  // panelin ustunde (tiklama haritaya gecmez)
        bool mInput = false;   // yazi kutusuna tiklandi (klavye odagi alinir)
        int mAirport = -1;     // tiklanan sonucun AirportGeometryStore indeksi
    };

    explicit AirportSearchPanel(const AirportGeometryStore& pAirports);

    bool IsOpen() const;
    void SetOpen(bool pOpen);  // kapaninca metin ve sonuclar silinir
    void Toggle();

    // pAnchor: "Go to airport" dugmesi; panel onun altina acilir
    void Layout(const RectI& pAnchor);
    // pFocused: klavye odagi bizde (imlec yanip soner); pNow: saniye (yanip sonme icin)
    void Draw(Gfx::Canvas& pCanvas, int pMouseX, int pMouseY, bool pFocused, float pNow) const;
    Hit HitTest(int pX, int pY) const;

    // --- Klavye (MapWindow'dan) ---
    void TypeCharacter(char pChar);  // yazdirilabilir ASCII; digerleri yok sayilir
    void Backspace();
    void MoveSelection(int pDelta);  // +1 asagi, -1 yukari
    int SelectedAirport() const;     // secili sonucun indeksi; sonuc yoksa -1

    // Havalimani ozetleri henuz hazir degilken yazilan metin, hazir olunca aranir
    void Refresh();

private:
    void Search();
    RectI InputRect() const;
    RectI RowRect(int pIndex) const;

    const AirportGeometryStore& mAirports;
    bool mOpen = false;
    std::string mQuery;
    std::vector<int> mResults;
    int mSelected = 0;
    bool mSearchedWhenReady = false;  // son arama ozetler hazirken mi yapildi
    RectI mRect;
};
