// AirportObjectsLayer.h
// Havalimanindaki nokta nesneleri, her biri kendi sekli ve rengiyle:
//
//   Nesne (apt.dat)          Sembol                               Zoom
//   kalkis / park (1300, 15) burun yonune donuk ok:               15+  (ad 16.5+)
//                              kapi = dolu mavi, baglama = bos yesil,
//                              hangar = kare + ok, diger = gri ok
//   helipad (102)            cember + yonune donuk "H"            13+
//   beacon (18)              isinli yildiz (renk = beacon turu)   13+
//   ruzgar tulumu (19)       direk + turuncu/beyaz koni           14+
//   PAPI / VASI (21)         kirmizi-beyaz isik sirasi            15+
//   tabela (20)              rengine gore kucuk dikdortgen        16+
//   isik dizileri (101-108)  turune gore renkli kucuk noktalar,   15+
//                            15 m arayla (yesil orta hat, mavi kenar ...)
//
// Fare uzerine gelince Describe ne oldugunu soyler ("Gate A10 (jets)").
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

    void Draw(const DrawContext& pContext) override;
    void DrawLabels(const DrawContext& pContext) override;
    std::string Describe(const DrawContext& pContext, double pX, double pY) const override;
    int LabelPriority() const override
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
