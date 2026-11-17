// X-Plane'in FMS'indeki ucus plani (XPLMCountFMSEntries / XPLMGetFMSEntryInfo).
// Her saniye yeniden okunur; FMS degisince harita da degisir.
// Etiketler navaid etiketlerinden oncelikli (yerleri Draw'da ayrilir).
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

    void update(float pNow) override;  // her saniye FMS'i okur (ana thread, flight loop)
    void draw(const drawContext& pContext) override;
    void drawLabels(const drawContext& pContext) override;

    // Rotanin henuz ucurulmamis kismi (aktif bacagin sonundan itibaren); zemin onceden yuklemesi icin
    std::vector<LatLon> routeAhead() const;

    struct Waypoint
    {
        LatLon mPosition;
        std::string mId;
        int mAltitudeFt;
    };

    // API: rota host'tan (true; setHostRoute ile) ya da X-Plane FMS'inden (false, varsayilan)
    void setFromHost(bool pFromHost);
    bool isFromHost() const;
    // pActive: ucagin su an uctugu noktanin indeksi (aktif bacagin sonu); -1 = yok
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
    int mDestination = -1;  // FMS'in su an uctugu nokta (aktif bacagin sonu)
    float mLastReadTime = -10.0f;
    std::vector<PendingLabel> mLabels;
};
#endif  // PRIME_MAP_SRC_LAYERS_FMSLAYER_H
