// Havalimanlari (X-Plane nav verisi; XPLMNavigation xplm_Nav_Airport).
// Kategori/zoom esikleri AirportLayer.cpp'deki kCategoryStyles'ta.
// Semboller ust uste binerse sonra geleni atlanir; once buyukler gelir.
#ifndef PRIME_MAP_SRC_LAYERS_AIRPORTLAYER_H
#define PRIME_MAP_SRC_LAYERS_AIRPORTLAYER_H

#include <vector>

#include "LabelPlacer.h"
#include "Layer.h"

class AirportGeometryStore;
class NavDatabase;
enum class AirportKind : unsigned char;

class AirportLayer : public Layer
{
public:
    // pGeometry: apt.dat ozeti (kule bilgisi icin, VFR renkleri); bos olabilir
    explicit AirportLayer(const NavDatabase& pDatabase, const AirportGeometryStore* pGeometry = nullptr);

    void draw(const drawContext& pContext) override;
    void drawLabels(const drawContext& pContext) override;
    std::string describe(const drawContext& pContext, double pX, double pY) const override;
    int labelPriority() const override
    {
        return 10;  // en onemli etiketler
    }

    enum class Category
    {
        Major,
        Minor,
        Seaplane,
        Heliport,
        Closed,
    };

private:
    struct VisibleAirport
    {
        int mIndex;
        float mX;
        float mY;
        Category mCategory;
        Rgba mColor;       // temadan: kategori veya kule rengi
        Rgba mLabelColor;
    };

    void collectVisible(const drawContext& pContext);
    static Category categoryOf(AirportKind pAirportKind, bool pMajor);

    const NavDatabase& mDatabase;
    const AirportGeometryStore* mGeometry;
    std::vector<int> mQuery;               // izgara sorgusunun sonucu (her karede yeniden)
    std::vector<VisibleAirport> mVisible;  // bu karede cizilenler (etiketler bunlara yazilir)
    LabelPlacer mSymbolPlacer;             // semboller ust uste binmesin
};
#endif  // PRIME_MAP_SRC_LAYERS_AIRPORTLAYER_H
