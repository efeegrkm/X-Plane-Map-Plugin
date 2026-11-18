// VOR, NDB, ILS ve fix sembolleri; tur parametreli tek sinif.
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
    NavaidLayer(NavaidType pType, const NavDatabase& pDatabase,
                const AirportGeometryStore* pRunways = nullptr);

    void draw(const drawContext& pContext) override;
    void drawLabels(const drawContext& pContext) override;
    std::string describe(const drawContext& pContext, double pX, double pY) const override;
    int labelPriority() const override;

    struct Style
    {
        const char* mId;
        const char* mName;
        bool mDefaultVisible;
        NavKind mKinds[2];
        int mKindCount;
        double mLabelMinZoom;
        double mDetailMinZoom;
        int mLabelPriority;
        Rgba mColor;
    };

private:
    struct VisibleNavaid
    {
        int mIndex = 0;
        PointF mPoint;
        std::array<PointF, 3> mFeather;
    };

    static const Style& styleFor(NavaidType pType);

    void collectVisible(const drawContext& pContext);
    bool computeFeather(const drawContext& pContext, int pIndex, VisibleNavaid* pOut);
    LatLon featherOrigin(int pIndex);
    void addSymbol(const VisibleNavaid& pNavaid, Gfx::OutlinedLines& pSymbols) const;
    void addCompassRose(const VisibleNavaid& pNavaid, double pZoom, Gfx::Mesh& pRose) const;
    void drawFeathers(Gfx::Canvas& pCanvas, double pZoom) const;
    std::string formatLabel(const NavRecord& pRecord, bool pDetail) const;

    NavaidType mType;
    const Style& mStyle;
    Rgba mColor;
    Rgba mHalo;
    const NavDatabase& mDatabase;
    const AirportGeometryStore* mRunways;

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
#endif
