// earth_awy.dat hava yollarini okur ve uc noktalari nav verisinden cozer.
#ifndef PRIME_MAP_SRC_DATA_AIRWAYDATABASE_H
#define PRIME_MAP_SRC_DATA_AIRWAYDATABASE_H

#include <string>
#include <vector>

#include "BackgroundJob.h"
#include "GeoGrid.h"
#include "Geo.h"

class NavDatabase;

struct AirwaySegment
{
    LatLon mFrom = {0.0, 0.0};
    LatLon mTo = {0.0, 0.0};
    bool mHigh = false;
    char mDirection = 'N';
    int mBaseFl = 0;
    int mTopFl = 0;
    std::string mNames;
};

struct AirwayLine
{
    std::string mFromId;
    int mFromType = 0;
    std::string mToId;
    int mToType = 0;
    char mDirection = 'N';
    int mLevel = 1;
    int mBaseFl = 0;
    int mTopFl = 0;
    std::string mNames;
};

class AirwayDatabase
{
public:
    static bool parseLine(const std::string& pLine, AirwayLine* pOut);

    bool loadFile(const std::string& pPath, const NavDatabase& pNav, const std::atomic<bool>& pCancel,
                  std::string* pReport);

    void startLoading(const std::vector<std::string>& pPaths, const NavDatabase& pNav);
    bool poll();

    bool isLoading() const;
    bool isReady() const;
    const std::string& lastReport() const;

    int count() const;
    const AirwaySegment& at(int pIndex) const;

    void query(const GeoBounds& pBounds, std::vector<int>& pOut) const;

private:
    void buildGrid();

    std::vector<AirwaySegment> mSegments;
    GeoGrid mGrid;
    BackgroundJob mJob;
    std::string mReport;
    bool mReady = false;
};
#endif
