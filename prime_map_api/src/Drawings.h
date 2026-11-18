// Kullanici cizimleri: noktalar, rotalar ve bolgeler; duzenleme, geri alma ve dosya bicimi.
#ifndef PRIME_MAP_SRC_DRAWINGS_H
#define PRIME_MAP_SRC_DRAWINGS_H

#include <string>
#include <vector>

#include "Geo.h"

class Drawings
{
public:
    enum class Mode
    {
        Pan,
        Point,
        Route,
        Region,
    };

    enum class ShapeKind
    {
        Point,
        Route,
        Region,
    };

    struct Shape
    {
        ShapeKind mKind = ShapeKind::Point;
        int mNumber = 0;
        std::vector<LatLon> mPoints;
        std::vector<int> mTriangles;
    };

    Mode getMode() const;
    void setMode(Mode pMode);
    static const char* modeName(Mode pMode);

    void addVertex(const LatLon& pPoint);

    void finish();

    void undo();

    void clear();

    bool remove(ShapeKind pKind, int pNumber);

    const std::vector<Shape>& shapes() const;
    const Shape* current() const;
    int shapeCount() const;

    static std::string labelOf(const Shape& pShape);

    std::string serialize() const;
    int deserialize(const std::string& pText);

private:
    int& counterFor(ShapeKind pKind);

    Mode mMode = Mode::Pan;
    std::vector<Shape> mShapes;
    Shape mCurrent;
    bool mHasCurrent = false;
    int mCounters[3] = {0, 0, 0};
};
#endif
