// LayerPanel.h
// "Layers" dugmesinin altinda acilan, GRUPLU ve UC SEVIYELI katman listesi.
//
//   +-------------------------------+
//   | LAYERS                        |
//   |-------------------------------|
//   | [x] > Map                     |   grup kapali (ok saga)
//   | [-] v Airports                |   grup acik, katmanlarin bazilari acik
//   |     [x]   Airport symbols     |   alt katmani olmayan katman
//   |     [-] v Airport surfaces    |   alt katmanli katman, acik
//   |         [x] Pavement          |
//   |         [ ] Airport boundary  |
//   |     [x] > Runways             |   alt katmanli katman, kapali (varsayilan)
//   | [x] > User drawings           |   grupsuz ama alt katmanli
//   +-------------------------------+
//
// Kutu: hepsi acik [x], hepsi kapali [ ], karisik [-]. Grup kutusu gruptaki
// tum katmanlari, katman kutusu katmani acar/kapatir. Oka veya ada tiklamak
// o satiri acar/kapar. Gruplar ve katmanlar varsayilan olarak KAPALI (katlanmis)
// gelir. Gruplar kGroups tablosunda (LayerPanel.cpp); orada olmayan katmanlar
// en altta grupsuz. Liste pencereye sigmazsa tekerlekle kaydirilir.
//
// Panel sadece cizer ve tiklamanin neye denk geldigini soyler; katmani
// acip kapatmak ve ayari kaydetmek MapWindow'un isi.
#pragma once

#include <set>
#include <string>
#include <vector>

#include "ScreenMath.h"

class LayerManager;

namespace Gfx
{
class Canvas;
}

class LayerPanel
{
public:
    // Bir tiklamanin sonucu
    struct Hit
    {
        bool mInside = false;          // panelin ustunde mi
        std::vector<int> mToggle;      // acilip kapanacak katmanlar (bos: hicbiri)
        bool mGroupOn = false;         // grup kutusu: true = hepsini ac, false = hepsini kapat
        bool mIsGroupBox = false;      // tiklama bir grup kutusuna (ac/kapa karari MapWindow'da)
        int mSubLayerOwner = -1;       // >= 0: bu katmanin mSubLayer. alt katmani ac/kapa
        int mSubLayer = -1;
        std::string mExpandKey;        // bos degilse bu satiri ac/kapa (ToggleExpanded)
    };

    bool IsOpen() const;
    void SetOpen(bool pOpen);
    void Toggle();
    void ToggleExpanded(const std::string& pKey);

    // pAnchor: "Layers" dugmesi; panel onun hemen altina, sol kenari
    // hizali acilir. pMinBottom: panelin inebilecegi en alt y (durum seridinin ustu).
    // Her cizimden once cagrilir.
    void Layout(const RectI& pAnchor, int pMinBottom, const LayerManager& pLayers);

    void Draw(Gfx::Canvas& pCanvas, int pMouseX, int pMouseY, const LayerManager& pLayers) const;

    Hit HitTest(int pX, int pY, const LayerManager& pLayers) const;

    // Tekerlek: pClicks > 0 yukari. Panelin ustundeyse true (harita zoom yapmasin)
    bool Scroll(int pX, int pY, int pClicks);

private:
    enum class RowKind
    {
        Group,
        Layer,
        SubLayer,
    };

    struct Row
    {
        RowKind mKind = RowKind::Layer;
        std::string mGroup;        // grup satiri: grubun adi
        std::vector<int> mLayers;  // grup: gruptaki katmanlar; katman / alt katman satiri: tek katman
        int mSubLayer = -1;        // alt katman satiri: indeksi
        int mIndent = 0;
    };

    RectI RowRect(int pIndex) const;  // pIndex: mRows indeksi (kaydirma hesaba katilir)
    static RectI BoxRect(const RectI& pRow, int pIndent);
    static int VisibleCount(const LayerManager& pLayers, const std::vector<int>& pIndices);
    static std::string ExpandKey(const Row& pRow, const LayerManager& pLayers);
    // 0 = kapali, 1 = acik, 2 = karisik
    static int RowState(const Row& pRow, const LayerManager& pLayers);
    void AddLayerRows(int pLayer, int pIndent, const LayerManager& pLayers);

    bool mOpen = false;
    RectI mRect;
    std::vector<Row> mRows;
    std::set<std::string> mExpanded;  // acik gruplar ("group:<ad>") ve katmanlar ("layer:<kimlik>")
    int mFirstRow = 0;                // kaydirma: ilk gosterilen satir
    int mShownRows = 0;               // panele sigan satir sayisi
};
