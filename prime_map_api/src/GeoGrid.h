// 1x1 derecelik hucrelere bolunmus cografi izgara (spatial index).
// X-Plane'e bagimli DEGIL. NavDatabase ve pist verisi bunu kullanir.
#pragma once

#include <cstdint>
#include <vector>

#include "XMapView.h"

struct GridItem
{
    uint32_t mIndex;  // kaydin, sahibinin listesindeki sirasi
    float mLat;
    float mLon;
};

class GeoGrid
{
public:
    void clear();

    void build(const std::vector<GridItem>& pItems);

    int count() const;

    // pBounds'un degdigi hucrelerdeki kayit numaralarini pOut'a EKLER
    // (hucre hassasiyetinde: sinirin biraz disindakiler de gelebilir).
    // Bati/dogu 180. boylami gecebilir (ornek: 179 .. 181).
    void query(const GeoBounds& pBounds, std::vector<int>& pOut) const;

    static int cellIndex(float pLat, float pLon);

private:
    std::vector<uint32_t> mCellStart;
    std::vector<uint32_t> mItems;
};
