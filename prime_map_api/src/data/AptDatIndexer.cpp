// AptDatIndexer.cpp
#include "data/AptDatIndexer.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>

#include "data/AptDatParser.h"
#include "data/AptLine.h"
#include "data/SceneryPacks.h"

namespace
{

const size_t kReadChunk = 4u << 20;  // dosya 4 MB'lik parcalarla okunur
const char kCacheMagic[4] = {'M', 'P', 'A', 'I'};
const uint32_t kCacheVersion = 1;  // AirportGeometry'nin kayitli alanlari degisirse artir
const char* kCacheFileName = "airport_index.bin";

// --- Onbellek dosyasi icin kucuk ikili okuma / yazma yardimcilari ---

class BinaryWriter
{
public:
    explicit BinaryWriter(std::ofstream& pFile)
        : mFile(pFile)
    {
    }

    template <typename TValue>
    void Value(const TValue& pValue)
    {
        mFile.write(reinterpret_cast<const char*>(&pValue), sizeof(TValue));
    }

    void Text(const std::string& pText)
    {
        Value(static_cast<uint32_t>(pText.size()));
        mFile.write(pText.data(), static_cast<std::streamsize>(pText.size()));
    }

private:
    std::ofstream& mFile;
};

class BinaryReader
{
public:
    explicit BinaryReader(std::ifstream& pFile)
        : mFile(pFile)
    {
    }

    template <typename TValue>
    bool Value(TValue& pValue)
    {
        return static_cast<bool>(mFile.read(reinterpret_cast<char*>(&pValue), sizeof(TValue)));
    }

    bool Text(std::string& pText)
    {
        uint32_t tSize = 0;
        if (!Value(tSize) || tSize > (16u << 20))
        {
            return false;
        }
        pText.resize(tSize);
        return static_cast<bool>(mFile.read(pText.data(), static_cast<std::streamsize>(tSize)));
    }

private:
    std::ifstream& mFile;
};

void WriteRunway(BinaryWriter& pOut, const Runway& pRunway)
{
    pOut.Value(pRunway.mWidthM);
    pOut.Value(pRunway.mSurface);
    pOut.Value(pRunway.mSurfaceCode);
    pOut.Value(pRunway.mShoulderCode);
    pOut.Value(pRunway.mCenterLights);
    pOut.Value(pRunway.mEdgeLights);
    pOut.Value(pRunway.mWater);
    for (const RunwayEnd& tEnd : pRunway.mEnds)
    {
        pOut.Text(tEnd.mName);
        pOut.Value(tEnd.mPosition);
        pOut.Value(tEnd.mDisplacedThresholdM);
        pOut.Value(tEnd.mOverrunM);
        pOut.Value(tEnd.mMarkings);
        pOut.Value(tEnd.mApproachLights);
        pOut.Value(tEnd.mTouchdownLights);
        pOut.Value(tEnd.mReil);
    }
}

bool ReadRunway(BinaryReader& pIn, Runway& pRunway)
{
    bool tOk = pIn.Value(pRunway.mWidthM) && pIn.Value(pRunway.mSurface) && pIn.Value(pRunway.mSurfaceCode) &&
               pIn.Value(pRunway.mShoulderCode) && pIn.Value(pRunway.mCenterLights) && pIn.Value(pRunway.mEdgeLights) &&
               pIn.Value(pRunway.mWater);
    for (RunwayEnd& tEnd : pRunway.mEnds)
    {
        tOk = tOk && pIn.Text(tEnd.mName) && pIn.Value(tEnd.mPosition) && pIn.Value(tEnd.mDisplacedThresholdM) &&
              pIn.Value(tEnd.mOverrunM) && pIn.Value(tEnd.mMarkings) && pIn.Value(tEnd.mApproachLights) &&
              pIn.Value(tEnd.mTouchdownLights) && pIn.Value(tEnd.mReil);
    }
    return tOk;
}

bool IsHeaderCode(int pCode)
{
    return pCode == 1 || pCode == 16 || pCode == 17;
}

}  // namespace

// ============================================================================
// AptDatScanner
// ============================================================================

AptDatScanner::AptDatScanner(int pFileIndex, std::unordered_set<std::string>& pSeen,
                             std::vector<AirportGeometry>& pOut)
    : mFileIndex(pFileIndex)
    , mSeen(pSeen)
    , mOut(pOut)
{
}

void AptDatScanner::Extend(const LatLon& pPoint)
{
    if (!mHasBounds)
    {
        mCurrent.mBounds = {pPoint.mLat, pPoint.mLon, pPoint.mLat, pPoint.mLon};
        mHasBounds = true;
        return;
    }
    GeoBounds& tBounds = mCurrent.mBounds;
    tBounds.mSouth = std::min(tBounds.mSouth, pPoint.mLat);
    tBounds.mNorth = std::max(tBounds.mNorth, pPoint.mLat);
    tBounds.mWest = std::min(tBounds.mWest, pPoint.mLon);
    tBounds.mEast = std::max(tBounds.mEast, pPoint.mLon);
}

void AptDatScanner::Close(uint64_t pEndOffset)
{
    if (mOpen && !mSkip && mHasBounds)
    {
        mCurrent.mLocation.mLength = static_cast<uint32_t>(pEndOffset - mCurrent.mLocation.mOffset);
        if (!mHasDatumLat || !mHasDatumLon)
        {
            mCurrent.mReference = {(mCurrent.mBounds.mSouth + mCurrent.mBounds.mNorth) * 0.5,
                                   (mCurrent.mBounds.mWest + mCurrent.mBounds.mEast) * 0.5};
        }
        mSeen.insert(mCurrent.mId);
        mRunwayCount += static_cast<int>(mCurrent.mRunways.size());
        mNodeCount += mCurrent.mNodeCount;
        ++mAirportCount;
        mOut.push_back(std::move(mCurrent));
    }
    mCurrent = AirportGeometry();
    mOpen = false;
    mSkip = false;
    mHasBounds = false;
    mHasDatumLat = false;
    mHasDatumLon = false;
}

void AptDatScanner::AddLine(std::string_view pLine, uint64_t pOffset)
{
    AptLine tLine(pLine);
    int tCode = 0;
    if (!tLine.Int(tCode))
    {
        return;  // bos satir, yorum, dosya basligi ("I", "1130 Version")
    }
    if (IsHeaderCode(tCode))
    {
        Close(pOffset);
        mOpen = AptDatParser::ParseHeader(pLine, &mCurrent.mType, &mCurrent.mElevationFt, &mCurrent.mId,
                                          &mCurrent.mName);
        mSkip = mOpen && mSeen.count(mCurrent.mId) > 0;
        mCurrent.mLocation.mFile = mFileIndex;
        mCurrent.mLocation.mOffset = pOffset;
        return;
    }
    if (tCode == 99)
    {
        Close(pOffset);
        return;
    }
    if (!mOpen || mSkip)
    {
        return;
    }

    LatLon tPoint;
    if (AptDatParser::HasLeadingLatLon(tCode))
    {
        if (tLine.Double(tPoint.mLat) && tLine.Double(tPoint.mLon))
        {
            Extend(tPoint);
        }
        if (tCode >= 111 && tCode <= 116)
        {
            ++mCurrent.mNodeCount;
        }
        return;
    }
    switch (tCode)
    {
        case 100:
        case 101:
        {
            Runway tRunway;
            const bool tOk = (tCode == 100) ? AptDatParser::ParseRunway(pLine, &tRunway)
                                            : AptDatParser::ParseWaterRunway(pLine, &tRunway);
            if (tOk)
            {
                Extend(tRunway.mEnds[0].mPosition);
                Extend(tRunway.mEnds[1].mPosition);
                mCurrent.mRunways.push_back(tRunway);
            }
            break;
        }
        case 102:
        {
            std::string_view tName;
            if (tLine.Token(tName) && tLine.Double(tPoint.mLat) && tLine.Double(tPoint.mLon))
            {
                Extend(tPoint);
            }
            break;
        }
        case 110:
            ++mCurrent.mPavementCount;
            break;
        case 54:
        case 1054:
            mCurrent.mHasTower = true;
            break;
        case 1302:
        {
            std::string_view tKey;
            if (!tLine.Token(tKey))
            {
                break;
            }
            if (tKey == "icao_code")
            {
                tLine.Token(mCurrent.mIcaoCode);
            }
            else if (tKey == "datum_lat")
            {
                mHasDatumLat = tLine.Double(mCurrent.mReference.mLat);
            }
            else if (tKey == "datum_lon")
            {
                mHasDatumLon = tLine.Double(mCurrent.mReference.mLon);
            }
            break;
        }
        default:
            break;
    }
}

void AptDatScanner::Finish(uint64_t pFileSize)
{
    Close(pFileSize);
}

int AptDatScanner::AirportCount() const
{
    return mAirportCount;
}

int AptDatScanner::RunwayCount() const
{
    return mRunwayCount;
}

int AptDatScanner::NodeCount() const
{
    return mNodeCount;
}

// ============================================================================
// AptDatIndexer
// ============================================================================

const char* AptDatIndexer::Name() const
{
    return "apt.dat indexer";
}

void AptDatIndexer::SetCacheDirectory(const std::string& pDirectory)
{
    mCacheDirectory = pDirectory;
}

std::string AptDatIndexer::CacheKey(const std::vector<std::string>& pFiles)
{
    std::string tKey = std::format("v{}", kCacheVersion);
    for (const std::string& tPath : pFiles)
    {
        std::error_code tError;
        const uintmax_t tSize = std::filesystem::file_size(tPath, tError);
        const auto tTime = std::filesystem::last_write_time(tPath, tError).time_since_epoch().count();
        tKey += std::format("|{}|{}|{}", tPath, tSize, tTime);
    }
    return tKey;
}

bool AptDatIndexer::SaveCache(const std::string& pPath, const std::string& pKey, const AirportIndex& pIndex)
{
    std::error_code tError;
    std::filesystem::create_directories(std::filesystem::path(pPath).parent_path(), tError);
    std::ofstream tFile(pPath, std::ios::binary | std::ios::trunc);
    if (!tFile)
    {
        return false;
    }
    BinaryWriter tOut(tFile);
    tFile.write(kCacheMagic, 4);
    tOut.Value(kCacheVersion);
    tOut.Text(pKey);
    tOut.Value(static_cast<uint32_t>(pIndex.mFiles.size()));
    for (const std::string& tPath : pIndex.mFiles)
    {
        tOut.Text(tPath);
    }
    tOut.Value(static_cast<uint32_t>(pIndex.mAirports.size()));
    for (const AirportGeometry& tAirport : pIndex.mAirports)
    {
        tOut.Text(tAirport.mId);
        tOut.Text(tAirport.mIcaoCode);
        tOut.Text(tAirport.mName);
        tOut.Value(tAirport.mType);
        tOut.Value(tAirport.mElevationFt);
        tOut.Value(tAirport.mHasTower);
        tOut.Value(tAirport.mReference);
        tOut.Value(tAirport.mBounds);
        tOut.Value(tAirport.mPavementCount);
        tOut.Value(tAirport.mNodeCount);
        tOut.Value(tAirport.mLocation);
        tOut.Value(static_cast<uint32_t>(tAirport.mRunways.size()));
        for (const Runway& tRunway : tAirport.mRunways)
        {
            WriteRunway(tOut, tRunway);
        }
    }
    return static_cast<bool>(tFile);
}

bool AptDatIndexer::LoadCache(const std::string& pPath, const std::string& pKey, AirportIndex& pOut)
{
    std::ifstream tFile(pPath, std::ios::binary);
    if (!tFile)
    {
        return false;
    }
    BinaryReader tIn(tFile);
    char tMagic[4] = {0, 0, 0, 0};
    uint32_t tVersion = 0;
    std::string tKey;
    if (!tFile.read(tMagic, 4) || std::memcmp(tMagic, kCacheMagic, 4) != 0 || !tIn.Value(tVersion) ||
        tVersion != kCacheVersion || !tIn.Text(tKey) || tKey != pKey)
    {
        return false;  // eski bicim ya da dosyalar degismis: yeniden taranacak
    }
    AirportIndex tIndex;
    uint32_t tCount = 0;
    if (!tIn.Value(tCount))
    {
        return false;
    }
    tIndex.mFiles.resize(tCount);
    for (std::string& tPath : tIndex.mFiles)
    {
        if (!tIn.Text(tPath))
        {
            return false;
        }
    }
    if (!tIn.Value(tCount))
    {
        return false;
    }
    tIndex.mAirports.resize(tCount);
    for (AirportGeometry& tAirport : tIndex.mAirports)
    {
        uint32_t tRunways = 0;
        if (!tIn.Text(tAirport.mId) || !tIn.Text(tAirport.mIcaoCode) || !tIn.Text(tAirport.mName) ||
            !tIn.Value(tAirport.mType) || !tIn.Value(tAirport.mElevationFt) || !tIn.Value(tAirport.mHasTower) ||
            !tIn.Value(tAirport.mReference) || !tIn.Value(tAirport.mBounds) || !tIn.Value(tAirport.mPavementCount) ||
            !tIn.Value(tAirport.mNodeCount) || !tIn.Value(tAirport.mLocation) || !tIn.Value(tRunways) ||
            tRunways > 10000)
        {
            return false;
        }
        tAirport.mRunways.resize(tRunways);
        for (Runway& tRunway : tAirport.mRunways)
        {
            if (!ReadRunway(tIn, tRunway))
            {
                return false;
            }
        }
    }
    pOut = std::move(tIndex);
    return true;
}

std::vector<std::string> AptDatIndexer::AptFiles(const std::string& pXPlaneDirectory)
{
    std::vector<std::string> tFiles;
    for (const std::string& tPack : ReadSceneryPacks(pXPlaneDirectory))
    {
        tFiles.push_back(tPack + "Earth nav data/apt.dat");
    }
    tFiles.push_back(pXPlaneDirectory + "Resources/default scenery/default apt dat/Earth nav data/apt.dat");
    return tFiles;
}

bool AptDatIndexer::ScanFile(const std::string& pPath, AptDatScanner& pScanner, const std::atomic<bool>& pCancel)
{
    std::ifstream tFile(pPath, std::ios::binary);
    std::vector<char> tChunk(kReadChunk);
    std::string tCarry;     // onceki parcadan kalan yarim satir
    uint64_t tCarryOffset = 0;  // yarim satirin dosyadaki baslangici
    uint64_t tChunkOffset = 0;  // bu parcanin dosyadaki baslangici
    while (tFile)
    {
        if (pCancel.load())
        {
            return false;
        }
        tFile.read(tChunk.data(), static_cast<std::streamsize>(tChunk.size()));
        const size_t tRead = static_cast<size_t>(tFile.gcount());
        if (tRead == 0)
        {
            break;
        }
        const std::string_view tData(tChunk.data(), tRead);
        size_t tLineStart = 0;
        while (true)
        {
            const size_t tNewline = tData.find('\n', tLineStart);
            if (tNewline == std::string_view::npos)
            {
                break;
            }
            if (!tCarry.empty())
            {
                // Parca sinirinda bolunmus satir: onceki parcadaki basiyla birlestir
                tCarry.append(tData.substr(0, tNewline));
                pScanner.AddLine(tCarry, tCarryOffset);
                tCarry.clear();
            }
            else
            {
                pScanner.AddLine(tData.substr(tLineStart, tNewline - tLineStart), tChunkOffset + tLineStart);
            }
            tLineStart = tNewline + 1;
        }
        if (tLineStart < tRead)
        {
            if (tCarry.empty())
            {
                tCarryOffset = tChunkOffset + tLineStart;
            }
            tCarry.append(tData.substr(tLineStart));
        }
        tChunkOffset += tRead;
    }
    if (!tCarry.empty())
    {
        pScanner.AddLine(tCarry, tCarryOffset);
    }
    pScanner.Finish(tChunkOffset);
    return true;
}

bool AptDatIndexer::LoadAll(const std::string& pXPlaneDirectory, const std::atomic<bool>& pCancel,
                            AirportIndex& pOut, std::string& pOutReport)
{
    const auto tStart = std::chrono::steady_clock::now();
    std::vector<std::string> tFiles;
    for (const std::string& tPath : AptFiles(pXPlaneDirectory))
    {
        std::error_code tError;
        if (std::filesystem::is_regular_file(tPath, tError))
        {
            tFiles.push_back(tPath);  // paketin apt.dat'i yoksa atlanir (cogu manzara paketinde olmaz)
        }
    }

    // Onbellek: dosyalar degismediyse taramaya gerek yok
    const std::string tKey = CacheKey(tFiles);
    const std::string tCachePath = mCacheDirectory.empty() ? "" : mCacheDirectory + kCacheFileName;
    if (!tCachePath.empty() && LoadCache(tCachePath, tKey, pOut))
    {
        const double tSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - tStart).count();
        pOutReport += std::format("{} apt.dat files, {} airports, from cache {} ({:.2f} s)\n", pOut.mFiles.size(),
                                  pOut.mAirports.size(), tCachePath, tSeconds);
        return !pOut.mFiles.empty();
    }

    std::unordered_set<std::string> tSeen;
    int tNodes = 0;
    for (const std::string& tPath : tFiles)
    {
        const int tFileIndex = static_cast<int>(pOut.mFiles.size());
        AptDatScanner tScanner(tFileIndex, tSeen, pOut.mAirports);
        if (!ScanFile(tPath, tScanner, pCancel))
        {
            pOutReport += "cancelled\n";
            return false;
        }
        pOut.mFiles.push_back(tPath);
        tNodes += tScanner.NodeCount();
        pOutReport += std::format("  {}: {} airports, {} runways, {} nodes\n", tPath, tScanner.AirportCount(),
                                  tScanner.RunwayCount(), tScanner.NodeCount());
    }
    const double tSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - tStart).count();
    pOutReport += std::format("{} apt.dat files, {} airports, {} nodes, scanned in {:.2f} s\n", pOut.mFiles.size(),
                              pOut.mAirports.size(), tNodes, tSeconds);
    if (!tCachePath.empty())
    {
        pOutReport += SaveCache(tCachePath, tKey, pOut) ? std::format("  cache written: {}\n", tCachePath)
                                                         : std::format("  cannot write cache: {}\n", tCachePath);
    }
    return !pOut.mFiles.empty();
}
