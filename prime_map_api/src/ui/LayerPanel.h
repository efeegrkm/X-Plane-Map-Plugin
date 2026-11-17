// "Layers" dugmesinin altinda acilan, gruplu ve uc seviyeli katman listesi.
// Panel sadece cizer ve tiklamayi raporlar; ac/kapa karari XMapWindow'da.
#ifndef PRIME_MAP_SRC_UI_LAYERPANEL_H
#define PRIME_MAP_SRC_UI_LAYERPANEL_H

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
    struct Hit
    {
        bool mInside = false;          // panelin ustunde mi
        std::vector<int> mToggle;      // acilip kapanacak katmanlar (bos: hicbiri)
        bool mGroupOn = false;         // grup kutusu: true = hepsini ac, false = hepsini kapat
        bool mIsGroupBox = false;      // tiklama bir grup kutusuna (ac/kapa karari XMapWindow'da)
        int mSubLayerOwner = -1;       // >= 0: bu katmanin mSubLayer. alt katmani ac/kapa
        int mSubLayer = -1;
        std::string mExpandKey;        // bos degilse bu satiri ac/kapa (toggleExpanded)
        bool mResetDefaults = false;   // baslik satirindaki "Defaults" dugmesi: bu modun varsayilanlari
    };

    bool isOpen() const;
    void setOpen(bool pOpen);
    void toggle();
    void toggleExpanded(const std::string& pKey);

    // pAnchor: "Layers" dugmesi; panel onun altina acilir (pMinBottom: inebilecegi en alt y)
    void layout(const RectI& pAnchor, int pMinBottom, const LayerManager& pLayers);

    void draw(Gfx::Canvas& pCanvas, int pMouseX, int pMouseY, const LayerManager& pLayers) const;

    Hit hitTest(int pX, int pY, const LayerManager& pLayers) const;

    // Tekerlek: pClicks > 0 yukari. Panelin ustundeyse true (harita zoom yapmasin)
    bool scroll(int pX, int pY, int pClicks);

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

    RectI resetButtonRect() const;
    RectI rowRect(int pIndex) const;  // pIndex: mRows indeksi (kaydirma hesaba katilir)
    static RectI boxRect(const RectI& pRow, int pIndent);
    static int visibleCount(const LayerManager& pLayers, const std::vector<int>& pIndices);
    static std::string expandKey(const Row& pRow, const LayerManager& pLayers);
    // 0 = kapali, 1 = acik, 2 = karisik
    static int rowState(const Row& pRow, const LayerManager& pLayers);
    void addLayerRows(int pLayer, int pIndent, const LayerManager& pLayers);

    bool mOpen = false;
    RectI mRect;
    std::vector<Row> mRows;
    std::set<std::string> mExpanded;  // acik gruplar ("group:<ad>") ve katmanlar ("layer:<kimlik>")
    int mFirstRow = 0;                // kaydirma: ilk gosterilen satir
    int mShownRows = 0;               // panele sigan satir sayisi
};
#endif  // PRIME_MAP_SRC_UI_LAYERPANEL_H
