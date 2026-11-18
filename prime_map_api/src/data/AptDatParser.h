// apt.dat satirlarini havalimani ayrinti yapilarina ceviren ayristirici.
#ifndef PRIME_MAP_SRC_DATA_APTDATPARSER_H
#define PRIME_MAP_SRC_DATA_APTDATPARSER_H

#include <string>
#include <string_view>

#include "data/AirportDetail.h"

class AptLine;

namespace AptDatParser
{
bool parseHeader(std::string_view pLine, AirportType* pOutType, int* pOutElevationFt, std::string* pOutId,
                 std::string* pOutName);

bool parseRunway(std::string_view pLine, Runway* pOut);

bool parseWaterRunway(std::string_view pLine, Runway* pOut);

AirportDetail parseAirport(std::string_view pText);

bool hasLeadingLatLon(int pRowCode);
}
#endif
