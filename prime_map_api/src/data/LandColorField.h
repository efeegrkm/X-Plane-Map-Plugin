// Arazi siniflari arasinda yumusak renk gecisi icin yumusatilmis renk alani.
#ifndef PRIME_MAP_SRC_DATA_LANDCOLORFIELD_H
#define PRIME_MAP_SRC_DATA_LANDCOLORFIELD_H

#include <vector>

struct TerrainColor;

struct LandBlendPlan
{
    int mPad = 0;
    int mFactor = 1;
    int mHalfX = 0;
    int mHalfY = 0;
};

LandBlendPlan planLandBlend(double pMeters, double pMetersPerPixelX, double pMetersPerPixelY,
                            double pMaxReachPixels);

class LandColorField
{
public:
    void reset(int pPixels, int pFactor);

    void add(int pPixelX, int pPixelY, const TerrainColor& pColor, float pWeight);

    void blur(int pHalfX, int pHalfY);

    bool colorAt(int pPixelX, int pPixelY, double pRgb[3]) const;

private:
    static void boxPass(std::vector<float>& pData, int pCells, int pHalf, bool pHorizontal);

    int mCells = 0;
    int mFactor = 1;
    std::vector<float> mData;
};
#endif
