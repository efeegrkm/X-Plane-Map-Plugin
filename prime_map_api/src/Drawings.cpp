#include "Drawings.h"

#include <locale>
#include <sstream>

#include "Log.h"

Drawings::Mode Drawings::getMode() const
{
    return mMode;
}

const char* Drawings::modeName(Mode pMode)
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

void Drawings::setMode(Mode pMode)
{
    if (pMode == mMode)
    {
        return;
    }
    finish();
    mMode = pMode;
    log("mode: {}", modeName(pMode));
}

void Drawings::addVertex(const LatLon& pPoint)
{
    switch (mMode)
    {
        case Mode::Pan:
        {
            return;
        }

        case Mode::Point:
        {
            Shape tShape;
            tShape.mKind = ShapeKind::Point;
            tShape.mNumber = ++counterFor(ShapeKind::Point);
            tShape.mPoints.push_back(pPoint);
            mShapes.push_back(tShape);
            log("P{} added: {:.5f}, {:.5f}", tShape.mNumber, pPoint.mLat, pPoint.mLon);
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
            log("{} vertex {}: {:.5f}, {:.5f}", modeName(mMode), static_cast<int>(mCurrent.mPoints.size()),
                pPoint.mLat, pPoint.mLon);
            return;
        }
    }
}

void Drawings::finish()
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
        log("unfinished shape discarded ({} vertices, at least {} needed)", static_cast<int>(tShape.mPoints.size()),
            static_cast<int>(tNeeded));
        return;
    }
    if (tShape.mKind == ShapeKind::Region)
    {
        if (!triangulatePolygon(tShape.mPoints, tShape.mTriangles))
        {
            log("region intersects itself: only its outline will be drawn");
        }
    }
    tShape.mNumber = ++counterFor(tShape.mKind);
    mShapes.push_back(tShape);
    log("{}{} finished ({} vertices)", tShape.mKind == ShapeKind::Route ? "R" : "A", tShape.mNumber,
        static_cast<int>(tShape.mPoints.size()));
}

void Drawings::undo()
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
        int& tCounter = counterFor(tLast.mKind);
        if (tCounter == tLast.mNumber)
        {
            --tCounter;
        }
        mShapes.pop_back();
    }
}

void Drawings::clear()
{
    mShapes.clear();
    mCurrent = Shape();
    mHasCurrent = false;
    mCounters[0] = 0;
    mCounters[1] = 0;
    mCounters[2] = 0;
    log("all drawings cleared");
}

bool Drawings::remove(ShapeKind pKind, int pNumber)
{
    for (auto tIterator = mShapes.begin(); tIterator != mShapes.end(); ++tIterator)
    {
        if (tIterator->mKind == pKind && tIterator->mNumber == pNumber)
        {
            mShapes.erase(tIterator);
            return true;
        }
    }
    return false;
}

const std::vector<Drawings::Shape>& Drawings::shapes() const
{
    return mShapes;
}

const Drawings::Shape* Drawings::current() const
{
    return mHasCurrent ? &mCurrent : nullptr;
}

int Drawings::shapeCount() const
{
    return static_cast<int>(mShapes.size());
}

int& Drawings::counterFor(ShapeKind pKind)
{
    return mCounters[static_cast<int>(pKind)];
}

std::string Drawings::labelOf(const Shape& pShape)
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

std::string Drawings::serialize() const
{
    std::ostringstream tStream;
    tStream.imbue(std::locale::classic());
    tStream.precision(12);
    tStream << "# Prime XMap Plugin drawings: P = point, R = route, A = area (region)."
               " <type> <number> <lat> <lon> ...\n";
    for (const Shape& tShape : mShapes)
    {
        tStream << labelOf(tShape).substr(0, 1) << ' ' << tShape.mNumber;
        for (const LatLon& tPoint : tShape.mPoints)
        {
            tStream << ' ' << tPoint.mLat << ' ' << tPoint.mLon;
        }
        tStream << '\n';
    }
    return tStream.str();
}

int Drawings::deserialize(const std::string& pText)
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
            triangulatePolygon(tShape.mPoints, tShape.mTriangles);
        }
        int& tCounter = counterFor(tShape.mKind);
        if (tShape.mNumber > tCounter)
        {
            tCounter = tShape.mNumber;
        }
        mShapes.push_back(tShape);
    }
    return shapeCount();
}
