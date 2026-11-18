// Bir karonun DSF vektor ayrintisi: orman ve yerlesim alanlari, yollar, dislama alanlari.
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
    std::array<float, 4> mBounds = {1.0f, 1.0f, 0.0f, 0.0f};
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
    uint8_t mKinds = 0;
};

class TerrainVectors
{
public:
    static const uint8_t kExcludeForest = 1;
    static const uint8_t kExcludeAutogen = 2;
    static const uint8_t kExcludeNetwork = 4;

    void reset(int pWest, int pSouth);
    int west() const;
    int south() const;

    bool addFromDsf(const DsfReader& pReader, std::string* pError);
    void addArea(const VectorArea& pArea);
    void addExclusions(const DsfInfo& pOverlayInfo);
    void applyNetworkExclusions();

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

template <typename TSet>
void fillVectorArea(const VectorArea& pArea, double pX0, double pYTop, double pPixelsPerUnitX,
                    double pPixelsPerUnitY, int pWidth, int pHeight, TSet pSet);

#include "data/TerrainVectors.inl"
#endif
