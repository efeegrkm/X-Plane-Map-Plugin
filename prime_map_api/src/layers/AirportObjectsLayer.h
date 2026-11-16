// Havalimanindaki nokta nesneleri (kapi/park oku, helipad, beacon, ruzgar
// tulumu, PAPI/VASI, tabela, isik dizileri), her biri kendi sekil/rengiyle.
// Veri AirportDetailStore'dan; okunmamis havalimani icin istek gonderir.
#pragma once

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
        return 65;  // kapi adlari: navaid'lerden sonra
    }

private:
    // Ekranda cizilen bir nesne: fareyle sorulabilsin diye
    struct Marker
    {
        PointF mPoint;
        float mRadius = 6.0f;
        std::string mText;   // fare aciklamasi
        std::string mLabel;  // haritada yazilan kisa ad (bos olabilir)
    };

    const AirportGeometryStore& mSummaries;
    AirportDetailStore& mDetails;
    std::vector<int> mQuery;
    std::vector<int> mVisible;
    std::vector<Marker> mMarkers;
};
