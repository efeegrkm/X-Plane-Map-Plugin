// Hava yollari: earth_awy.dat (X-Plane 11, 1100 bicimi). X-Plane'e bagimli DEGIL.
// Her satir bir segmenttir: <kimlik1> <bolge1> <tur1> <kimlik2> <bolge2> <tur2>
// <yon> <seviye> <taban FL> <tavan FL> <adlar>; tur: 11 fix, 2 NDB, 3 VOR;
// yon: N iki yonlu, F/B tek yon; seviye: 1 low, 2 high.
// Uc nokta koordinatlari dosyada YOK: NavDatabase'den kimlik + tur ile bulunur,
// ayni kimlikte birden cok aday varsa birbirine en yakin cift secilir.
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
    bool mHigh = false;     // seviye 2: yuksek hava yolu
    char mDirection = 'N';  // 'N' iki yonlu, 'F' mFrom -> mTo, 'B' mTo -> mFrom
    int mBaseFl = 0;
    int mTopFl = 0;
    std::string mNames;
};

// Dosyadaki bir satirin ham hali (uc noktalar henuz cozulmemis)
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
    // baslik, bos ve "99" satirlari icin false
    static bool parseLine(const std::string& pLine, AirwayLine* pOut);

    // Uc noktalari cozer (bekleten islem); pReport: log ozeti.
    bool loadFile(const std::string& pPath, const NavDatabase& pNav, const std::atomic<bool>& pCancel,
                  std::string* pReport);

    // Arka planda okur; cagiran NavDatabase'in degismemesinden sorumludur
    void startLoading(const std::vector<std::string>& pPaths, const NavDatabase& pNav);
    bool poll();  // okuma bu cagrida bittiyse true (bir kez)

    bool isLoading() const;
    bool isReady() const;
    const std::string& lastReport() const;

    int count() const;
    const AirwaySegment& at(int pIndex) const;

    // pBounds'a degebilecek segmentler; uzun segmentler cok hucreye konur, tekrarlar mumkun
    void query(const GeoBounds& pBounds, std::vector<int>& pOut) const;

private:
    void buildGrid();

    std::vector<AirwaySegment> mSegments;
    GeoGrid mGrid;
    BackgroundJob mJob;
    std::string mReport;
    bool mReady = false;
};
#endif  // PRIME_MAP_SRC_DATA_AIRWAYDATABASE_H
