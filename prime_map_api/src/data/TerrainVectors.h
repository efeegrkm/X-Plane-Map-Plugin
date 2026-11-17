// Bir 1x1 derece karonun DSF vektor ayrintisi: orman/yerlesim poligonlari, yollar,
// havalimani dislama alanlari. Koordinatlar karo birimi: x = boylam - bati, y = enlem - guney.
// Onbellek <karo>.vectors (noktalar 16 bit ~1,7 m, LZMA); anahtar zeminle ayni.
#ifndef PRIME_MAP_SRC_DATA_TERRAINVECTORS_H
#define PRIME_MAP_SRC_DATA_TERRAINVECTORS_H

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
    std::vector<uint32_t> mRingStarts;
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
    std::array<float, 4> mBounds = {0.0f, 0.0f, 0.0f, 0.0f};
    uint8_t mKinds = 0;  // kExclude... bitleri
};

class TerrainVectors
{
public:
    static const uint8_t kExcludeForest = 1;   // sim/exclude_for
    static const uint8_t kExcludeAutogen = 2;  // sim/exclude_obj, exclude_fac
    static const uint8_t kExcludeNetwork = 4;  // sim/exclude_net

    void reset(int pWest, int pSouth);
    int west() const;
    int south() const;

    bool addFromDsf(const DsfReader& pReader, std::string* pError);
    void addExclusions(const DsfInfo& pOverlayInfo);
    void applyNetworkExclusions();  // yollari dislama alanlarina gore keser

    const std::vector<VectorArea>& areas() const;
    const std::vector<VectorLine>& lines() const;
    bool isAreaExcluded(TerrainClass pClass, double pX, double pY) const;
    size_t pointCount() const;

    bool save(const std::string& pPath, const std::string& pKey) const;
    bool load(const std::string& pPath, const std::string& pKey);

    static TerrainClass areaClassFor(const std::string& pDefinitionName);
    static int lineKindFor(int pSubtype);
    static bool isForestClass(TerrainClass pClass);

private:
    int mWest = 0;
    int mSouth = 0;
    std::vector<VectorArea> mAreas;
    std::vector<VectorLine> mLines;
    std::vector<VectorExclusion> mExclusions;
};

// Alani cift-tek kuraliyle piksel izgarasina tarar (delikler bos kalir).
template <typename TSet>
void fillVectorArea(const VectorArea& pArea, double pX0, double pYTop, double pPixelsPerUnitX,
                    double pPixelsPerUnitY, int pWidth, int pHeight, TSet pSet);

#include "data/TerrainVectors.inl"
#endif  // PRIME_MAP_SRC_DATA_TERRAINVECTORS_H
