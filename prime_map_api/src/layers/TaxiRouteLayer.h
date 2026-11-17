// Taksi agi (apt.dat 1201 dugum + 1202 kenar) ve taksi yolu adlari.
// VARSAYILAN KAPALI, zoom 13.5+ (uzaktan cok yogun). Fare uzerine gelince
// kenarin adi ve yonu gorunur.
#ifndef PRIME_MAP_SRC_LAYERS_TAXIROUTELAYER_H
#define PRIME_MAP_SRC_LAYERS_TAXIROUTELAYER_H

#include <string>
#include <vector>

#include "Layer.h"

class AirportGeometryStore;
class AirportDetailStore;

class TaxiRouteLayer : public Layer
{
public:
    TaxiRouteLayer(const AirportGeometryStore& pSummaries, AirportDetailStore& pDetails);

    void draw(const drawContext& pContext) override;
    void drawLabels(const drawContext& pContext) override;
    std::string describe(const drawContext& pContext, double pX, double pY) const override;
    int labelPriority() const override
    {
        return 66;
    }

private:
    struct ScreenEdge
    {
        PointF mFrom;
        PointF mTo;
        const std::string* mName = nullptr;
        bool mRunway = false;
        bool mOneWay = false;
    };

    const AirportGeometryStore& mSummaries;
    AirportDetailStore& mDetails;
    std::vector<int> mQuery;
    std::vector<int> mVisible;
    std::vector<ScreenEdge> mEdges;
};
#endif  // PRIME_MAP_SRC_LAYERS_TAXIROUTELAYER_H
