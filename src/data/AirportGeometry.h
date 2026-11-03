// AirportGeometry.h
// Havalimani geometrisi (pistler) veri yapilari ve bu veriyi ureten
// kaynagin arayuzu. X-Plane'e bagimli DEGIL.
//
// ============================================================================
// EKIP apt.dat MOTORUNU BAGLAMA (yerel AI / gelistirici icin talimat)
// ============================================================================
// Haritanin pist katmani apt.dat'i dogrudan bilmez; sadece bu dosyadaki
// IAirportGeometrySource arayuzunu bilir. Su an arayuzu GECICI bir okuyucu
// (data/AptDatReader.*) uyguluyor. Ekip motorunu baglamak icin:
//
//   1. IAirportGeometrySource'dan tureyen bir adaptor sinifi yaz
//      (ornek ad: data/TeamAptDatSource.h/.cpp). LoadAll() icinde ekip
//      motorunu calistir ve sonucunu asagidaki AirportGeometry yapilarina
//      cevir. Birimler: derece (WGS84, kuzey/dogu +), metre.
//   2. src/Plugin.cpp'de "MOTOR BAGLAMA NOKTASI" yazan TEK satirda
//      std::make_unique<AptDatReader>() yerine kendi sinifini ver.
//   3. Yeni .cpp dosyasini CMakeLists.txt'de PLUGIN_SOURCES'a ekle.
//      (Ekip motorunun kendi dosyalari/kutuphanesi de oraya eklenir.)
//   4. AptDatReader'a dokunmak gerekmez; istersen silebilirsin (testleri de
//      tests/tests.cpp'deki TestAptDatReader'dadir).
//
// Kurallar:
//   - LoadAll ARKA PLAN thread'inde cagrilir: icinde hicbir XPLM fonksiyonu
//     (XPLMDebugString / Log dahil) cagrilmamali. Log'a yazilacak her sey
//     pOutReport'a eklenir; ana thread sonra Log.txt'ye yazar.
//   - pCancel true olursa (X-Plane kapaniyor / plugin yeniden yukleniyor)
//     olabildigince cabuk false donerek cik.
//   - Ayni ICAO bir kez verilmeli. X-Plane'in kurali: Custom Scenery
//     paketleri scenery_packs.ini sirasiyla, sonra default apt.dat; once gelen
//     kazanir. (Ekip motoru bunu zaten yapiyorsa oldugu gibi kullan.)
//   - Pisti olmayan havalimanlarini (heliport vb.) vermek zorunlu degil.
// ============================================================================
#pragma once

#include <atomic>
#include <string>
#include <vector>

#include "Geo.h"

// Pist yuzeyi, cizim rengi icin gruplanmis (apt.dat yuzey kodlari parantezde)
enum class RunwaySurface : unsigned char
{
    Paved,        // asfalt (1), beton (2); XP12'nin asfalt/beton turleri (20-38, 50-57)
    Unpaved,      // cim (3), toprak (4), cakil (5), kuru gol yatagi (12)
    Water,        // su (13)
    Snow,         // kar / buz (14)
    Transparent,  // saydam (15): sadece isaretler var, yuzey cizilmez
};

struct RunwayEnd
{
    std::string mName;                  // pist numarasi: "03R", "16", "H1"
    LatLon mPosition = {0.0, 0.0};      // pist ucunun orta noktasi (kaydirilmis esik DAHIL degil)
    double mDisplacedThresholdM = 0.0;  // kaydirilmis esik: uctan iceri, metre
    double mOverrunM = 0.0;             // blast pad / stopway: uctan disari, metre
};

struct Runway
{
    double mWidthM = 0.0;
    RunwaySurface mSurface = RunwaySurface::Paved;
    RunwayEnd mEnds[2];
};

struct AirportGeometry
{
    std::string mIcao;  // X-Plane'deki havalimani kimligi (nav verisindekiyle ayni)
    std::string mName;
    std::vector<Runway> mRunways;
    // Ileride (motor verebiliyorsa): taksi yollari, apron poligonlari, binalar...
};

class IAirportGeometrySource
{
public:
    virtual ~IAirportGeometrySource() = default;

    // Log'da gorunen ad (ornek: "gecici apt.dat okuyucu")
    virtual const char* Name() const = 0;

    // Tum havalimanlarini okuyup pOutAirports'a ekler. ARKA PLAN thread'inde
    // cagrilir (yukaridaki kurallar). pXPlaneDirectory: "C:\X-Plane 11\"
    // gibi, sonunda ayirici ile. pOutReport: Log.txt'ye yazilacak kisa rapor.
    // Basarisizsa false (pOutReport'a nedeni yazilir).
    virtual bool LoadAll(const std::string& pXPlaneDirectory, const std::atomic<bool>& pCancel,
                         std::vector<AirportGeometry>& pOutAirports, std::string& pOutReport) = 0;
};

// apt.dat yuzey kodunu gruba cevirir (bilinmeyen kod: Paved)
RunwaySurface SurfaceFromAptCode(int pCode);
