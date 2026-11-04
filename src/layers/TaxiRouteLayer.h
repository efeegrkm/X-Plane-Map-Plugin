// TaxiRouteLayer.h
// Taksi agi (apt.dat 1201 dugum + 1202 kenar): X-Plane ATC'nin taksi
// talimatlarinda kullandigi yollar ve taksi yolu adlari ("A", "B" ...).
// Uzaktan cok yogun gorundugu icin VARSAYILAN KAPALI, zoom 14+ cizilir.
// Fare uzerine gelince kenarin adi ve yonu gorunur.
#pragma once

#include <string>
#include <vector>

#include "Layer.h"

class AirportGeometryStore;
class AirportDetailStore;

class TaxiRouteLayer : public Layer
{
public:
    TaxiRouteLayer(const AirportGeometryStore& pSummaries, AirportDetailStore& pDetails);

    void Draw(const DrawContext& pContext) override;
    void DrawLabels(const DrawContext& pContext) override;
    std::string Describe(const DrawContext& pContext, double pX, double pY) const override;
    int LabelPriority() const override
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
