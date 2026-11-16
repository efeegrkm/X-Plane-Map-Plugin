// XMapManager'in ozel durumu; yalnizca api klasoru .cpp dosyalari gorur.
#ifndef PRIME_MAP_XMAP_MANAGER_PRIVATE_H
#define PRIME_MAP_XMAP_MANAGER_PRIVATE_H

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "PrimeMap.h"
#include "engine/MapData.h"

struct XMapManagerPrivate
{
    std::unique_ptr<::XMapData> mData;  // ortak veri ve arka plan isleri
    std::vector<prime::XMap*> mMaps;    // yasayan harita nesneleri (olusturulma sirasiyla)
    int mNextMapId = 1;

    // initialize() oncesi yapilandirma (setXxx ile gelir)
    std::string mDataDirectory;
    std::string mCacheDirectory;
    std::string mLogPrefix;
    std::function<void(std::string_view)> mLogCallback;
    int mTerrainCacheMb = 0;
    int mPrefetchTiles = 0;
};

#endif  // PRIME_MAP_XMAP_MANAGER_PRIVATE_H
