// AptDatIndexer.cpp
#include "data/AptDatIndexer.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>

#include "data/AptDatParser.h"
#include "data/AptLine.h"
#include "data/SceneryPacks.h"

namespace
{

const size_t kReadChunk = 4u << 20;  // dosya 4 MB'lik parcalarla okunur

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
    std::unordered_set<std::string> tSeen;
    int tNodes = 0;
    for (const std::string& tPath : AptFiles(pXPlaneDirectory))
    {
        std::error_code tError;
        if (!std::filesystem::is_regular_file(tPath, tError))
        {
            continue;  // paketin apt.dat'i yok (cogu manzara paketinde olmaz)
        }
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
    pOutReport += std::format("{} apt.dat files, {} airports, {} nodes, {:.1f} s\n", pOut.mFiles.size(),
                              pOut.mAirports.size(), tNodes, tSeconds);
    return !pOut.mFiles.empty();
}
