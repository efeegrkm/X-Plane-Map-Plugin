// AptDatParser.h
// apt.dat satirlarini veri yapilarina ceviren ayristirici. X-Plane'e
// bagimli DEGIL (testlerde metinle beslenir).
//
// Iki kullanim:
//   - ParseAirport: tek bir havalimaninin metnini (baslik satirindan bir
//     sonraki havalimanina kadar) AirportDetail'e cevirir. Ekrandaki
//     havalimanlari icin arka plan thread'inde cagrilir.
//   - Tek satir fonksiyonlari (baslik, pist): dosya taramasi (AptDatIndexer)
//     ozet cikarirken de ayni kodu kullanir.
#pragma once

#include <string>
#include <string_view>

#include "data/AirportDetail.h"

class AptLine;

namespace AptDatParser
{

// "1 <rakim ft> <eski> <eski> <kimlik> <ad...>" (16, 17 ayni bicim)
bool ParseHeader(std::string_view pLine, AirportType* pOutType, int* pOutElevationFt, std::string* pOutId,
                 std::string* pOutName);

// "100 <genislik> <yuzey> <omuz> <puruzsuzluk> <orta isik> <kenar isik> <tabela> <uc1 x 9> <uc2 x 9>"
bool ParseRunway(std::string_view pLine, Runway* pOut);

// "101 <genislik> <samandira> <uc1 no> <enlem> <boylam> <uc2 no> <enlem> <boylam>"
bool ParseWaterRunway(std::string_view pLine, Runway* pOut);

// Bir havalimaninin tum satirlari -> ayrinti. Ilk satir baslik olmali.
// Bozuk / taninmayan satirlar atlanir ve mSkippedLineCount'ta sayilir.
AirportDetail ParseAirport(std::string_view pText);

// Satir kodu, koordinat iceren bir satir mi (indeks taramasi kapsayan kutuyu
// bunlardan hesaplar). Evetse ilk iki sayi enlem / boylamdir.
bool HasLeadingLatLon(int pRowCode);

}  // namespace AptDatParser
