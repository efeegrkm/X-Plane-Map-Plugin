// Gercek olcekli pistler, uzantilari, isaretleri ve numaralari.
#ifndef PRIME_MAP_SRC_LAYERS_RUNWAYLAYER_H
#define PRIME_MAP_SRC_LAYERS_RUNWAYLAYER_H

#include <string>
#include <vector>

#include "Color.h"
#include "Layer.h"

namespace Gfx
{
class Mesh;
}

class AirportGeometryStore;
struct Runway;

class RunwayLayer : public Layer
{
public:
    explicit RunwayLayer(const AirportGeometryStore& pStore);

    void draw(const drawContext& pContext) override;
    void drawLabels(const drawContext& pContext) override;
    std::string describe(const drawContext& pContext, double pX, double pY) const override;
    int labelPriority() const override
    {
        return 15;
    }

private:
    struct ScreenRunway
    {
        const Runway* mRunway;
        float mEndX[2];
        float mEndY[2];
        float mDirX;
        float mDirY;
        float mHalfWidth;
        float mLength;
        float mBoxelsPerMeter;
    };

    void collectVisible(const drawContext& pContext);
    static void addRectangle(Gfx::Mesh& pMesh, const ScreenRunway& pRunway, float pFrom, float pTo,
                             float pHalfWidth, const Rgba& pColor);
    static void addMarkings(Gfx::Mesh& pMesh, const ScreenRunway& pRunway);

    const AirportGeometryStore& mStore;
    std::vector<int> mQuery;
    std::vector<ScreenRunway> mVisible;
};
#endif
