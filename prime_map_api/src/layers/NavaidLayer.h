// Radyo seyrusefer yardimcilari ve fix'ler. Tek sinif, tur parametreli
// (VOR, NDB, ILS, Fix). Zoom esikleri ve renkler NavaidLayer.cpp'deki
// kStyles tablosunda.
#ifndef PRIME_MAP_SRC_LAYERS_NAVAIDLAYER_H
#define PRIME_MAP_SRC_LAYERS_NAVAIDLAYER_H

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
class Mesh;
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

    void draw(const drawContext& pContext) override;
    void drawLabels(const drawContext& pContext) override;
    std::string describe(const drawContext& pContext, double pX, double pY) const override;
    int labelPriority() const override;

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
        int mLabelPriority;         // Layer::labelPriority(kucuk = once)
        Rgba mColor;
    };

private:
    struct VisibleNavaid
    {
        int mIndex = 0;
        PointF mPoint;                   // sembolun yeri (ILS: tuyun basladigi esik)
        std::array<PointF, 3> mFeather;  // sadece ILS: sol kanat, orta uc, sag kanat
    };

    static const Style& styleFor(NavaidType pType);

    void collectVisible(const drawContext& pContext);
    bool computeFeather(const drawContext& pContext, int pIndex, VisibleNavaid* pOut);
    LatLon featherOrigin(int pIndex);  // esik (bulunduysa) veya anten
    void addSymbol(const VisibleNavaid& pNavaid, Gfx::OutlinedLines& pSymbols) const;
    void addCompassRose(const VisibleNavaid& pNavaid, double pZoom, Gfx::Mesh& pRose) const;
    void drawFeathers(Gfx::Canvas& pCanvas, double pZoom) const;
    std::string formatLabel(const NavRecord& pRecord, bool pDetail) const;

    NavaidType mType;
    const Style& mStyle;
    Rgba mColor;  // bu karenin rengi (temadan, Draw / drawLabels basinda)
    Rgba mHalo;   // kontur rengi (temadan)
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
#endif  // PRIME_MAP_SRC_LAYERS_NAVAIDLAYER_H
