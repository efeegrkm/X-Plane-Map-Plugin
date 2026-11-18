// Nav verisinin bellekteki kopyasi: havalimani, VOR, DME, NDB, ILS ve fix kayitlari.
#ifndef PRIME_MAP_SRC_DATA_NAVDATABASE_H
#define PRIME_MAP_SRC_DATA_NAVDATABASE_H

#include <string>
#include <vector>

#include "GeoGrid.h"
#include "XMapView.h"

enum class NavKind : unsigned char
{
    Airport,
    Vor,
    Dme,
    Ndb,
    Ils,
    Localizer,
    Fix,
};

const int kNavKindCount = 7;

enum class AirportKind : unsigned char
{
    Land,
    Seaplane,
    Heliport,
    Closed,
};

struct NavRecord
{
    NavKind mKind = NavKind::Fix;
    float mLat = 0.0f;
    float mLon = 0.0f;
    int mFrequency = 0;
    float mCourseTrue = 0.0f;
    short mCourseMagnetic = -1;
    float mVariation = 0.0f;
    bool mHasDme = false;
    bool mHidden = false;
    bool mMajor = false;
    AirportKind mAirportKind = AirportKind::Land;
    std::string mId;
    std::string mName;
};

AirportKind airportKindFromName(const std::string& pName, std::string* pOutCleanName);

void decodeLocalizerCourse(double pRaw, float* pOutTrue, short* pOutMagnetic);

bool isMajorAirportId(const std::string& pId);

bool isPlaceholderId(const std::string& pId);

bool isTacanChannel(int pFrequency);

class NavDatabase
{
public:
    void clear();

    void add(const NavRecord& pRecord);

    void build();

    bool isReady() const;
    int count() const;
    int countOf(NavKind pKind) const;
    const NavRecord& at(int pIndex) const;

    void query(NavKind pKind, const GeoBounds& pBounds, std::vector<int>& pOut) const;

private:
    void pairDmes();
    void buildIndex();

    std::vector<NavRecord> mRecords;
    GeoGrid mGrids[kNavKindCount];
    bool mReady = false;
};
#endif
