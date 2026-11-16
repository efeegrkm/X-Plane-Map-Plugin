// "Go to airport" dugmesinin altinda acilan arama paneli. Ucagin konumuna
// dokunmaz: sadece haritayi secilen havalimanina goturur. Panel sadece cizer,
// klavye tuslari XMapWindow'dan gelir.
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

    bool isOpen() const;
    void setOpen(bool pOpen);  // kapaninca metin ve sonuclar silinir
    void toggle();

    // pAnchor: "Go to airport" dugmesi; panel onun altina acilir
    void layout(const RectI& pAnchor);
    // pFocused: klavye odagi bizde (imlec yanip soner); pNow: saniye (yanip sonme icin)
    void draw(Gfx::Canvas& pCanvas, int pMouseX, int pMouseY, bool pFocused, float pNow) const;
    Hit hitTest(int pX, int pY) const;

    // Klavye (XMapWindow'dan)
    void typeCharacter(char pChar);  // yazdirilabilir ASCII; digerleri yok sayilir
    void backspace();
    void moveSelection(int pDelta);  // +1 asagi, -1 yukari
    int selectedAirport() const;     // secili sonucun indeksi; sonuc yoksa -1

    // Havalimani ozetleri henuz hazir degilken yazilan metin, hazir olunca aranir
    void refresh();

private:
    void search();
    RectI inputRect() const;
    RectI rowRect(int pIndex) const;

    const AirportGeometryStore& mAirports;
    bool mOpen = false;
    std::string mQuery;
    std::vector<int> mResults;
    int mSelected = 0;
    bool mSearchedWhenReady = false;  // son arama ozetler hazirken mi yapildi
    RectI mRect;
};
