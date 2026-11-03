// AirportLayer.h
// Havalimanlari (X-Plane nav verisinden; XPLMNavigation xplm_Nav_Airport).
//
// Kategoriler ve seyreltme (esikler AirportLayer.cpp'deki kCategoryStyles):
//   buyuk   : kara havalimani, 4 harfli ICAO     zoom 4  (etiket 6,   ad 11)
//   kucuk   : diger kara havalimanlari           zoom 8  (etiket 9,   ad 12)
//   deniz   : "[S]" deniz ussu                    zoom 9  (etiket 10.5, ad 13)
//   heliport: "[H]"                               zoom 11 (etiket 12.5, ad 14)
//   kapali  : "[X]"                               zoom 11 (etiket 13)
// Semboller birbirinin ustune binerse sonra gelen atlanir; once buyukler gelir.
#pragma once

#include <vector>

#include "LabelPlacer.h"
#include "Layer.h"

class NavDatabase;
enum class AirportKind : unsigned char;

class AirportLayer : public Layer
{
public:
    explicit AirportLayer(const NavDatabase& pDatabase);

    void Draw(const DrawContext& pContext) override;
    void DrawLabels(const DrawContext& pContext) override;

    enum class Category
    {
        Major,
        Minor,
        Seaplane,
        Heliport,
        Closed,
    };

private:
    struct VisibleAirport
    {
        int mIndex;
        float mX;
        float mY;
        Category mCategory;
    };

    void CollectVisible(const DrawContext& pContext);
    static Category CategoryOf(AirportKind pAirportKind, bool pMajor);

    const NavDatabase& mDatabase;
    std::vector<int> mQuery;               // izgara sorgusunun sonucu (her karede yeniden)
    std::vector<VisibleAirport> mVisible;  // bu karede cizilenler (etiketler bunlara yazilir)
    LabelPlacer mSymbolPlacer;             // semboller ust uste binmesin
};
