// AirportLayer.h
// Havalimanlari (X-Plane nav verisinden; XPLMNavigation xplm_Nav_Airport).
//
// Seyreltme (zoom'a gore):
//   zoom < 4       : hic cizilmez
//   zoom 4 - 8     : sadece 4 harfli ICAO kodlular (buyuk havalimani sayilir)
//   zoom >= 8      : hepsi
//   zoom >= 6      : etiket (ICAO), zoom >= 11: ICAO + ad
// Semboller birbirinin ustune binerse sonra gelen atlanir; buyukler once gelir.
#pragma once

#include <vector>

#include "LabelPlacer.h"
#include "Layer.h"

class NavDatabase;

class AirportLayer : public Layer
{
public:
    explicit AirportLayer(const NavDatabase& pDatabase);

    void Draw(const DrawContext& pContext) override;
    void DrawLabels(const DrawContext& pContext) override;

private:
    struct VisibleAirport
    {
        int mIndex;
        float mX;
        float mY;
        bool mMajor;
    };

    void CollectVisible(const DrawContext& pContext);

    const NavDatabase& mDatabase;
    std::vector<int> mQuery;               // izgara sorgusunun sonucu (her karede yeniden)
    std::vector<VisibleAirport> mVisible;  // bu karede cizilenler (etiketler bunlara yazilir)
    LabelPlacer mSymbolPlacer;             // semboller ust uste binmesin
};
