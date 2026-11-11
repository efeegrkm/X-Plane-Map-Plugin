// NavLoader.h
// X-Plane'in kendi nav veritabanini SDK ile (XPLMGetFirstNavAid /
// XPLMGetNextNavAid / XPLMGetNavAidInfo) gezip NavDatabase'i doldurur.
// Dosya okumaz: X-Plane'in o an yuklu verisini kullanir, boylece haritadaki
// navaid'ler simulatordekiyle birebir aynidir.
//
// XPLM fonksiyonlari sadece ana thread'den cagrilabilir; bu yuzden is arka
// plana degil, karelere bolunur: her karede (flight loop) en fazla pBudgetMs
// milisaniye calisir. ~250 bin kayit birkac saniyede biter.
#pragma once

#include "XPLMNavigation.h"

class NavDatabase;

class NavLoader
{
public:
    explicit NavLoader(NavDatabase& pDatabase);

    // Bir adim calisir. Yukleme bittiyse (veya zaten bitmisse) true doner.
    bool Step(double pBudgetMs);

    bool IsDone() const;
    int VisitedCount() const;  // simdiye kadar gezilen kayit (ilerleme yazisi icin)

private:
    void ReadOne(XPLMNavRef pRef);

    NavDatabase& mDatabase;
    XPLMNavRef mNext = XPLM_NAV_NOT_FOUND;
    bool mStarted = false;
    bool mDone = false;
    int mVisited = 0;
    int mIlsSamplesLogged = 0;
    int mVorSamplesLogged = 0;
    double mElapsedMs = 0.0;  // toplam harcanan sure (log icin)
};
