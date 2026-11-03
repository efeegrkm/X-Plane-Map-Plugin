// NavLoader.cpp
#include "data/NavLoader.h"

#include <chrono>

#include "Log.h"
#include "data/NavDatabase.h"

namespace
{

const int kTimeCheckInterval = 256;  // saati her kayitta degil, bu kadar kayitta bir oku
const int kIlsSampleCount = 3;       // ILS yon kodlamasini dogrulamak icin log'a yazilacak ornek

using Clock = std::chrono::steady_clock;

double MillisecondsSince(const Clock::time_point& pStart)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - pStart).count();
}

// XPLM turunu bizim turumuze cevirir. Ilgilenmedigimiz turler
// (suzulus yolu, marker, enlem/boylam noktasi) icin false.
bool ToNavKind(XPLMNavType pType, NavKind* pOut)
{
    switch (pType)
    {
        case xplm_Nav_Airport:
            *pOut = NavKind::Airport;
            return true;
        case xplm_Nav_VOR:
            *pOut = NavKind::Vor;
            return true;
        case xplm_Nav_DME:
            *pOut = NavKind::Dme;
            return true;
        case xplm_Nav_NDB:
            *pOut = NavKind::Ndb;
            return true;
        case xplm_Nav_ILS:
            *pOut = NavKind::Ils;
            return true;
        case xplm_Nav_Localizer:
            *pOut = NavKind::Localizer;
            return true;
        case xplm_Nav_Fix:
            *pOut = NavKind::Fix;
            return true;
        default:
            return false;
    }
}

}  // namespace

NavLoader::NavLoader(NavDatabase& pDatabase)
    : mDatabase(pDatabase)
{
}

bool NavLoader::Step(double pBudgetMs)
{
    if (mDone)
    {
        return true;
    }
    const Clock::time_point tStart = Clock::now();

    if (!mStarted)
    {
        mNext = XPLMGetFirstNavAid();
        if (mNext == XPLM_NAV_NOT_FOUND)
        {
            return false;  // nav verisi henuz hazir degil: sonraki karede tekrar dene
        }
        mStarted = true;
        mDatabase.Clear();
        Log("reading nav data...");
    }

    int tCount = 0;
    while (mNext != XPLM_NAV_NOT_FOUND)
    {
        ReadOne(mNext);
        mNext = XPLMGetNextNavAid(mNext);
        ++mVisited;
        if (++tCount % kTimeCheckInterval == 0 && MillisecondsSince(tStart) >= pBudgetMs)
        {
            break;
        }
    }

    if (mNext == XPLM_NAV_NOT_FOUND)
    {
        mDatabase.Build();
        mDone = true;
        mElapsedMs += MillisecondsSince(tStart);
        Log("nav data ready: %d records visited, %.0f ms (airports %d, VOR %d, DME %d, NDB %d, ILS %d, LOC %d, "
            "fixes %d)",
            mVisited, mElapsedMs, mDatabase.CountOf(NavKind::Airport), mDatabase.CountOf(NavKind::Vor),
            mDatabase.CountOf(NavKind::Dme), mDatabase.CountOf(NavKind::Ndb), mDatabase.CountOf(NavKind::Ils),
            mDatabase.CountOf(NavKind::Localizer), mDatabase.CountOf(NavKind::Fix));
        return true;
    }
    mElapsedMs += MillisecondsSince(tStart);
    return false;
}

bool NavLoader::IsDone() const
{
    return mDone;
}

int NavLoader::VisitedCount() const
{
    return mVisited;
}

void NavLoader::ReadOne(XPLMNavRef pRef)
{
    XPLMNavType tType = xplm_Nav_Unknown;
    float tLat = 0.0f;
    float tLon = 0.0f;
    int tFrequency = 0;
    float tHeading = 0.0f;
    char tId[32] = "";
    char tName[256] = "";
    XPLMGetNavAidInfo(pRef, &tType, &tLat, &tLon, nullptr, &tFrequency, &tHeading, tId, tName, nullptr);

    NavRecord tRecord;
    if (!ToNavKind(tType, &tRecord.mKind))
    {
        return;
    }
    if (IsPlaceholderId(tId))
    {
        return;  // "----": X-Plane'in yer tutucu kaydi (konum/yon bilgisi yok)
    }
    tRecord.mLat = tLat;
    tRecord.mLon = tLon;
    tRecord.mFrequency = tFrequency;
    tRecord.mId = tId;
    if (tRecord.mKind != NavKind::Fix)
    {
        tRecord.mName = tName;  // fix'in adi kimligiyle ayni: saklama
    }
    if (tRecord.mKind == NavKind::Airport)
    {
        tRecord.mAirportKind = AirportKindFromName(tName, &tRecord.mName);
        tRecord.mMajor = tRecord.mAirportKind == AirportKind::Land && IsMajorAirportId(tRecord.mId);
    }
    if (tRecord.mKind == NavKind::Ils || tRecord.mKind == NavKind::Localizer)
    {
        // X-Plane 11.55'te denendi: SDK yonu zaten cozulmus GERCEK yon olarak
        // veriyor (IANC: 89.9). Dosyadaki "manyetik*360 + gercek" kodlu deger
        // gelirse de dogru cozulsun diye Decode yine de cagriliyor.
        DecodeLocalizerCourse(tHeading, &tRecord.mCourseTrue, &tRecord.mCourseMagnetic);
        if (mIlsSamplesLogged < kIlsSampleCount)
        {
            ++mIlsSamplesLogged;
            Log("ILS sample: %s SDK course=%.3f -> true %.1f, magnetic %d", tId, tHeading, tRecord.mCourseTrue,
                tRecord.mCourseMagnetic);
        }
    }
    mDatabase.Add(tRecord);
}
