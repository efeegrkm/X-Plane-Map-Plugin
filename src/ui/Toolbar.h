// Toolbar.h
// Haritanin ust kenarindaki ince, yari saydam arac seridi.
//
//   [Layers] [Map: VFR] | [Follow aircraft] [Center on aircraft] | [Draw tools]   ...   [Pop out] [Hide]
//
// Cizim araclari (Pan/Point/Route/Region, Finish/Undo/Clear) artik seritte
// degil, yuzen ToolPalette panelinde; "Draw tools" o paneli gosterir/gizler.
// Gruplar arasinda ince ayirici cizgi var. Pencere cok darsa pencere
// dugmeleri ikinci satira iner. Serit sadece cizer ve hangi dugmeye
// tiklandigini soyler; ne yapilacagina MapWindow karar verir.
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
    MapMode,         // harita modu listesini ac/kapat ("Map: VFR")
    FollowAircraft,  // ucagi takip et (ac/kapat)
    CenterAircraft,  // haritayi bir kez ucaga ortala
    DrawTools,       // cizim araclari paletini goster/gizle
    ModePan,         // cizim araclari (ToolPalette): gezin
    ModePoint,       //                  nokta
    ModeRoute,       //                  rota
    ModeRegion,      //                  bolge
    Finish,          // yarim kalan rota/bolgeyi bitir
    Undo,            // son kose / son sekil
    Clear,           // tum cizimleri sil
    PopOut,          // pencereyi X-Plane disina al / geri al
    HideControls,    // tum kontrolleri gizle
};

// Dugmelerin gorunumu icin gereken durum
struct ToolbarState
{
    bool mLayersOpen = false;
    bool mModesOpen = false;
    const char* mModeLabel = "Map: Scenery";  // MapWindow doldurur (statik metin)
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
    void Layout(const RectI& pArea, const ToolbarState& pState);

    void Draw(Gfx::Canvas& pCanvas, int pMouseX, int pMouseY, const ToolbarState& pState) const;

    // Tiklanan dugme (dugme yoksa veya pasifse None)
    ToolbarAction HitTest(int pX, int pY, const ToolbarState& pState) const;

    bool Contains(int pX, int pY) const;
    const RectI& Strip() const;
    int Height() const;  // son Layout'a gore: bir veya iki satir

    // Bir dugmenin yeri (katman panelini "Layers"in altina koymak icin)
    RectI ButtonRect(ToolbarAction pAction) const;

private:
    struct Item
    {
        ToolbarAction mAction;
        const char* mLabel;
        RectI mRect;
    };

    // pActions'i pY satirinda pX'ten saga (pRightToLeft ise sola) dizer; bitis x'ini doner
    int PlaceRow(const std::vector<ToolbarAction>& pActions, int pX, int pTop, bool pRightToLeft,
                 const ToolbarState& pState);
    static int RowWidth(const std::vector<ToolbarAction>& pActions, const ToolbarState& pState);
    static bool IsActive(ToolbarAction pAction, const ToolbarState& pState);
    static bool IsEnabled(ToolbarAction pAction, const ToolbarState& pState);
    static const char* LabelFor(ToolbarAction pAction, const ToolbarState& pState);

    std::vector<Item> mItems;
    std::vector<int> mDividers;  // gruplar arasi ince dikey cizgilerin x'i
    RectI mStrip;
};
