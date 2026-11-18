// Harita modlarinin temalari: renkler, zemin bicimi ve varsayilan katmanlar.
#ifndef PRIME_MAP_SRC_XMAPTHEME_H
#define PRIME_MAP_SRC_XMAPTHEME_H

#include <array>
#include <span>
#include <string_view>
#include <vector>

#include "Color.h"
#include "api/XMapTypes.h"
#include "data/TerrainClasses.h"

using prime::XMapMode;

const int kXMapModeCount = 5;

struct LayerDefault
{
    const char* mId;
    bool mVisible;
};

struct XMapTheme
{
    XMapMode mMode;
    const char* mId;
    const char* mName;

    Rgba mBackground;
    Rgba mWater;
    TerrainStyle mTerrainStyle;
    Rgba mHalo;
    bool mLight;

    Rgba mGridLine;
    Rgba mGridLabel;

    std::array<Rgba, 5> mAirport;
    std::array<Rgba, 5> mAirportLabel;
    bool mAirportByTower;
    Rgba mAirportTowered;
    Rgba mAirportUntowered;
    bool mMajorAirportsOnly;

    std::array<Rgba, 4> mNavaid;
    bool mVorRose;

    Rgba mAirwayLow;
    Rgba mAirwayHigh;
    Rgba mAirwayLowLabel;
    Rgba mAirwayHighLabel;
    float mAirwayLowWidth;
    float mAirwayHighWidth;

    Rgba mRunwayEdge;
    Rgba mRunwayLabel;

    std::span<const LayerDefault> mLayerDefaults;

    Rgba mNoData;

    std::array<Rgba, 6> mRoads;
    Rgba mBuiltUp;
};

const XMapTheme& themeFor(XMapMode pMode);

Rgba* themeLayerColor(XMapTheme& pTheme, prime::XMapLayer pLayer);

TerrainImageStyle terrainStyleFor(XMapMode pMode);

std::vector<TerrainImageStyle> allTerrainStyles();

XMapMode modeFromId(std::string_view pId, XMapMode pFallback);

XMapMode nextMode(XMapMode pMode);

std::string_view layerSectionFor(XMapMode pMode);
#endif
