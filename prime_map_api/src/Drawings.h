// Kullanicinin haritaya cizdikleri: noktalar, rotalar, bolgeler (lat/lon olarak saklanir).
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
        int mNumber = 0;               // P1, R2, A3 etiket numarasi
        std::vector<LatLon> mPoints;   // koseler
        std::vector<int> mTriangles;   // bolge dolgusu ucgen indeksleri (bossa sadece kenar)
    };

    Mode getMode() const;
    void setMode(Mode pMode);              // mod degisince yarim cizim bitirilir
    static const char* modeName(Mode pMode);

    void addVertex(const LatLon& pPoint);

    void finish();  // rota >= 2, bolge >= 3 nokta ister

    void undo();

    void clear();

    // (tur + numara); bulunamazsa false
    bool remove(ShapeKind pKind, int pNumber);

    const std::vector<Shape>& shapes() const;
    const Shape* current() const;
    int shapeCount() const;

    // Etiket: "P1", "R2", "A3" (A = area: bolge)
    static std::string labelOf(const Shape& pShape);

    // Kayit bicimi (yarim cizim kaydedilmez):
    //   P <no> <enlem> <boylam>
    //   R <no> <enlem> <boylam> <enlem> <boylam> ...
    //   A <no> <enlem> <boylam> ...
    std::string serialize() const;
    // okunamayan satirlari atlar; okunan sekil sayisini doner
    int deserialize(const std::string& pText);

private:
    int& counterFor(ShapeKind pKind);

    Mode mMode = Mode::Pan;
    std::vector<Shape> mShapes;
    Shape mCurrent;
    bool mHasCurrent = false;
    int mCounters[3] = {0, 0, 0};
};
#endif  // PRIME_MAP_SRC_DRAWINGS_H
