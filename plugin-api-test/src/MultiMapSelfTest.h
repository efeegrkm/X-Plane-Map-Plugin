// MultiMapSelfTest.h
// Prime Map API 0.23'un coklu harita / fabrika ozelliklerini X-Plane ICINDE kendi kendine
// dener; her kontrolu Log.txt'ye "[plugin-api-test] SELFTEST PASS/FAIL ..." olarak yazar.
//
// Ne zaman calisir: eklenti klasorunde (…/Resources/plugins/plugin-api-test/) "selftest.flag"
// adli bir dosya varsa, eklenti acilirken. Dosya yoksa hicbir sey yapmaz (normal kullanim).
//
// Uc asama (kendi flight loop'undan):
//   1. hemen     : kutuphane ayarlari, fabrika (acilmadan once ayar), iki haritanin bagimsizligi,
//                  ad / klasor kurallari, ortak onbellek sinirlarinin buyumesi, dosyalar
//   2. 3 s sonra : (haritalar cizildikten sonra) kapat / yeniden ac, dosya - kod onceligi
//   3. 2 s sonra : haritalari birak, sinirlar geri inmeli; ozet satiri
//
// Test kendi haritalarini kullanir (maps/selftest_a, maps/selftest_b); eklentinin gomulu
// haritasina dokunmaz.
#pragma once

#include <memory>
#include <string>

#include <prime_map_api/PrimeMap.h>

class MultiMapSelfTest
{
public:
    ~MultiMapSelfTest();

    // pPluginDirectory: sonunda ayirici ile. Bayrak dosyasi yoksa false doner, test baslamaz.
    bool StartIfRequested(const std::string& pPluginDirectory);

private:
    static float FlightLoopCallback(float pSinceLastCall, float pSinceLastLoop, int pCounter, void* pRefcon);
    float Step();
    void Check(bool pCondition, const std::string& pName, const std::string& pDetail = "");
    void RunImmediate();
    void RunAfterDraw();
    void RunCleanup();

    std::unique_ptr<prime::Map> mFirst;
    std::unique_ptr<prime::Map> mSecond;
    prime::LibrarySettings mOriginalSettings;
    int mMapsBefore = 0;
    int mPhase = 0;
    float mPhaseStart = 0.0f;
    int mPassed = 0;
    int mFailed = 0;
    bool mRegistered = false;
};
