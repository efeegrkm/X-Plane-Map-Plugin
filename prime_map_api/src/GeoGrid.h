// 1x1 derecelik hucrelere bolunmus mekansal indeks.
#ifndef PRIME_MAP_SRC_GEOGRID_H
#define PRIME_MAP_SRC_GEOGRID_H

#include <cstdint>
#include <vector>

#include "XMapView.h"

struct GridItem
{
    uint32_t mIndex;
    float mLat;
    float mLon;
};

class GeoGrid
{
public:
    void clear();

    void build(const std::vector<GridItem>& pItems);

    int count() const;

    void query(const GeoBounds& pBounds, std::vector<int>& pOut) const;

    static int cellIndex(float pLat, float pLon);

private:
    std::vector<uint32_t> mCellStart;
    std::vector<uint32_t> mItems;
};
#endif
