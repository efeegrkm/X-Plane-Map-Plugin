// FMS ya da host'tan gelen ucus plani rotasi.
#ifndef PRIME_MAP_SRC_LAYERS_FMSLAYER_H
#define PRIME_MAP_SRC_LAYERS_FMSLAYER_H

#include <string>
#include <vector>

#include "Geo.h"
#include "Layer.h"
#include "gfx/Mesh.h"

class FmsLayer : public Layer
{
public:
    FmsLayer();

    void update(float pNow) override;
    void draw(const drawContext& pContext) override;
    void drawLabels(const drawContext& pContext) override;

    std::vector<LatLon> routeAhead() const;

    struct Waypoint
    {
        LatLon mPosition;
        std::string mId;
        int mAltitudeFt;
    };

    void setFromHost(bool pFromHost);
    bool isFromHost() const;
    void setHostRoute(const std::vector<Waypoint>& pWaypoints, int pActive);

private:

    struct PendingLabel
    {
        std::string mText;
        int mX;
        int mY;
    };

    void readFms();

    std::vector<Waypoint> mWaypoints;
    bool mFromHost = false;
    int mDestination = -1;
    float mLastReadTime = -10.0f;
    std::vector<PendingLabel> mLabels;
};
#endif
