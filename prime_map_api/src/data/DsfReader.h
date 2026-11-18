// X-Plane DSF sahne dosyasi okuyucu: arazi ucgenleri, vektorler ve yukseklik verisi.
#ifndef PRIME_MAP_SRC_DATA_DSFREADER_H
#define PRIME_MAP_SRC_DATA_DSFREADER_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct DsfInfo
{
    double mWest = 0.0;
    double mSouth = 0.0;
    double mEast = 0.0;
    double mNorth = 0.0;
    bool mOverlay = false;
    std::vector<std::string> mTerrainNames;
    std::vector<std::string> mObjectNames;
    std::vector<std::string> mPolygonNames;
    std::vector<std::string> mNetworkNames;
    std::vector<std::pair<std::string, std::string>> mProperties;
};

struct DsfCommandStatistics
{
    struct PolygonCount
    {
        long long mPolygons = 0;
        long long mWindings = 0;
        long long mPoints = 0;
    };
    struct ChainCount
    {
        long long mChains = 0;
        long long mPoints = 0;
    };
    std::vector<long long> mObjects;
    std::vector<PolygonCount> mPolygons;
    std::vector<std::pair<std::pair<int, int>, ChainCount>> mChains;
    long long mTerrainPatches = 0;
};

struct DsfRaster
{
    int mWidth = 0;
    int mHeight = 0;
    bool mPostCentric = false;
    std::vector<float> mValues;
};

struct DsfPolygon
{
    int mDefinition = 0;
    int mParameter = 0;
    std::vector<std::vector<double>> mRingLon;
    std::vector<std::vector<double>> mRingLat;
};

struct DsfChain
{
    int mDefinition = 0;
    int mSubtype = 0;
    std::vector<double> mLon;
    std::vector<double> mLat;
};

struct DsfTriangle
{
    double mLon[3];
    double mLat[3];
    double mElevation[3];
    int mTerrain;
    int mPatchFlags;
    int mPlaneCount = 3;
};

class DsfReader
{
public:
    static const int kPatchFlagPhysical = 1;
    static const int kPatchFlagOverlay = 2;

    bool open(const std::vector<uint8_t>& pData, std::string* pError);

    const DsfInfo& info() const;

    bool forEachTriangle(const std::function<void(const DsfTriangle&)>& pVisitor, std::string* pError) const;

    bool readRaster(const std::string& pName, DsfRaster* pOut) const;
    const std::vector<std::string>& rasterNames() const;

    bool forEachVector(const std::function<void(const DsfPolygon&)>& pPolygonVisitor,
                       const std::function<void(const DsfChain&)>& pChainVisitor, std::string* pError) const;

    int poolCount() const;

    bool countCommands(DsfCommandStatistics* pOut, std::string* pError) const;

private:
    struct Pool
    {
        int mCount = 0;
        int mPlaneCount = 0;
        std::vector<uint16_t> mPlanes[3];
        double mScale[3] = {0.0, 0.0, 0.0};
        double mOffset[3] = {0.0, 0.0, 0.0};
    };

    struct Pool32
    {
        int mCount = 0;
        int mPlaneCount = 0;
        std::vector<uint32_t> mPlanes[2];
        double mScale[2] = {0.0, 0.0};
        double mOffset[2] = {0.0, 0.0};
    };

    bool readPool(const uint8_t* pData, size_t pSize, Pool* pOut) const;
    bool readPool32(const uint8_t* pData, size_t pSize, Pool32* pOut) const;
    bool readScale(const uint8_t* pData, size_t pSize, Pool* pPool) const;
    void readProperties(const uint8_t* pData, size_t pSize);
    static std::vector<std::string> readStringTable(const uint8_t* pData, size_t pSize);
    double planeValue(const Pool& pPool, int pPlane, int pIndex) const;

    const std::vector<uint8_t>* mData = nullptr;
    DsfInfo mInfo;
    std::vector<Pool> mPools;
    std::vector<Pool32> mPools32;
    size_t mCommandsOffset = 0;
    size_t mCommandsSize = 0;

    struct RasterAtoms
    {
        size_t mInfoOffset = 0;
        size_t mInfoSize = 0;
        size_t mDataOffset = 0;
        size_t mDataSize = 0;
    };
    std::vector<std::string> mRasterNames;
    std::vector<RasterAtoms> mRasters;
};
#endif
