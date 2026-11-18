#include "data/NavLoader.h"

#include <array>
#include <chrono>

#include "Log.h"
#include "data/NavDatabase.h"

namespace
{
const int kTimeCheckInterval = 256;
const int kIlsSampleCount = 3;

using Clock = std::chrono::steady_clock;

double millisecondsSince(const Clock::time_point& pStart)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - pStart).count();
}

bool toNavKind(XPLMNavType pType, NavKind* pOut)
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
}

NavLoader::NavLoader(NavDatabase& pDatabase)
    : mDatabase(pDatabase)
{
}

bool NavLoader::step(double pBudgetMs)
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
            return false;
        }
        mStarted = true;
        mDatabase.clear();
        log("reading nav data...");
    }

    int tCount = 0;
    while (mNext != XPLM_NAV_NOT_FOUND)
    {
        readOne(mNext);
        mNext = XPLMGetNextNavAid(mNext);
        ++mVisited;
        if (++tCount % kTimeCheckInterval == 0 && millisecondsSince(tStart) >= pBudgetMs)
        {
            break;
        }
    }

    if (mNext == XPLM_NAV_NOT_FOUND)
    {
        mDatabase.build();
        mDone = true;
        mElapsedMs += millisecondsSince(tStart);
        log("nav data ready: {} records visited, {:.0f} ms (airports {}, VOR {}, DME {}, NDB {}, ILS {}, LOC {}, "
            "fixes {})",
            mVisited, mElapsedMs, mDatabase.countOf(NavKind::Airport), mDatabase.countOf(NavKind::Vor),
            mDatabase.countOf(NavKind::Dme), mDatabase.countOf(NavKind::Ndb), mDatabase.countOf(NavKind::Ils),
            mDatabase.countOf(NavKind::Localizer), mDatabase.countOf(NavKind::Fix));
        return true;
    }
    mElapsedMs += millisecondsSince(tStart);
    return false;
}

bool NavLoader::isDone() const
{
    return mDone;
}

int NavLoader::visitedCount() const
{
    return mVisited;
}

void NavLoader::readOne(XPLMNavRef pRef)
{
    XPLMNavType tType = xplm_Nav_Unknown;
    float tLat = 0.0f;
    float tLon = 0.0f;
    int tFrequency = 0;
    float tHeading = 0.0f;
    std::array<char, 32> tId = {};
    std::array<char, 256> tName = {};
    XPLMGetNavAidInfo(pRef, &tType, &tLat, &tLon, nullptr, &tFrequency, &tHeading, tId.data(), tName.data(),
                      nullptr);
    const std::string tIdText = tId.data();
    const std::string tNameText = tName.data();

    NavRecord tRecord;
    if (!toNavKind(tType, &tRecord.mKind))
    {
        return;
    }
    if (isPlaceholderId(tIdText))
    {
        return;
    }
    tRecord.mLat = tLat;
    tRecord.mLon = tLon;
    tRecord.mFrequency = tFrequency;
    tRecord.mId = tIdText;
    if (tRecord.mKind != NavKind::Fix)
    {
        tRecord.mName = tNameText;
    }
    if (tRecord.mKind == NavKind::Airport)
    {
        tRecord.mAirportKind = airportKindFromName(tNameText, &tRecord.mName);
        tRecord.mMajor = tRecord.mAirportKind == AirportKind::Land && isMajorAirportId(tRecord.mId);
    }
    if (tRecord.mKind == NavKind::Vor)
    {
        tRecord.mVariation = tHeading;
        if (mVorSamplesLogged < kIlsSampleCount)
        {
            ++mVorSamplesLogged;
            log("VOR sample: {} SDK heading (variation) = {:.2f}", tIdText, tHeading);
        }
    }
    if (tRecord.mKind == NavKind::Ils || tRecord.mKind == NavKind::Localizer)
    {
        decodeLocalizerCourse(tHeading, &tRecord.mCourseTrue, &tRecord.mCourseMagnetic);
        if (mIlsSamplesLogged < kIlsSampleCount)
        {
            ++mIlsSamplesLogged;
            log("ILS sample: {} SDK course={:.3f} -> true {:.1f}, magnetic {}", tIdText, tHeading, tRecord.mCourseTrue,
                tRecord.mCourseMagnetic);
        }
    }
    mDatabase.add(tRecord);
}
