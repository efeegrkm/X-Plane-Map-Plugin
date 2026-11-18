// Coklu harita ve fabrika ozelliklerinin X-Plane icindeki otomatik testi.
// Eklenti klasorunde selftest.flag varsa acilista calisir; her kontrolu Log.txt'ye yazar.
#ifndef PRIME_MAP_TEST_SRC_MULTIXMAPSELFTEST_H
#define PRIME_MAP_TEST_SRC_MULTIXMAPSELFTEST_H

#include <memory>
#include <string>

#include <prime_map_api/PrimeMap.h>

class MultiXMapSelfTest
{
public:
    ~MultiXMapSelfTest();

    bool startIfRequested(const std::string& pPluginDirectory, prime::XMapManager* pManager);

private:
    static float flightLoopCallback(float pSinceLastCall, float pSinceLastLoop, int pCounter, void* pRefcon);
    float step();
    void Check(bool pCondition, const std::string& pName, const std::string& pDetail = "");
    void runImmediate();
    void runAfterDraw();
    void runCleanup();

    prime::XMap* mFirst = nullptr;
    prime::XMap* mSecond = nullptr;
    prime::XMapManager* mManager = nullptr;
    prime::XMapManagerSettings mOriginalSettings;
    int mMapsBefore = 0;
    int mPhase = 0;
    float mPhaseStart = 0.0f;
    int mPassed = 0;
    int mFailed = 0;
    bool mRegistered = false;
};
#endif
