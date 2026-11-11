// SelfTest.h
// X-Plane icinde Prime Map API'nin butun fonksiyonlarini MapApiCalls uzerinden cagirir,
// her kontrolu Log.txt'ye "PASS" / "FAIL" olarak yazar. Uc asama (flight loop'tan Step):
//
//   1. hemen    : yasam dongusu, pencere, gorunum, mod, katman yardimcilari, cizimler, ayarlar
//   2. veri     : (havalimani + nav hazir olunca) arama, GoToAirport, FitBounds, ucak, rota
//   3. 2 s sonra: (harita cizildikten sonra) sorgu, on onbellek, teshis, kapat / yeniden ac
#pragma once

#include <string>

class MapApiCalls;

class SelfTest
{
public:
    explicit SelfTest(MapApiCalls& pCalls);

    void Start(float pNow);
    void Step(float pNow);  // her karede
    bool IsDone() const;

private:
    void Check(bool pCondition, const std::string& pName, const std::string& pDetail = "");
    void RunImmediate();
    void RunWithData();
    void RunAfterDraw();
    void Finish();

    MapApiCalls& mCalls;
    int mPhase = 0;
    float mPhaseStart = 0.0f;
    int mPassed = 0;
    int mFailed = 0;
};
