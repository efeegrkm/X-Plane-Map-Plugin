// NavaidLayer.h
// Radyo seyrusefer yardimcilari ve fix'ler. Tek sinif, tur parametreli:
//
//   tur   semboller                                   gorunur   etiket   frekans
//   Vor   altigen (VOR), altigen+kare (VOR/DME),      zoom 5    6.5      8
//         kare (tek basina DME)
//   Ndb   noktali cember                              zoom 7    8        9
//   Ils   localizer "tuyu": pist esiginden yaklasma   zoom 8.5  10       10
//         yonune 10 NM'lik ucgen (pist bulunamazsa antenden)
//   Fix   ucgen                                       zoom 9    10       -
//
// Zoom esikleri ve renkler NavaidLayer.cpp'deki tablodadir (kStyles).
#pragma once

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

#include "Color.h"
#include "LabelPlacer.h"
#include "Layer.h"
#include "data/NavDatabase.h"

class AirportGeometryStore;

namespace Gfx
{
class Canvas;
class OutlinedLines;
}

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
    // pRunways: sadece ILS katmani icin; tuyu pist esiginden baslatmak icin (bos olabilir)
    NavaidLayer(NavaidType pType, const NavDatabase& pDatabase,
                const AirportGeometryStore* pRunways = nullptr);

    void Draw(const DrawContext& pContext) override;
    void DrawLabels(const DrawContext& pContext) override;
    std::string Describe(const DrawContext& pContext, double pX, double pY) const override;
    int LabelPriority() const override;

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
        int mLabelPriority;         // Layer::LabelPriority (kucuk = once)
        Rgba mColor;
    };

private:
    struct VisibleNavaid
    {
        int mIndex = 0;
        PointF mPoint;                   // sembolun yeri (ILS: tuyun basladigi esik)
        std::array<PointF, 3> mFeather;  // sadece ILS: sol kanat, orta uc, sag kanat
    };

    static const Style& StyleFor(NavaidType pType);

    void CollectVisible(const DrawContext& pContext);
    bool ComputeFeather(const DrawContext& pContext, int pIndex, VisibleNavaid* pOut);
    LatLon FeatherOrigin(int pIndex);  // esik (bulunduysa) veya anten
    void AddSymbol(const VisibleNavaid& pNavaid, Gfx::OutlinedLines& pSymbols) const;
    void DrawFeathers(Gfx::Canvas& pCanvas, double pZoom) const;
    std::string FormatLabel(const NavRecord& pRecord, bool pDetail) const;

    NavaidType mType;
    const Style& mStyle;
    const NavDatabase& mDatabase;
    const AirportGeometryStore* mRunways;

    // ILS kayit indeksi -> tuyun basladigi nokta. Pist verisi hazir olduktan
    // sonra her ILS icin bir kez hesaplanir.
    struct FeatherOriginCache
    {
        bool mOnThreshold;
        LatLon mOrigin;
    };
    std::unordered_map<int, FeatherOriginCache> mOrigins;
    std::vector<int> mQuery;
    std::vector<VisibleNavaid> mVisible;
    LabelPlacer mSymbolPlacer;
};
