// ToolPalette.h
// Cizim araclari paleti: haritanin ustunde YUZEN, tasinabilir ve katlanabilir
// kucuk bir panel (ust seritteki daginik Pan/Point/Route/Region/Finish/Undo/
// Clear dugmelerinin yerine).
//
//   Acik:                         Katlanmis (secili aracin uzerine):
//   +------------------------+    +------------------------+
//   | :: DRAW             ^  |    | :: DRAW             v  |
//   |------------------------|    | [ikon] Route           |
//   | [ikon] Pan             |    +------------------------+
//   | [ikon] Point           |
//   |#[ikon] Route##########|   <- secili arac vurgulu
//   | [ikon] Region          |
//   |------------------------|
//   | [Finish] [Undo] [Clear]|
//   +------------------------+
//
// Baslik cubugundan (":: DRAW") tutup surukleyince panel tasinir; yeri
// haritanin SAG UST kosesine gore saklanir (pencere boyutu degisince kenara
// yapisik kalir). Ok (^ / v) paneli katlar/acar; katliyken secili araca
// tiklamak da acar. Ust seritteki "Draw tools" dugmesi paneli gizler/gosterir.
//
// Panel sadece cizer ve tiklamanin neye denk geldigini soyler; ne yapilacagina
// MapWindow karar verir (ToolbarAction ile ayni eylemler).
#pragma once

#include <vector>

#include "Color.h"
#include "ScreenMath.h"
#include "ui/Toolbar.h"

namespace Gfx
{
class Canvas;
}

class ToolPalette
{
public:
    struct Hit
    {
        bool mInside = false;          // panelin ustunde mi
        ToolbarAction mAction = ToolbarAction::None;
        bool mStartDrag = false;       // baslik cubuguna basildi: surukleme basliyor
        bool mToggleCollapse = false;  // ok: katla / ac
    };

    bool IsVisible() const;
    void SetVisible(bool pVisible);
    bool IsCollapsed() const;
    void SetCollapsed(bool pCollapsed);

    // Haritanin sag ust kosesine gore yer (boxel, >= 0); ayar dosyasi icin
    int OffsetRight() const;
    int OffsetTop() const;
    void SetOffset(int pRight, int pTop);

    // pBounds: panelin kalabilecegi alan (ust ve alt seritlerin arasi). Her cizimden once.
    void Layout(const RectI& pBounds, const ToolbarState& pState);

    void Draw(Gfx::Canvas& pCanvas, int pMouseX, int pMouseY, const ToolbarState& pState) const;
    Hit HitTest(int pX, int pY, const ToolbarState& pState) const;
    bool Contains(int pX, int pY) const;

    // Surukleme: fare bu kadar kaydi (Layout bir sonraki karede alana sigdirir)
    void MoveBy(int pDeltaX, int pDeltaY);

private:
    struct Item
    {
        ToolbarAction mAction;
        RectI mRect;
        bool mIsTool;  // true: ikonlu arac satiri, false: islem dugmesi
    };

    static const char* LabelFor(ToolbarAction pAction);
    static bool IsActive(ToolbarAction pAction, const ToolbarState& pState);
    static bool IsEnabled(ToolbarAction pAction, const ToolbarState& pState);
    static ToolbarAction ActiveTool(const ToolbarState& pState);
    static void DrawIcon(Gfx::Canvas& pCanvas, ToolbarAction pAction, const PointF& pCenter, const Rgba& pColor);
    RectI ChevronRect() const;

    bool mVisible = true;
    bool mCollapsed = false;
    int mOffsetRight = 8;
    int mOffsetTop = 8;
    RectI mRect;
    RectI mHeader;
    int mSeparatorY = 0;  // arac satirlari ile islem dugmeleri arasi cizgi (0: yok)
    std::vector<Item> mItems;
};
