// Haritanin ust kenarindaki ince, yari saydam arac seridi. Serit sadece cizer
// ve hangi dugmeye tiklandigini soyler; ne yapilacagina XMapWindow karar verir.
// Ornek: [Layers] [XMap: VFR] | [Follow aircraft] [Center on aircraft] | [Draw tools]
#pragma once

#include <vector>

#include "Drawings.h"
#include "ScreenMath.h"

namespace Gfx
{
class Canvas;
}

enum class ToolbarAction
{
    None,
    Layers,          // katman panelini ac/kapat
    MapMode,         // harita modu listesini ac/kapat ("XMap: VFR")
    FollowAircraft,  // ucagi takip et (ac/kapat)
    CenterAircraft,  // haritayi bir kez ucaga ortala
    goToAirport,     // havalimani arama panelini ac/kapat (0.18; sadece harita gider, ucak degil)
    drawTools,       // cizim araclari paletini goster/gizle
    ModePan,         // cizim araclari (ToolPalette): gezin
    ModePoint,       //                  nokta
    ModeRoute,       //                  rota
    ModeRegion,      //                  bolge
    Finish,          // yarim kalan rota/bolgeyi bitir
    Undo,            // son kose / son sekil
    Clear,           // tum cizimleri sil
    popOut,          // pencereyi X-Plane disina al / geri al
    HideControls,    // tum kontrolleri gizle
};

// Dugmelerin gorunumu icin gereken durum
struct ToolbarState
{
    bool mLayersOpen = false;
    bool mModesOpen = false;
    bool mSearchOpen = false;  // "Go to airport" paneli acik
    const char* mModeLabel = "XMap: Scenery";  // XMapWindow doldurur (statik metin)
    bool mFollowing = false;
    bool mHasAircraft = false;
    bool mPoppedOut = false;
    bool mToolsOpen = false;  // cizim araclari paleti gorunur
    Drawings::Mode mMode = Drawings::Mode::Pan;
    bool mCanFinish = false;  // yarim kalan cizim var
    bool mCanUndo = false;    // geri alinacak bir sey var
};

class Toolbar
{
public:
    static const int kRowHeight = 28;  // bir satirin yuksekligi (boxel)

    // Seridi pArea'nin ust kenarina yerlestirir, dugmeleri dizer.
    // Her cizimden once cagrilir (pencere boyutu degisebilir).
    void layout(const RectI& pArea, const ToolbarState& pState);

    void draw(Gfx::Canvas& pCanvas, int pMouseX, int pMouseY, const ToolbarState& pState) const;

    // Tiklanan dugme (dugme yoksa veya pasifse None)
    ToolbarAction hitTest(int pX, int pY, const ToolbarState& pState) const;

    bool contains(int pX, int pY) const;
    const RectI& strip() const;
    int height() const;  // son Layout'a gore: bir veya iki satir

    // Bir dugmenin yeri (katman panelini "Layers"in altina koymak icin)
    RectI buttonRect(ToolbarAction pAction) const;

private:
    struct Item
    {
        ToolbarAction mAction;
        const char* mLabel;
        RectI mRect;
    };

    // pActions'i pY satirinda pX'ten saga (pRightToLeft ise sola) dizer; bitis x'ini doner
    int placeRow(const std::vector<ToolbarAction>& pActions, int pX, int pTop, bool pRightToLeft,
                 const ToolbarState& pState);
    static int rowWidth(const std::vector<ToolbarAction>& pActions, const ToolbarState& pState);
    static bool isActive(ToolbarAction pAction, const ToolbarState& pState);
    static bool isEnabled(ToolbarAction pAction, const ToolbarState& pState);
    static const char* labelFor(ToolbarAction pAction, const ToolbarState& pState);

    std::vector<Item> mItems;
    std::vector<int> mDividers;  // gruplar arasi ince dikey cizgilerin x'i
    RectI mStrip;
};
