// apt.dat satirlarini veri yapilarina ceviren ayristirici. X-Plane'e
// bagimli DEGIL (testlerde metinle beslenir).
//
// Iki kullanim:
//   - parseAirport: tek bir havalimaninin metnini AirportDetail'e cevirir
//     (ekrandaki havalimanlari, arka plan thread'i).
//   - Tek satir fonksiyonlari (baslik, pist): dosya taramasi (AptDatIndexer)
//     ozet cikarirken de ayni kodu kullanir.
#ifndef PRIME_MAP_SRC_DATA_APTDATPARSER_H
#define PRIME_MAP_SRC_DATA_APTDATPARSER_H

#include <string>
#include <string_view>

#include "data/AirportDetail.h"

class AptLine;

namespace AptDatParser
{

// "1 <rakim ft> <eski> <eski> <kimlik> <ad...>" (16, 17 ayni bicim)
bool parseHeader(std::string_view pLine, AirportType* pOutType, int* pOutElevationFt, std::string* pOutId,
                 std::string* pOutName);

// "100 <genislik> <yuzey> <omuz> <puruzsuzluk> <orta isik> <kenar isik> <tabela> <uc1 x 9> <uc2 x 9>"
bool parseRunway(std::string_view pLine, Runway* pOut);

// "101 <genislik> <samandira> <uc1 no> <enlem> <boylam> <uc2 no> <enlem> <boylam>"
bool parseWaterRunway(std::string_view pLine, Runway* pOut);

// Bir havalimaninin tum satirlari -> ayrinti. Ilk satir baslik olmali.
// Bozuk / taninmayan satirlar atlanir ve mSkippedLineCount'ta sayilir.
AirportDetail parseAirport(std::string_view pText);

// Satir kodu, koordinat iceren bir satir mi (indeks taramasi kapsayan kutuyu
// bunlardan hesaplar). Evetse ilk iki sayi enlem / boylamdir.
bool hasLeadingLatLon(int pRowCode);

}  // namespace AptDatParser
#endif  // PRIME_MAP_SRC_DATA_APTDATPARSER_H
