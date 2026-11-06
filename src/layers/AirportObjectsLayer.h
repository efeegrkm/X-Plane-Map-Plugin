// AirportObjectsLayer.h
// Havalimanindaki nokta nesneleri, her biri kendi sekli ve rengiyle:
//
//   Nesne (apt.dat)          Sembol                               Zoom
//   kalkis / park (1300, 15) burun yonune donuk ok:               14+  (ad 15.5+)
//                              kapi = dolu mavi, baglama = bos yesil,
//                              hangar = kare + ok, diger = gri ok
//   helipad (102)            cember + yonune donuk "H"            12.5+
//   beacon (18)              isinli yildiz (renk = beacon turu)   12.5+
//   ruzgar tulumu (19)       direk + turuncu/beyaz koni           13.5+
//   PAPI / VASI (21)         kirmizi-beyaz isik sirasi            14.5+
//   tabela (20)              rengine gore kucuk dikdortgen        15.5+
//   isik dizileri (101-108)  turune gore renkli kucuk noktalar,   14.5+
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
