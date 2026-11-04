// LayerPanel.h
// "Layers" dugmesinin altinda acilan, GRUPLU katman listesi.
//
//   +----------------------------+
//   | LAYERS                     |
//   | [x] > Map                  |   grup kapali (ok saga)
//   | [-] v Airports             |   grup acik, alt katmanlarin bazilari acik
//   |       [x] Airport symbols  |
//   |       [ ] Taxi routes      |
//   | [x]   User drawings        |   grupsuz katman
//   +----------------------------+
//
// Grup kutusu: hepsi acik [x], hepsi kapali [ ], karisik [-]. Kutuya
// tiklamak gruptaki hepsini acar (karisik/kapaliysa) ya da kapatir.
// Oka / grup adina tiklamak grubu acar-kapar. Gruplar kKatmanGruplari
// tablosunda (LayerPanel.cpp); orada olmayan katmanlar en altta grupsuz.
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
        bool mIsGroupBox = false;      // tiklama bir grup kutusuna
        std::string mExpandGroup;      // bos degilse bu grubu ac/kapa
    };

    bool IsOpen() const;
    void SetOpen(bool pOpen);
    void Toggle();
    void ToggleGroup(const std::string& pGroup);

    // pAnchor: "Layers" dugmesi; panel onun hemen altina, sol kenari
    // hizali acilir. Her cizimden once cagrilir.
    void Layout(const RectI& pAnchor, const LayerManager& pLayers);

    void Draw(Gfx::Canvas& pCanvas, int pMouseX, int pMouseY, const LayerManager& pLayers) const;

    Hit HitTest(int pX, int pY, const LayerManager& pLayers) const;

private:
    struct Row
    {
        bool mIsGroup = false;
        std::string mGroup;      // grup satiri: grubun adi
        std::vector<int> mLayers;  // grup: gruptaki katmanlar; katman satiri: tek katman
        int mIndent = 0;
    };

    RectI RowRect(int pIndex) const;
    static RectI BoxRect(const RectI& pRow, int pIndent);
    static int VisibleCount(const LayerManager& pLayers, const std::vector<int>& pIndices);

    bool mOpen = false;
    RectI mRect;
    std::vector<Row> mRows;
    std::set<std::string> mExpanded;  // acik gruplar
};
