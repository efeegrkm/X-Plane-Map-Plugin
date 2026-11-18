// Bir karonun arazi sinifi rasteri, disk onbellegi ve zemin goruntusu uretimi.
#ifndef PRIME_MAP_SRC_DATA_TERRAINRASTER_H
#define PRIME_MAP_SRC_DATA_TERRAINRASTER_H

#include <cstdint>
#include <string>
#include <vector>

#include "data/TerrainClasses.h"

struct ElevationTiles;
class TerrainTriangles;
class TerrainVectors;

class TerrainRaster
{
public:
    static const int kSize = 4096;

    static const int kAreaForest = 1;
    static const int kAreaBuiltUp = 2;

    void reset(int pWest, int pSouth);

    int west() const;
    int south() const;

    void fillTriangle(const double pLon[3], const double pLat[3], TerrainClass pClass);

    TerrainClass classAt(int pX, int pY) const;

    void fillAreas(const TerrainVectors& pVectors);
    int countOf(TerrainClass pClass) const;

    TerrainClass warpedClassAt(int pX, int pY, double pOffsetCellsX, double pOffsetCellsY) const;

    void buildImage(int pX0, int pY0, int pX1, int pY1, int pOutSize, const TerrainPalette& pPalette,
                    const TerrainImageStyle& pStyle, std::vector<uint8_t>* pRgba,
                    const ElevationTiles* pElevation = nullptr, int pAreaMask = 0) const;

    void buildDetailImage(const TerrainTriangles& pTriangles, int pChunksPerSide, int pChunk, int pOutSize,
                          const TerrainPalette& pPalette, const TerrainImageStyle& pStyle,
                          std::vector<uint8_t>* pRgba, const ElevationTiles* pElevation = nullptr,
                          const TerrainVectors* pVectors = nullptr, int pAreaMask = 0) const;

    static void bleedTransparent(int pSize, std::vector<uint8_t>* pRgba);

    bool save(const std::string& pPath, const std::string& pKey) const;
    bool load(const std::string& pPath, const std::string& pKey);

    static bool isAreaAllowed(TerrainClass pArea, int pAreaMask);

private:
    TerrainClass areaClassAt(int pX, int pY, TerrainClass pOwn, int pAreaMask, bool* pBuiltUp) const;

    int mWest = 0;
    int mSouth = 0;
    std::vector<uint8_t> mClasses;
    std::vector<uint8_t> mAreas;
};
#endif
