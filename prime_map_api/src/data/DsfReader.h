// X-Plane DSF (sahne) dosyasi okuyucu. X-Plane'e bagimli DEGIL; arka plan thread'inde
// calisir. Bicim: developer.x-plane.com/article/dsf-file-format-specification/
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
    bool mOverlay = false;  // temel mesh degil, ustune serilen katman
    std::vector<std::string> mTerrainNames;  // TERT
    std::vector<std::string> mObjectNames;   // OBJT
    std::vector<std::string> mPolygonNames;  // POLY
    std::vector<std::string> mNetworkNames;  // NETW
    std::vector<std::pair<std::string, std::string>> mProperties;
};

// Komut akisi sayimi (teshis / envanter); anahtar tanim indeksidir.
struct DsfCommandStatistics
{
    struct PolygonCount
    {
        long long mPolygons = 0;
        long long mWindings = 0;  // dis + delikler
        long long mPoints = 0;
    };
    struct ChainCount
    {
        long long mChains = 0;
        long long mPoints = 0;
    };
    std::vector<long long> mObjects;
    std::vector<PolygonCount> mPolygons;
    std::vector<std::pair<std::pair<int, int>, ChainCount>> mChains;  // (tanim, yol alt turu) -> sayim
    long long mTerrainPatches = 0;
};

// DSF icindeki bir raster katmani (DEMS). Satir 0 = GUNEY, sutun 0 = BATI
// (gercek XP11 dosyasinda dogrulandi; belge satir sirasini belirtmez).
struct DsfRaster
{
    int mWidth = 0;
    int mHeight = 0;
    bool mPostCentric = false;   // kenar pikselleri tam karo sinirinda
    std::vector<float> mValues;  // ham * olcek + kaydirma (yukseklik: metre)
};

// Bir poligon (.for orman, .ags autogen, .fac cephe ...). Halka 0 dis sinir, digerleri delik.
struct DsfPolygon
{
    int mDefinition = 0;  // DsfInfo::mPolygonNames indeksi
    int mParameter = 0;   // orman yogunlugu, cephe yuksekligi ...
    std::vector<std::vector<double>> mRingLon;
    std::vector<std::vector<double>> mRingLat;
};

// Bir ag zinciri (yol, demiryolu, enerji hatti)
struct DsfChain
{
    int mDefinition = 0;  // DsfInfo::mNetworkNames indeksi
    int mSubtype = 0;     // yol alt turu (roads.net: 10 primary, 50 local, 151 demiryolu ...)
    std::vector<double> mLon;
    std::vector<double> mLat;
};

struct DsfTriangle
{
    double mLon[3];
    double mLat[3];
    double mElevation[3];  // metre; -32768 = deger ayri raster'da (bu okuyucu kullanmaz)
    int mTerrain;          // DsfInfo::mTerrainNames indeksi
    int mPatchFlags;       // kPatchFlag... bitleri
    int mPlaneCount = 3;   // nokta havuzunun duzlem sayisi (3 = lon/lat/elev)
};

class DsfReader
{
public:
    static const int kPatchFlagPhysical = 1;
    static const int kPatchFlagOverlay = 2;

    // pData: acilmis (sikistirilmamis) DSF baytlari; okuma boyunca yasamali
    bool open(const std::vector<uint8_t>& pData, std::string* pError);

    const DsfInfo& info() const;

    // Butun arazi ucgenlerini gezer (overlay dahil; ayiklamak cagiranin isidir)
    bool forEachTriangle(const std::function<void(const DsfTriangle&)>& pVisitor, std::string* pError) const;

    // XP11 raster katmanlari: "elevation", "sea_level", "bathymetry"
    bool readRaster(const std::string& pName, DsfRaster* pOut) const;
    const std::vector<std::string>& rasterNames() const;

    // Poligonlar 16 bit (POOL), zincirler 32 bit (PO32) havuzlardan okunur
    bool forEachVector(const std::function<void(const DsfPolygon&)>& pPolygonVisitor,
                       const std::function<void(const DsfChain&)>& pChainVisitor, std::string* pError) const;

    int poolCount() const;

    // Komut akisini koordinat okumadan tanim basina sayar
    bool countCommands(DsfCommandStatistics* pOut, std::string* pError) const;

private:
    struct Pool
    {
        int mCount = 0;
        int mPlaneCount = 0;
        std::vector<uint16_t> mPlanes[3];  // sadece lon, lat, yukseklik saklanir
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

    // DEMN adlari ve ayni siradaki DEMI/DEMD atom yerleri
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
#endif  // PRIME_MAP_SRC_DATA_DSFREADER_H
