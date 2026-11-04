// FileNavLoader.cpp
#include "tools/preview/FileNavLoader.h"

#include <fstream>
#include <locale>
#include <sstream>
#include <unordered_set>
#include <vector>

#include "data/AptDatIndexer.h"
#include "data/NavDatabase.h"

namespace
{

// earth_nav.dat (1150): "<tur> <enlem> <boylam> <rakim> <frekans> <menzil> <yon> <kimlik> <...ad>"
void LoadNav(const std::string& pPath, NavDatabase& pDatabase)
{
    std::ifstream tFile(pPath);
    std::string tLine;
    while (std::getline(tFile, tLine))
    {
        std::istringstream tStream(tLine);
        tStream.imbue(std::locale::classic());
        int tCode = 0;
        double tLat = 0.0;
        double tLon = 0.0;
        double tElevation = 0.0;
        int tFrequency = 0;
        double tRange = 0.0;
        double tHeading = 0.0;
        std::string tId;
        if (!(tStream >> tCode >> tLat >> tLon >> tElevation >> tFrequency >> tRange >> tHeading >> tId))
        {
            continue;
        }
        NavRecord tRecord;
        switch (tCode)
        {
            case 2:
                tRecord.mKind = NavKind::Ndb;
                break;
            case 3:
                tRecord.mKind = NavKind::Vor;
                break;
            case 4:
                tRecord.mKind = NavKind::Ils;
                break;
            case 5:
                tRecord.mKind = NavKind::Localizer;
                break;
            case 12:
            case 13:
                tRecord.mKind = NavKind::Dme;
                break;
            default:
                continue;
        }
        tRecord.mLat = static_cast<float>(tLat);
        tRecord.mLon = static_cast<float>(tLon);
        tRecord.mFrequency = tFrequency;
        tRecord.mId = tId;
        if (tRecord.mKind == NavKind::Ils || tRecord.mKind == NavKind::Localizer)
        {
            DecodeLocalizerCourse(tHeading, &tRecord.mCourseTrue, &tRecord.mCourseMagnetic);
        }
        pDatabase.Add(tRecord);
    }
}

// earth_fix.dat (1101): "<enlem> <boylam> <kimlik> <...>"
void LoadFixes(const std::string& pPath, NavDatabase& pDatabase)
{
    std::ifstream tFile(pPath);
    std::string tLine;
    while (std::getline(tFile, tLine))
    {
        std::istringstream tStream(tLine);
        tStream.imbue(std::locale::classic());
        double tLat = 0.0;
        double tLon = 0.0;
        std::string tId;
        if (!(tStream >> tLat >> tLon >> tId))
        {
            continue;
        }
        NavRecord tRecord;
        tRecord.mKind = NavKind::Fix;
        tRecord.mLat = static_cast<float>(tLat);
        tRecord.mLon = static_cast<float>(tLon);
        tRecord.mId = tId;
        pDatabase.Add(tRecord);
    }
}

// apt.dat: baslik (1/16/17) + 1302 datum_lat / datum_lon; yoksa ilk pist ucu
void LoadAirports(const std::string& pPath, std::unordered_set<std::string>& pSeen, NavDatabase& pDatabase)
{
    std::ifstream tFile(pPath);
    std::string tLine;
    NavRecord tCurrent;
    bool tHasCurrent = false;
    bool tHasPosition = false;
    auto tFlush = [&]()
    {
        if (tHasCurrent && tHasPosition && pSeen.insert(tCurrent.mId).second)  // ayni ICAO: once gelen kazanir
        {
            pDatabase.Add(tCurrent);
        }
        tHasCurrent = false;
        tHasPosition = false;
    };
    while (std::getline(tFile, tLine))
    {
        std::istringstream tStream(tLine);
        tStream.imbue(std::locale::classic());
        int tCode = 0;
        if (!(tStream >> tCode))
        {
            continue;
        }
        if (tCode == 1 || tCode == 16 || tCode == 17)
        {
            tFlush();
            int tElevation = 0;
            int tUnused = 0;
            std::string tId;
            if (!(tStream >> tElevation >> tUnused >> tUnused >> tId))
            {
                continue;
            }
            std::string tName;
            std::getline(tStream, tName);
            const size_t tStart = tName.find_first_not_of(' ');
            tName = (tStart == std::string::npos) ? "" : tName.substr(tStart);
            if (!tName.empty() && tName.back() == '\r')
            {
                tName.pop_back();
            }
            tCurrent = NavRecord();
            tCurrent.mKind = NavKind::Airport;
            tCurrent.mId = tId;
            tCurrent.mAirportKind = AirportKindFromName(tName, &tCurrent.mName);
            tCurrent.mMajor = tCurrent.mAirportKind == AirportKind::Land && IsMajorAirportId(tId);
            tHasCurrent = true;
            continue;
        }
        if (!tHasCurrent)
        {
            continue;
        }
        if (tCode == 1302)
        {
            std::string tKey;
            double tValue = 0.0;
            if (tStream >> tKey >> tValue)
            {
                if (tKey == "datum_lat")
                {
                    tCurrent.mLat = static_cast<float>(tValue);
                }
                else if (tKey == "datum_lon")
                {
                    tCurrent.mLon = static_cast<float>(tValue);
                    tHasPosition = true;
                }
            }
        }
        else if (tCode == 100 && !tHasPosition)
        {
            // Datum yoksa ilk pistin ilk ucu: genislik + 6 alandan sonra uc adi, enlem, boylam
            double tSkip = 0.0;
            std::string tEndName;
            double tLat = 0.0;
            double tLon = 0.0;
            for (int tField = 0; tField < 7; ++tField)
            {
                tStream >> tSkip;
            }
            if (tStream >> tEndName >> tLat >> tLon)
            {
                tCurrent.mLat = static_cast<float>(tLat);
                tCurrent.mLon = static_cast<float>(tLon);
                tHasPosition = true;
            }
        }
    }
    tFlush();
}

}  // namespace

int LoadNavFromFiles(const std::string& pXPlaneDirectory, NavDatabase& pDatabase)
{
    pDatabase.Clear();
    LoadNav(pXPlaneDirectory + "Resources/default data/earth_nav.dat", pDatabase);
    LoadFixes(pXPlaneDirectory + "Resources/default data/earth_fix.dat", pDatabase);
    // Havalimanlari: X-Plane'in sirasiyla (scenery_packs.ini, sonra default apt.dat)
    const std::vector<std::string> tFiles = AptDatIndexer::AptFiles(pXPlaneDirectory);
    std::unordered_set<std::string> tSeen;
    for (const std::string& tFile : tFiles)
    {
        LoadAirports(tFile, tSeen, pDatabase);
    }
    pDatabase.Build();
    return pDatabase.Count();
}
