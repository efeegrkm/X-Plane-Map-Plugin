// AptDatReader.cpp
// *** GECICI *** EKIP MOTORU GELINCE DEGISTIRILECEK (bkz. AptDatReader.h)
#include "data/AptDatReader.h"

#include <array>
#include <format>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <sstream>

#include "data/SceneryPacks.h"

namespace
{

const size_t kReadChunk = 4 * 1024 * 1024;  // dosya 4 MB'lik parcalarla okunur
const int kCancelCheckLines = 20000;         // bu kadar satirda bir iptal kontrolu

// --- Kucuk ayristirma yardimcilari (yerel ayardan bagimsiz: ondalik hep nokta) ---

void SkipSpaces(const char*& pCursor, const char* pEnd)
{
    while (pCursor < pEnd && (*pCursor == ' ' || *pCursor == '\t'))
    {
        ++pCursor;
    }
}

bool ReadToken(const char*& pCursor, const char* pEnd, std::string* pOut)
{
    SkipSpaces(pCursor, pEnd);
    const char* tStart = pCursor;
    while (pCursor < pEnd && *pCursor != ' ' && *pCursor != '\t' && *pCursor != '\r')
    {
        ++pCursor;
    }
    if (pCursor == tStart)
    {
        return false;
    }
    pOut->assign(tStart, pCursor);
    return true;
}

bool ReadInt(const char*& pCursor, const char* pEnd, int* pOut)
{
    SkipSpaces(pCursor, pEnd);
    bool tNegative = false;
    if (pCursor < pEnd && (*pCursor == '-' || *pCursor == '+'))
    {
        tNegative = (*pCursor == '-');
        ++pCursor;
    }
    if (pCursor >= pEnd || *pCursor < '0' || *pCursor > '9')
    {
        return false;
    }
    int tValue = 0;
    while (pCursor < pEnd && *pCursor >= '0' && *pCursor <= '9')
    {
        tValue = tValue * 10 + (*pCursor - '0');
        ++pCursor;
    }
    *pOut = tNegative ? -tValue : tValue;
    return true;
}

// strtod yerel ayara (Turkce Windows'ta virgul) bagli; bu yuzden kendimiz okuyoruz
bool ReadDouble(const char*& pCursor, const char* pEnd, double* pOut)
{
    SkipSpaces(pCursor, pEnd);
    bool tNegative = false;
    if (pCursor < pEnd && (*pCursor == '-' || *pCursor == '+'))
    {
        tNegative = (*pCursor == '-');
        ++pCursor;
    }
    double tValue = 0.0;
    bool tHasDigit = false;
    while (pCursor < pEnd && *pCursor >= '0' && *pCursor <= '9')
    {
        tValue = tValue * 10.0 + (*pCursor - '0');
        ++pCursor;
        tHasDigit = true;
    }
    if (pCursor < pEnd && *pCursor == '.')
    {
        ++pCursor;
        double tScale = 0.1;
        while (pCursor < pEnd && *pCursor >= '0' && *pCursor <= '9')
        {
            tValue += (*pCursor - '0') * tScale;
            tScale *= 0.1;
            ++pCursor;
            tHasDigit = true;
        }
    }
    if (!tHasDigit)
    {
        return false;
    }
    *pOut = tNegative ? -tValue : tValue;
    return true;
}

bool ReadRunwayEnd(const char*& pCursor, const char* pEnd, RunwayEnd* pOut)
{
    int tIgnored = 0;
    return ReadToken(pCursor, pEnd, &pOut->mName) && ReadDouble(pCursor, pEnd, &pOut->mPosition.mLat) &&
           ReadDouble(pCursor, pEnd, &pOut->mPosition.mLon) &&
           ReadDouble(pCursor, pEnd, &pOut->mDisplacedThresholdM) && ReadDouble(pCursor, pEnd, &pOut->mOverrunM) &&
           ReadInt(pCursor, pEnd, &tIgnored) && ReadInt(pCursor, pEnd, &tIgnored) &&  // isaret, yaklasma isigi
           ReadInt(pCursor, pEnd, &tIgnored) && ReadInt(pCursor, pEnd, &tIgnored);    // TDZ, REIL
}

bool FileExists(const std::string& pPath)
{
    FILE* tFile = std::fopen(pPath.c_str(), "rb");
    if (!tFile)
    {
        return false;
    }
    std::fclose(tFile);
    return true;
}

}  // namespace

// ============================================================================
// AptDatParser
// ============================================================================

AptDatParser::AptDatParser(std::unordered_set<std::string>& pSeen, std::vector<AirportGeometry>& pOut)
    : mSeen(pSeen)
    , mOut(pOut)
{
}

void AptDatParser::ParseLine(const char* pLine, size_t pLength)
{
    // Hizli eleme: ilgilendigimiz satir kodlari (1, 16, 17, 100, 99) '1' veya '9' ile baslar
    if (pLength < 2 || (pLine[0] != '1' && pLine[0] != '9'))
    {
        return;
    }
    const char* tCursor = pLine;
    const char* tEnd = pLine + pLength;
    int tCode = 0;
    if (!ReadInt(tCursor, tEnd, &tCode))
    {
        return;
    }

    if (tCode == 1 || tCode == 16 || tCode == 17)
    {
        CloseAirport();
        std::string tIcao;
        std::string tName;
        if (ParseAirportHeader(pLine, pLength, &tIcao, &tName))
        {
            mHasCurrent = true;
            mSkipCurrent = mSeen.count(tIcao) > 0;
            mCurrent.mIcao = tIcao;
            mCurrent.mName = tName;
        }
        return;
    }
    if (tCode == 100 && mHasCurrent && !mSkipCurrent)
    {
        Runway tRunway;
        if (ParseRunway(pLine, pLength, &tRunway))
        {
            mCurrent.mRunways.push_back(tRunway);
        }
        return;
    }
    if (tCode == 99)
    {
        CloseAirport();
    }
}

void AptDatParser::Finish()
{
    CloseAirport();
}

int AptDatParser::RunwayCount() const
{
    return mRunwayCount;
}

bool AptDatParser::ParseAirportHeader(const char* pLine, size_t pLength, std::string* pOutIcao,
                                      std::string* pOutName)
{
    const char* tCursor = pLine;
    const char* tEnd = pLine + pLength;
    int tCode = 0;
    int tElevation = 0;
    int tDeprecated = 0;
    if (!ReadInt(tCursor, tEnd, &tCode) || !ReadInt(tCursor, tEnd, &tElevation) ||
        !ReadInt(tCursor, tEnd, &tDeprecated) || !ReadInt(tCursor, tEnd, &tDeprecated) ||
        !ReadToken(tCursor, tEnd, pOutIcao))
    {
        return false;
    }
    // Ad: satirin geri kalani (bosluklar dahil), sondaki \r atilir
    SkipSpaces(tCursor, tEnd);
    const char* tNameEnd = tEnd;
    while (tNameEnd > tCursor && (tNameEnd[-1] == '\r' || tNameEnd[-1] == ' '))
    {
        --tNameEnd;
    }
    pOutName->assign(tCursor, tNameEnd);
    return true;
}

bool AptDatParser::ParseRunway(const char* pLine, size_t pLength, Runway* pOut)
{
    const char* tCursor = pLine;
    const char* tEnd = pLine + pLength;
    int tCode = 0;
    int tSurface = 0;
    int tIgnored = 0;
    double tSmoothness = 0.0;
    if (!ReadInt(tCursor, tEnd, &tCode) || tCode != 100 || !ReadDouble(tCursor, tEnd, &pOut->mWidthM) ||
        !ReadInt(tCursor, tEnd, &tSurface) || !ReadInt(tCursor, tEnd, &tIgnored) ||  // omuz
        !ReadDouble(tCursor, tEnd, &tSmoothness) || !ReadInt(tCursor, tEnd, &tIgnored) ||  // orta hat isigi
        !ReadInt(tCursor, tEnd, &tIgnored) || !ReadInt(tCursor, tEnd, &tIgnored))  // kenar isigi, tabela
    {
        return false;
    }
    pOut->mSurface = SurfaceFromAptCode(tSurface);
    return ReadRunwayEnd(tCursor, tEnd, &pOut->mEnds[0]) && ReadRunwayEnd(tCursor, tEnd, &pOut->mEnds[1]);
}

void AptDatParser::CloseAirport()
{
    if (mHasCurrent && !mSkipCurrent && !mCurrent.mRunways.empty())
    {
        mSeen.insert(mCurrent.mIcao);
        mRunwayCount += static_cast<int>(mCurrent.mRunways.size());
        mOut.push_back(std::move(mCurrent));
    }
    mCurrent = AirportGeometry();
    mHasCurrent = false;
    mSkipCurrent = false;
}

// ============================================================================
// AptDatReader
// ============================================================================

const char* AptDatReader::Name() const
{
    return "temporary apt.dat reader (AptDatReader)";
}

std::vector<std::string> AptDatReader::AptFilesFromSceneryPacks(const std::string& pIniText,
                                                                const std::string& pXPlaneDirectory)
{
    std::vector<std::string> tFiles;
    for (const std::string& tPack : ParseSceneryPacks(pIniText, pXPlaneDirectory))
    {
        tFiles.push_back(tPack + "Earth nav data/apt.dat");
    }
    return tFiles;
}

bool AptDatReader::LoadAll(const std::string& pXPlaneDirectory, const std::atomic<bool>& pCancel,
                           std::vector<AirportGeometry>& pOutAirports, std::string& pOutReport)
{
    const std::chrono::steady_clock::time_point tStart = std::chrono::steady_clock::now();

    // 1) scenery_packs.ini sirasi
    std::vector<std::string> tFiles;
    FILE* tIni = std::fopen((pXPlaneDirectory + "Custom Scenery/scenery_packs.ini").c_str(), "rb");
    if (tIni)
    {
        std::string tText;
        std::array<char, 4096> tBuffer;
        size_t tRead = 0;
        while ((tRead = std::fread(tBuffer.data(), 1, tBuffer.size(), tIni)) > 0)
        {
            tText.append(tBuffer.data(), tRead);
        }
        std::fclose(tIni);
        tFiles = AptFilesFromSceneryPacks(tText, pXPlaneDirectory);
    }
    else
    {
        pOutReport += "scenery_packs.ini not found; reading only the default apt.dat\n";
    }
    // 2) en son default
    tFiles.push_back(pXPlaneDirectory + "Resources/default scenery/default apt dat/Earth nav data/apt.dat");

    std::unordered_set<std::string> tSeen;
    int tFilesRead = 0;
    for (const std::string& tFile : tFiles)
    {
        if (!FileExists(tFile))
        {
            continue;  // paketin apt.dat'i yok (cogu manzara paketinde olmaz)
        }
        const size_t tBefore = pOutAirports.size();
        AptDatParser tParser(tSeen, pOutAirports);
        if (!ReadFile(tFile, tParser, pCancel))
        {
            pOutReport += "cancelled\n";
            return false;
        }
        ++tFilesRead;
        pOutReport += std::format("  {}: {} airports, {} runways\n", tFile, pOutAirports.size() - tBefore,
                                  tParser.RunwayCount());
    }

    const double tSeconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - tStart).count();
    pOutReport += std::format("{} apt.dat files, {} airports, {:.1f} s\n", tFilesRead, pOutAirports.size(), tSeconds);
    return tFilesRead > 0;
}

bool AptDatReader::ReadFile(const std::string& pPath, AptDatParser& pParser, const std::atomic<bool>& pCancel)
{
    FILE* tFile = std::fopen(pPath.c_str(), "rb");
    if (!tFile)
    {
        return true;  // okunamayan dosyayi atla (iptal degil)
    }
    std::vector<char> tBuffer(kReadChunk);
    size_t tCarry = 0;  // onceki parcadan kalan yarim satirin uzunlugu
    int tLines = 0;
    bool tCancelled = false;
    while (!tCancelled)
    {
        const size_t tRead = std::fread(tBuffer.data() + tCarry, 1, tBuffer.size() - tCarry, tFile);
        const size_t tFilled = tCarry + tRead;
        if (tFilled == 0)
        {
            break;
        }
        size_t tLineStart = 0;
        for (size_t tIndex = 0; tIndex < tFilled; ++tIndex)
        {
            if (tBuffer[tIndex] != '\n')
            {
                continue;
            }
            pParser.ParseLine(tBuffer.data() + tLineStart, tIndex - tLineStart);
            tLineStart = tIndex + 1;
            if (++tLines % kCancelCheckLines == 0 && pCancel.load())
            {
                tCancelled = true;
                break;
            }
        }
        if (tRead == 0)
        {
            // dosya sonu: son satirda \n olmayabilir
            if (tLineStart < tFilled)
            {
                pParser.ParseLine(tBuffer.data() + tLineStart, tFilled - tLineStart);
            }
            break;
        }
        // Yarim kalan satiri tamponun basina tasi
        tCarry = tFilled - tLineStart;
        if (tCarry == tBuffer.size())
        {
            tBuffer.resize(tBuffer.size() * 2);  // 4 MB'den uzun satir (olmamali): tamponu buyut
        }
        else if (tCarry > 0)
        {
            std::memmove(tBuffer.data(), tBuffer.data() + tLineStart, tCarry);
        }
    }
    std::fclose(tFile);
    pParser.Finish();
    return !tCancelled;
}
