// SelfTest.h
// X-Plane icinde Prime XMap API'nin butun fonksiyonlarini XMapApiCalls uzerinden cagirir,
// her kontrolu Log.txt'ye "PASS" / "FAIL" olarak yazar. Uc asama (flight loop'tan Step):
//
//   1. hemen    : yasam dongusu, pencere, gorunum, mod, katman yardimcilari, cizimler, ayarlar
//   2. veri     : (havalimani + nav hazir olunca) arama, goToAirport, fitBounds, ucak, rota
//   3. 2 s sonra: (harita cizildikten sonra) sorgu, on onbellek, teshis, kapat / yeniden ac
#pragma once

#include <string>

class XMapApiCalls;

class SelfTest
{
public:
    explicit SelfTest(XMapApiCalls& pCalls);

    void start(float pNow);
    void step(float pNow);  // her karede
    bool isDone() const;

private:
    void Check(bool pCondition, const std::string& pName, const std::string& pDetail = "");
    void runImmediate();
    void runWithData();
    void runAfterDraw();
    void finish();

    XMapApiCalls& mCalls;
    int mPhase = 0;
    float mPhaseStart = 0.0f;
    int mPassed = 0;
    int mFailed = 0;
};
