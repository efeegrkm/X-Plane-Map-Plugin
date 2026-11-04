// TrackLayer.cpp
#include "layers/TrackLayer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "MapView.h"
#include "data/AircraftData.h"
#include "gfx/Canvas.h"

namespace
{

const float kSampleInterval = 1.0f;   // saniye
const double kMinMoveNm = 0.03;       // ~55 m: bundan az hareket kaydedilmez
const double kJumpNm = 20.0;          // bundan buyuk sicrama: yeni iz parcasi
const int kMaxPoints = 20000;         // ~5.5 saatlik ucus
const float kMinScreenStep = 1.5f;    // ekranda bundan yakin noktalar atlanir (boxel)
const Rgba kTrackColor = {0.98f, 0.78f, 0.05f, 0.95f};

}  // namespace

TrackLayer::TrackLayer(const AircraftData& pAircraft)
    : Layer("track", "Flight track", true)
    , mAircraft(pAircraft)
{
}

void TrackLayer::Update(float pNow)
{
    if (pNow - mLastSampleTime < kSampleInterval)
    {
        return;
    }
    mLastSampleTime = pNow;

    const AircraftState& tState = mAircraft.State();
    if (!tState.mValid)
    {
        return;
    }
    if (!mSegments.empty() && !mSegments.back().empty())
    {
        const double tMoved = DistanceNm(mSegments.back().back(), tState.mPosition);
        if (tMoved < kMinMoveNm)
        {
            return;
        }
        if (tMoved > kJumpNm)
        {
            mSegments.emplace_back();  // ucak tasindi: eski izle birlestirme
        }
    }
    if (mSegments.empty())
    {
        mSegments.emplace_back();
    }
    mSegments.back().push_back(tState.mPosition);
    ++mPointCount;
    if (mPointCount > kMaxPoints)
    {
        TrimOldest();
    }
}

void TrackLayer::TrimOldest()
{
    // En eski parcanin ilk %5'ini sil (her noktada silmek yavas olurdu)
    std::vector<LatLon>& tOldest = mSegments.front();
    const size_t tRemove = std::max<size_t>(1, kMaxPoints / 20);
    if (tOldest.size() <= tRemove)
    {
        mPointCount -= static_cast<int>(tOldest.size());
        mSegments.erase(mSegments.begin());
        return;
    }
    tOldest.erase(tOldest.begin(), tOldest.begin() + static_cast<std::ptrdiff_t>(tRemove));
    mPointCount -= static_cast<int>(tRemove);
}

void TrackLayer::Draw(const DrawContext& pContext)
{
    Gfx::OutlinedLines tLines;
    for (const std::vector<LatLon>& tSegment : mSegments)
    {
        std::vector<Gfx::Point> tScreen;
        tScreen.reserve(tSegment.size());
        for (const LatLon& tPoint : tSegment)
        {
            const Gfx::Point tNew = pContext.mView->ToScreen(tPoint);
            if (!tScreen.empty() && std::fabs(tNew.mX - tScreen.back().mX) < kMinScreenStep &&
                std::fabs(tNew.mY - tScreen.back().mY) < kMinScreenStep)
            {
                continue;  // uzaktan bakinca ust uste binen noktalar: cizgi icin gereksiz
            }
            tScreen.push_back(tNew);
        }
        tLines.AddPolyline(tScreen, false, 2.0f, kTrackColor);
    }
    pContext.mCanvas->Draw(tLines);
}

void TrackLayer::Clear()
{
    mSegments.clear();
    mPointCount = 0;
}

int TrackLayer::PointCount() const
{
    return mPointCount;
}
