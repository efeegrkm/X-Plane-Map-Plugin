// Drawings.cpp
#include "Drawings.h"

#include <locale>
#include <sstream>

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
            return "Pan";
        case Mode::Point:
            return "Point";
        case Mode::Route:
            return "Route";
        case Mode::Region:
            return "Region";
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
    Log("mode: %s", ModeName(pMode));
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
            Log("P%d added: %.5f, %.5f", tShape.mNumber, pPoint.mLat, pPoint.mLon);
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
            Log("%s vertex %d: %.5f, %.5f", ModeName(mMode), static_cast<int>(mCurrent.mPoints.size()),
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
        Log("unfinished shape discarded (%d vertices, at least %d needed)", static_cast<int>(tShape.mPoints.size()),
            static_cast<int>(tNeeded));
        return;
    }
    if (tShape.mKind == ShapeKind::Region)
    {
        if (!TriangulatePolygon(tShape.mPoints, tShape.mTriangles))
        {
            Log("region intersects itself: only its outline will be drawn");
        }
    }
    tShape.mNumber = ++CounterFor(tShape.mKind);
    mShapes.push_back(tShape);
    Log("%s%d finished (%d vertices)", tShape.mKind == ShapeKind::Route ? "R" : "A", tShape.mNumber,
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
    Log("all drawings cleared");
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

std::string Drawings::LabelOf(const Shape& pShape)
{
    const char* tPrefix = "P";
    if (pShape.mKind == ShapeKind::Route)
    {
        tPrefix = "R";
    }
    else if (pShape.mKind == ShapeKind::Region)
    {
        tPrefix = "A";
    }
    return tPrefix + std::to_string(pShape.mNumber);
}

std::string Drawings::Serialize() const
{
    std::ostringstream tStream;
    tStream.imbue(std::locale::classic());  // ondalik ayirici her zaman nokta
    tStream.precision(12);  // ~0.1 mm: kayit/yukleme koordinati bozmasin
    tStream << "# Map Plugin drawings: P = point, R = route, A = area (region). <type> <number> <lat> <lon> ...\n";
    for (const Shape& tShape : mShapes)
    {
        tStream << LabelOf(tShape).substr(0, 1) << ' ' << tShape.mNumber;
        for (const LatLon& tPoint : tShape.mPoints)
        {
            tStream << ' ' << tPoint.mLat << ' ' << tPoint.mLon;
        }
        tStream << '\n';
    }
    return tStream.str();
}

int Drawings::Deserialize(const std::string& pText)
{
    mShapes.clear();
    mCurrent = Shape();
    mHasCurrent = false;
    mCounters[0] = 0;
    mCounters[1] = 0;
    mCounters[2] = 0;

    std::istringstream tLines(pText);
    std::string tLine;
    while (std::getline(tLines, tLine))
    {
        if (tLine.empty() || tLine[0] == '#')
        {
            continue;
        }
        std::istringstream tStream(tLine);
        tStream.imbue(std::locale::classic());
        std::string tType;
        Shape tShape;
        if (!(tStream >> tType >> tShape.mNumber) || tShape.mNumber <= 0)
        {
            continue;
        }
        if (tType == "P")
        {
            tShape.mKind = ShapeKind::Point;
        }
        else if (tType == "R")
        {
            tShape.mKind = ShapeKind::Route;
        }
        else if (tType == "A")
        {
            tShape.mKind = ShapeKind::Region;
        }
        else
        {
            continue;
        }
        LatLon tPoint;
        while (tStream >> tPoint.mLat >> tPoint.mLon)
        {
            tShape.mPoints.push_back(tPoint);
        }
        const size_t tNeeded = tShape.mKind == ShapeKind::Point ? 1 : (tShape.mKind == ShapeKind::Route ? 2 : 3);
        if (tShape.mPoints.size() < tNeeded)
        {
            continue;
        }
        if (tShape.mKind == ShapeKind::Region)
        {
            TriangulatePolygon(tShape.mPoints, tShape.mTriangles);
        }
        int& tCounter = CounterFor(tShape.mKind);
        if (tShape.mNumber > tCounter)
        {
            tCounter = tShape.mNumber;
        }
        mShapes.push_back(tShape);
    }
    return ShapeCount();
}
