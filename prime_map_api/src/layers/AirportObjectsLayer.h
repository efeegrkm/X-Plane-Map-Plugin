// Havalimani nesneleri: park yerleri, helipad, beacon, ruzgar tulumu, PAPI, tabela ve isiklar.
#ifndef PRIME_MAP_SRC_LAYERS_AIRPORTOBJECTSLAYER_H
#define PRIME_MAP_SRC_LAYERS_AIRPORTOBJECTSLAYER_H

#include <string>
#include <vector>

#include "Layer.h"

class AirportGeometryStore;
class AirportDetailStore;

class AirportObjectsLayer : public Layer
{
public:
    AirportObjectsLayer(const AirportGeometryStore& pSummaries, AirportDetailStore& pDetails);

    void draw(const drawContext& pContext) override;
    void drawLabels(const drawContext& pContext) override;
    std::string describe(const drawContext& pContext, double pX, double pY) const override;
    int labelPriority() const override
    {
        return 65;
    }

private:
    struct Marker
    {
        PointF mPoint;
        float mRadius = 6.0f;
        std::string mText;
        std::string mLabel;
    };

    const AirportGeometryStore& mSummaries;
    AirportDetailStore& mDetails;
    std::vector<int> mQuery;
    std::vector<int> mVisible;
    std::vector<Marker> mMarkers;
};
#endif
