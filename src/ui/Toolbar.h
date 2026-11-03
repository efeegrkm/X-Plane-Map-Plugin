// Toolbar.h
// Haritanin ust kenarindaki ince, yari saydam arac seridi.
//
//   [Layers] [Follow aircraft] [Center on aircraft]        [Pop out] [Hide]
//
// Serit sadece cizer ve hangi dugmeye tiklandigini soyler; ne yapilacagina
// MapWindow karar verir. (Cizim modlari - Gezin/Nokta/Rota/Bolge - asama 4'te.)
#pragma once

#include <vector>

#include "ScreenMath.h"

enum class ToolbarAction
{
    None,
    Layers,          // katman panelini ac/kapat
    FollowAircraft,  // ucagi takip et (ac/kapat)
    CenterAircraft,  // haritayi bir kez ucaga ortala
    PopOut,          // pencereyi X-Plane disina al / geri al
    HideControls,    // tum kontrolleri gizle
};

// Dugmelerin gorunumu icin gereken durum
struct ToolbarState
{
    bool mLayersOpen = false;
    bool mFollowing = false;
    bool mHasAircraft = false;
    bool mPoppedOut = false;
};

class Toolbar
{
public:
    static const int kHeight = 28;  // serit yuksekligi (boxel)

    // Seridi pArea'nin ust kenarina yerlestirir, dugmeleri dizer.
    // Her cizimden once cagrilir (pencere boyutu degisebilir).
    void Layout(const RectI& pArea, const ToolbarState& pState);

    void Draw(int pMouseX, int pMouseY, const ToolbarState& pState) const;

    // Tiklanan dugme (dugme yoksa veya pasifse None)
    ToolbarAction HitTest(int pX, int pY, const ToolbarState& pState) const;

    bool Contains(int pX, int pY) const;
    const RectI& Strip() const;

    // Bir dugmenin yeri (katman panelini "Layers"in altina koymak icin)
    RectI ButtonRect(ToolbarAction pAction) const;

private:
    struct Item
    {
        ToolbarAction mAction;
        const char* mLabel;
        RectI mRect;
    };

    static bool IsActive(ToolbarAction pAction, const ToolbarState& pState);
    static bool IsEnabled(ToolbarAction pAction, const ToolbarState& pState);
    static const char* LabelFor(ToolbarAction pAction, const ToolbarState& pState);

    std::vector<Item> mItems;
    RectI mStrip;
};
