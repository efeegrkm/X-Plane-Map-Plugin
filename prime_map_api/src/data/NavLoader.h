// X-Plane nav veritabanini SDK uzerinden karelere bolerek NavDatabase'e yukler.
#ifndef PRIME_MAP_SRC_DATA_NAVLOADER_H
#define PRIME_MAP_SRC_DATA_NAVLOADER_H

#include "XPLMNavigation.h"

class NavDatabase;

class NavLoader
{
public:
    explicit NavLoader(NavDatabase& pDatabase);

    bool step(double pBudgetMs);

    bool isDone() const;
    int visitedCount() const;

private:
    void readOne(XPLMNavRef pRef);

    NavDatabase& mDatabase;
    XPLMNavRef mNext = XPLM_NAV_NOT_FOUND;
    bool mStarted = false;
    bool mDone = false;
    int mVisited = 0;
    int mIlsSamplesLogged = 0;
    int mVorSamplesLogged = 0;
    double mElapsedMs = 0.0;
};
#endif
