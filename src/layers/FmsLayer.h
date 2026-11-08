// FmsLayer.h
// X-Plane'in FMS'indeki ucus plani (XPLMCountFMSEntries / XPLMGetFMSEntryInfo).
// Her saniye yeniden okunur; FMS'te degisiklik olursa harita da degisir.
//
//   gecilmis bacaklar : soluk mor
//   aktif bacak       : parlak, kalin (ucagin su an uctugu, hedef noktaya giden)
//   kalan bacaklar    : mor
//   noktalar          : elmas + kimlik (+ irtifa varsa)
// Etiketler navaid etiketlerinden oncelikli (yerleri Draw'da ayrilir).
#pragma once

#include <string>
#include <vector>

#include "Geo.h"
#include "Layer.h"
#include "gfx/Mesh.h"

class FmsLayer : public Layer
{
public:
    FmsLayer();

    void Update(float pNow) override;  // her saniye FMS'i okur (ana thread, flight loop)
    void Draw(const DrawContext& pContext) override;
    void DrawLabels(const DrawContext& pContext) override;

    // Rotanin henuz ucurulmamis kismi (aktif bacagin sonundan itibaren); zemin onceden yuklemesi icin
    std::vector<LatLon> RouteAhead() const;

    struct Waypoint
    {
        LatLon mPosition;
        std::string mId;
        int mAltitudeFt;
    };

    // API: rota host'tan (true; SetHostRoute ile) ya da X-Plane FMS'inden (false, varsayilan)
    void SetFromHost(bool pFromHost);
    bool IsFromHost() const;
    // pActive: ucagin su an uctugu noktanin indeksi (aktif bacagin sonu); -1 = yok
    void SetHostRoute(const std::vector<Waypoint>& pWaypoints, int pActive);

private:

    struct PendingLabel
    {
        std::string mText;
        int mX;
        int mY;
    };

    void ReadFms();

    std::vector<Waypoint> mWaypoints;
    bool mFromHost = false;
    int mDestination = -1;  // FMS'in su an uctugu nokta (aktif bacagin sonu)
    float mLastReadTime = -10.0f;
    std::vector<PendingLabel> mLabels;
};
