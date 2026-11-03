// GeoGrid.h
// 1x1 derecelik hucrelere bolunmus cografi izgara (spatial index).
// X-Plane'e bagimli DEGIL. NavDatabase ve pist verisi bunu kullanir.
//
// Neden: on binlerce kayit var; her karede hepsine bakmak yerine sadece
// ekranda gorunen hucrelerdekilere bakiyoruz.
//
// Yapi ("CSR" denilen sikistirilmis bicim):
//   mItems     : kayit numaralari, hucre numarasina gore sirali
//   mCellStart : hucre c'nin kayitlari mItems[mCellStart[c] .. mCellStart[c+1])
// Hucre numarasi = (enlem + 90) * 360 + (boylam + 180), asagi yuvarlanmis.
#pragma once

#include <cstdint>
#include <vector>

#include "MapView.h"

struct GridItem
{
    uint32_t mIndex;  // kaydin, sahibinin listesindeki sirasi
    float mLat;
    float mLon;
};

class GeoGrid
{
public:
    void Clear();

    // Izgarayi bastan kurar (onceki icerik silinir)
    void Build(const std::vector<GridItem>& pItems);

    int Count() const;

    // pBounds'un degdigi hucrelerdeki kayit numaralarini pOut'a EKLER
    // (hucre hassasiyetinde: sinirin biraz disindakiler de gelebilir).
    // Bati/dogu 180. boylami gecebilir (ornek: 179 .. 181).
    void Query(const GeoBounds& pBounds, std::vector<int>& pOut) const;

    static int CellIndex(float pLat, float pLon);

private:
    std::vector<uint32_t> mCellStart;
    std::vector<uint32_t> mItems;
};
