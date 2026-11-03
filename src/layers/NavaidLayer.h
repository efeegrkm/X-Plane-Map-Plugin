// NavaidLayer.h
// Radyo seyrusefer yardimcilari ve fix'ler. Tek sinif, tur parametreli:
//
//   tur   semboller                                   gorunur   etiket   frekans
//   Vor   altigen (VOR), altigen+kare (VOR/DME),      zoom 5    6.5      8
//         kare (tek basina DME)
//   Ndb   noktali cember                              zoom 7    8        9
//   Ils   localizer "tuyu": antenden yaklasma          zoom 8.5  10       10
//         yonune 10 NM'lik ucgen
//   Fix   ucgen                                       zoom 9    10       -
//
// Zoom esikleri ve renkler NavaidLayer.cpp'deki tablodadir (kStyles).
#pragma once

#include <vector>

#include "LabelPlacer.h"
#include "Layer.h"
#include "data/NavDatabase.h"
#include "ui/UiDraw.h"

enum class NavaidType
{
    Vor,
    Ndb,
    Ils,
    Fix,
};

class NavaidLayer : public Layer
{
public:
    NavaidLayer(NavaidType pType, const NavDatabase& pDatabase);

    void Draw(const DrawContext& pContext) override;
    void DrawLabels(const DrawContext& pContext) override;

    // Katman turunun tablo bilgisi
    struct Style
    {
        const char* mId;            // ayar dosyasindaki anahtar
        const char* mName;          // katman panelindeki ad
        bool mDefaultVisible;
        NavKind mKinds[2];          // bu katmanin gosterdigi kayit turleri
        int mKindCount;
        double mMinZoom;            // bunun altinda hic cizilmez
        double mLabelMinZoom;       // kimlik etiketi
        double mDetailMinZoom;      // frekans / yon
        Rgba mColor;
    };

private:
    struct VisibleNavaid
    {
        int mIndex;
        float mX;
        float mY;
        float mFeather[6];  // sadece ILS: sol kanat, orta uc, sag kanat (x, y)
    };

    static const Style& StyleFor(NavaidType pType);

    void CollectVisible(const DrawContext& pContext);
    bool ComputeFeather(const DrawContext& pContext, const NavRecord& pRecord, VisibleNavaid* pOut) const;
    void EmitSymbolEdges(const VisibleNavaid& pNavaid) const;  // GL_LINES
    void DrawFeathers() const;
    void FormatLabel(const NavRecord& pRecord, bool pDetail, char* pOut, size_t pSize) const;

    NavaidType mType;
    const Style& mStyle;
    const NavDatabase& mDatabase;
    std::vector<int> mQuery;
    std::vector<VisibleNavaid> mVisible;
    LabelPlacer mSymbolPlacer;
};
