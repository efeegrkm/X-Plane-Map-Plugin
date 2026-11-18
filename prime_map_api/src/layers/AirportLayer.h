// Havalimani sembolleri ve etiketleri; kategoriye gore zoom yogunlugu.
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
    explicit AirportLayer(const NavDatabase& pDatabase, const AirportGeometryStore* pGeometry = nullptr);

    void draw(const drawContext& pContext) override;
    void drawLabels(const drawContext& pContext) override;
    std::string describe(const drawContext& pContext, double pX, double pY) const override;
    int labelPriority() const override
    {
        return 10;
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
        Rgba mColor;
        Rgba mLabelColor;
    };

    void collectVisible(const drawContext& pContext);
    static Category categoryOf(AirportKind pAirportKind, bool pMajor);

    const NavDatabase& mDatabase;
    const AirportGeometryStore* mGeometry;
    std::vector<int> mQuery;
    std::vector<VisibleAirport> mVisible;
    LabelPlacer mSymbolPlacer;
};
#endif
