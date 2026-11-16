// Bir 1x1 derece karonun DSF vektor ayrintisi: orman/yerlesim poligonlari,
// yollar/demiryolu/enerji hatti, havalimani dislama alanlari. X-Plane'e
// bagimli DEGIL; zemin iscisinde uretilir.
//
// kaynak (GERCEK VERI, dsf_info envanteri):
//   POLY  lib/g8/*.for -> orman/cali/sulak/tarla (TerrainClass);
//         lib/g10/autogen/urban_*, ind_*, com_* .ags -> Urban; park_* -> Grass;
//         natural.ags, .fac, .bch -> kullanilmaz
//   NETW  roads.net: 100-131 otoyol, 10-28 ana yol, 30-48 tali, 50-71 yerel,
//         151-154 demiryolu, 220 enerji hatti
//   Overlay: sim/exclude_for|fac|obj|net = "bati/guney/dogu/kuzey"
//
// Koordinatlar karo birimi (float): x = boylam - bati, y = enlem - guney (0..1).
// Onbellek: <karo>.vectors (noktalar 16 bit ~1,7 m, LZMA), anahtar zeminle ayni.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "data/TerrainClasses.h"

class DsfReader;
struct DsfInfo;

enum class VectorLineKind : uint8_t
{
    Motorway,
    Primary,
    Secondary,
    Local,
    Railway,
    PowerLine,
};

const int kVectorLineKindCount = 6;

struct VectorArea
{
    TerrainClass mClass = TerrainClass::None;
    std::array<float, 4> mBounds = {1.0f, 1.0f, 0.0f, 0.0f};  // bati, guney, dogu, kuzey (karo birimi)
    std::vector<uint32_t> mRingStarts;                        // her halkanin mX/mY'deki ilk indeksi
    std::vector<float> mX;
    std::vector<float> mY;
};

struct VectorLine
{
    VectorLineKind mKind = VectorLineKind::Local;
    std::vector<float> mX;
    std::vector<float> mY;
};

struct VectorExclusion
{
    std::array<float, 4> mBounds = {0.0f, 0.0f, 0.0f, 0.0f};  // bati, guney, dogu, kuzey (karo birimi)
    uint8_t mKinds = 0;                                       // kExclude... bitleri
};

class TerrainVectors
{
public:
    static const uint8_t kExcludeForest = 1;   // sim/exclude_for
    static const uint8_t kExcludeAutogen = 2;  // sim/exclude_obj, sim/exclude_fac (autogen yerlesim)
    static const uint8_t kExcludeNetwork = 4;  // sim/exclude_net

    void reset(int pWest, int pSouth);
    int west() const;
    int south() const;

    // Temel mesh DSF'inin poligon ve zincirlerini ekler (sadelestirerek). Bicim hatasinda false.
    bool addFromDsf(const DsfReader& pReader, std::string* pError);
    // Bir overlay DSF'inin dislama ozelliklerini ekler
    void addExclusions(const DsfInfo& pOverlayInfo);
    // Yollarin dislama alanina dusen parcalarini keser (addFromDsf ve addExclusions'tan sonra)
    void applyNetworkExclusions();

    const std::vector<VectorArea>& areas() const;
    const std::vector<VectorLine>& lines() const;
    // Alan sinifi bu noktada (karo birimi) dislanmis mi (orman: exclude_for, yerlesim: exclude_obj/fac)
    bool isAreaExcluded(TerrainClass pClass, double pX, double pY) const;
    size_t pointCount() const;

    bool save(const std::string& pPath, const std::string& pKey) const;
    bool load(const std::string& pPath, const std::string& pKey);

    // Tanim adindan alan sinifi; kullanilmayan tanim none(orman / autogen kurallari dosya basinda)
    static TerrainClass areaClassFor(const std::string& pDefinitionName);
    // Yol alt turunden cizgi turu; kullanilmayan alt tur -1
    static int lineKindFor(int pSubtype);
    // Orman ailesi (forests alt katmani) mi, yerlesim ailesi (built-up) mi
    static bool isForestClass(TerrainClass pClass);

private:
    int mWest = 0;
    int mSouth = 0;
    std::vector<VectorArea> mAreas;
    std::vector<VectorLine> mLines;
    std::vector<VectorExclusion> mExclusions;
};

// Alani bir piksel izgarasina tarar (cift-tek kurali: delikler bos kalir). piksel(sutun, satir)
// merkezinin karo birimi: x = pX0 + (sutun + 0.5) / pPixelsPerUnitX, y = pYTop - (satir + 0.5) / pPixelsPerUnitY.
// pSet(sutun, satir) icteki her piksel icin cagrilir.
template <typename TSet>
void fillVectorArea(const VectorArea& pArea, double pX0, double pYTop, double pPixelsPerUnitX,
                    double pPixelsPerUnitY, int pWidth, int pHeight, TSet pSet);

#include "data/TerrainVectors.inl"
