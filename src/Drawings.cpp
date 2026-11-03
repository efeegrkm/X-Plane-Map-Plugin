// Drawings.cpp
#include "Drawings.h"

#include "Log.h"

Drawings::Mode Drawings::GetMode() const
{
    return mMode;
}

const char* Drawings::ModeName(Mode pMode)
{
    switch (pMode)
    {
        case Mode::Pan:
            return "Gezin";
        case Mode::Point:
            return "Nokta";
        case Mode::Route:
            return "Rota";
        case Mode::Region:
            return "Bolge";
    }
    return "?";
}

void Drawings::SetMode(Mode pMode)
{
    if (pMode == mMode)
    {
        return;
    }
    Finish();
    mMode = pMode;
    Log("mod: %s", ModeName(pMode));
}

void Drawings::AddVertex(const LatLon& pPoint)
{
    switch (mMode)
    {
        case Mode::Pan:
        {
            return;  // gezinme modunda isaretleme yok
        }

        case Mode::Point:
        {
            Shape tShape;
            tShape.mKind = ShapeKind::Point;
            tShape.mNumber = ++CounterFor(ShapeKind::Point);
            tShape.mPoints.push_back(pPoint);
            mShapes.push_back(tShape);
            Log("P%d eklendi: %.5f, %.5f", tShape.mNumber, pPoint.mLat, pPoint.mLon);
            return;
        }

        case Mode::Route:
        case Mode::Region:
        {
            const ShapeKind tKind = (mMode == Mode::Route) ? ShapeKind::Route : ShapeKind::Region;
            if (!mHasCurrent)
            {
                mCurrent = Shape();
                mCurrent.mKind = tKind;
                mHasCurrent = true;
            }
            mCurrent.mPoints.push_back(pPoint);
            Log("%s kosesi %d: %.5f, %.5f", ModeName(mMode), static_cast<int>(mCurrent.mPoints.size()),
                pPoint.mLat, pPoint.mLon);
            return;
        }
    }
}

void Drawings::Finish()
{
    if (!mHasCurrent)
    {
        return;
    }
    mHasCurrent = false;

    Shape tShape = mCurrent;
    mCurrent = Shape();

    const size_t tNeeded = (tShape.mKind == ShapeKind::Route) ? 2 : 3;
    if (tShape.mPoints.size() < tNeeded)
    {
        Log("yarim cizim atildi (%d kose, en az %d gerekli)", static_cast<int>(tShape.mPoints.size()),
            static_cast<int>(tNeeded));
        return;
    }
    if (tShape.mKind == ShapeKind::Region)
    {
        if (!TriangulatePolygon(tShape.mPoints, tShape.mTriangles))
        {
            Log("bolge kendini kesiyor: sadece kenarlari cizilecek");
        }
    }
    tShape.mNumber = ++CounterFor(tShape.mKind);
    mShapes.push_back(tShape);
    Log("%s%d bitti (%d kose)", tShape.mKind == ShapeKind::Route ? "R" : "B", tShape.mNumber,
        static_cast<int>(tShape.mPoints.size()));
}

void Drawings::Undo()
{
    if (mHasCurrent && !mCurrent.mPoints.empty())
    {
        mCurrent.mPoints.pop_back();
        if (mCurrent.mPoints.empty())
        {
            mHasCurrent = false;
        }
        return;
    }
    if (!mShapes.empty())
    {
        const Shape& tLast = mShapes.back();
        int& tCounter = CounterFor(tLast.mKind);
        if (tCounter == tLast.mNumber)
        {
            --tCounter;
        }
        mShapes.pop_back();
    }
}

void Drawings::Clear()
{
    mShapes.clear();
    mCurrent = Shape();
    mHasCurrent = false;
    mCounters[0] = 0;
    mCounters[1] = 0;
    mCounters[2] = 0;
    Log("tum cizimler silindi");
}

const std::vector<Drawings::Shape>& Drawings::Shapes() const
{
    return mShapes;
}

const Drawings::Shape* Drawings::Current() const
{
    return mHasCurrent ? &mCurrent : nullptr;
}

int Drawings::ShapeCount() const
{
    return static_cast<int>(mShapes.size());
}

int& Drawings::CounterFor(ShapeKind pKind)
{
    return mCounters[static_cast<int>(pKind)];
}
