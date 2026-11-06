// DsfReader.h
// X-Plane DSF (sahne) dosyasi okuyucu: ozellikler, arazi turu adlari ve
// arazi ucgenleri. X-Plane'e bagimli DEGIL; arka plan thread'inde calisir.
//
// Bicim: https://developer.x-plane.com/article/dsf-file-format-specification/
//   baslik  : "XPLNEDSF" + uint32 surum (1)
//   atomlar : uint32 kimlik + uint32 boyut (baslik dahil); kucuk-endian, kimlik
//             harfleri dosyada ters ("HEAD" -> "DAEH")
//   son     : 16 bayt MD5
//   HEAD/PROP: ad/deger dizgi ciftleri (sim/west, sim/overlay ...)
//   DEFN/TERT: arazi turu dosya adlari (sira numarasi = arazi indeksi)
//   GEOD/POOL+SCAL: 16 bitlik nokta havuzlari (duzlem duzlem; ham / delta /
//             RLE / RLE+delta kodlu) ve duzlem basina olcek + kaydirma
//   CMDS    : komut akisi; terrain patch + ucgen / serit / yelpaze komutlari
//
// Veri 7z ile sikistirilmissa once SevenZip ile acilmalidir.
#pragma once

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
    bool mOverlay = false;                   // "sim/overlay 1": temel mesh degil, ustune serilen katman
    std::vector<std::string> mTerrainNames;  // TERT
    std::vector<std::string> mObjectNames;   // OBJT: .obj nesneleri
    std::vector<std::string> mPolygonNames;  // POLY: .for orman, .fac cephe, .ags/.agb autogen, .pol serilen
    std::vector<std::string> mNetworkNames;  // NETW: yol / demiryolu ag tanimi (.net)
    std::vector<std::pair<std::string, std::string>> mProperties;
};

// Komut akisinin sayimi (teshis / envanter icin; dsf_info kullanir).
// Anahtar: tanim indeksi (ilgili ad tablosunda).
struct DsfCommandStatistics
{
    struct PolygonCount
    {
        long long mPolygons = 0;  // poligon komutu sayisi
        long long mWindings = 0;  // halka sayisi (dis + delikler)
        long long mPoints = 0;    // toplam kose
    };
    struct ChainCount
    {
        long long mChains = 0;  // ag zinciri komutu sayisi
        long long mPoints = 0;  // zincirlerdeki toplam nokta
    };
    std::vector<long long> mObjects;      // mObjectNames sirasiyla
    std::vector<PolygonCount> mPolygons;  // mPolygonNames sirasiyla
    // Ag: (tanim indeksi, yol alt turu) -> sayim
    std::vector<std::pair<std::pair<int, int>, ChainCount>> mChains;
    long long mTerrainPatches = 0;
};

// DSF icindeki bir raster katmani (DEMS). Satir 0 = GUNEY kenari, sutun 0 = BATI
// (gercek XP11 dosyasinda dogrulandi: belge satir sirasini belirtmiyor).
struct DsfRaster
{
    int mWidth = 0;
    int mHeight = 0;
    bool mPostCentric = false;  // true: kenar pikselleri tam karo sinirinda
    std::vector<float> mValues;  // mWidth * mHeight, ham * olcek + kaydirma (yukseklik: metre)
};

// Bir arazi ucgeni
struct DsfTriangle
{
    double mLon[3];
    double mLat[3];
    double mElevation[3];  // metre; -32768 = deger ayri raster'da (bu okuyucu kullanmaz)
    int mTerrain;          // DsfInfo::mTerrainNames indeksi
    int mPatchFlags;       // 1 = fiziksel (carpisma), 2 = overlay (ustune cizilen)
    int mPlaneCount = 3;   // nokta havuzunun duzlem sayisi (3 = lon/lat/elev; fazlasi alfa/serit duzlemi)
};

class DsfReader
{
public:
    static const int kPatchFlagPhysical = 1;
    static const int kPatchFlagOverlay = 2;

    // pData: acilmis (sikistirilmamis) DSF baytlari; okuma boyunca yasamali.
    // Ozellikleri, arazi adlarini ve nokta havuzlarini okur.
    bool Open(const std::vector<uint8_t>& pData, std::string* pError);

    const DsfInfo& Info() const;

    // Komut akisindaki butun arazi ucgenlerini gezer (overlay'ler dahil;
    // ayiklamak cagiranin isi). Bicim hatasinda false.
    bool ForEachTriangle(const std::function<void(const DsfTriangle&)>& pVisitor, std::string* pError) const;

    // Adi verilen raster katmanini okur (XP11: "elevation", "sea_level", "bathymetry")
    bool ReadRaster(const std::string& pName, DsfRaster* pOut) const;
    // Dosyadaki raster katmanlarinin adlari (DEMN sirasiyla)
    const std::vector<std::string>& RasterNames() const;

    // Teshis icin: nokta havuzu sayisi
    int PoolCount() const;

    // Teshis / envanter: komut akisindaki nesne, poligon ve ag komutlarini
    // tanim basina sayar (koordinat okumaz). Bicim hatasinda false.
    // NOT: atlama mantigi ForEachTriangle ile ayni; Asama 2'de (overlay okuyucu)
    // iki dongu tek bir komut gezgininde birlestirilecek.
    bool CountCommands(DsfCommandStatistics* pOut, std::string* pError) const;

private:
    struct Pool
    {
        int mCount = 0;
        int mPlaneCount = 0;
        std::vector<uint16_t> mPlanes[3];  // sadece lon, lat, yukseklik saklanir
        double mScale[3] = {0.0, 0.0, 0.0};
        double mOffset[3] = {0.0, 0.0, 0.0};
    };

    bool ReadPool(const uint8_t* pData, size_t pSize, Pool* pOut) const;
    bool ReadScale(const uint8_t* pData, size_t pSize, Pool* pPool) const;
    void ReadProperties(const uint8_t* pData, size_t pSize);
    static std::vector<std::string> ReadStringTable(const uint8_t* pData, size_t pSize);
    double PlaneValue(const Pool& pPool, int pPlane, int pIndex) const;

    const std::vector<uint8_t>* mData = nullptr;
    DsfInfo mInfo;
    std::vector<Pool> mPools;
    size_t mCommandsOffset = 0;
    size_t mCommandsSize = 0;

    // Raster katmanlari: DEMN adlari ve ayni siradaki DEMI/DEMD atomlarinin yerleri
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
