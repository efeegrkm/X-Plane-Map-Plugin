// Drawings.h
// Kullanicinin haritaya cizdikleri: noktalar, rotalar, bolgeler (cizim modeli).
// Hepsi lat/lon olarak saklanir; her karede ekrana cevrilip cizilir, bu
// yuzden zoom/pan'de kendiliginden dogru yerde kalir.
//
// X-Plane'e bagimli DEGIL (sadece Log kullanir). Haritaya cizimi
// DrawingLayer yapacak (asama 4).
#pragma once

#include <string>
#include <vector>

#include "Geo.h"

class Drawings
{
public:
    // Isaretleme modu: haritaya tiklayinca ne olacagi
    enum class Mode
    {
        Pan,     // "Pan" (gezin): isaretleme kapali, surukleme haritayi kaydirir
        Point,   // her tiklama tek bir nokta
        Route,   // tiklamalar acik bir cizgi (rota) olusturur
        Region,  // tiklamalar kapali bir alan (bolge) olusturur
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
        int mNumber = 0;               // P1, R2, A3 gibi etiket numarasi
        std::vector<LatLon> mPoints;   // koseler
        std::vector<int> mTriangles;   // bolge dolgusu icin ucgen indeksleri (bossa sadece kenar cizilir)
    };

    Mode GetMode() const;
    void SetMode(Mode pMode);              // mod degisince yarim kalan cizim bitirilir
    static const char* ModeName(Mode pMode);  // "Pan", "Point", ...

    // Mevcut moda gore tiklanan noktayi ekler
    void AddVertex(const LatLon& pPoint);

    // Yarim kalan rota/bolgeyi bitirir (rota >= 2, bolge >= 3 nokta ister)
    void Finish();

    // Son koseyi, yarim cizim yoksa son sekli geri alir
    void Undo();

    // Her seyi siler
    void Clear();

    const std::vector<Shape>& Shapes() const;  // bitmis sekiller
    const Shape* Current() const;              // yarim kalan cizim (yoksa nullptr)
    int ShapeCount() const;

    // Etiket: "P1", "R2", "A3" (A = area: bolge)
    static std::string LabelOf(const Shape& pShape);

    // Kayit: bitmis sekiller metin olarak (yarim kalan cizim kaydedilmez).
    //   P <no> <enlem> <boylam>
    //   R <no> <enlem> <boylam> <enlem> <boylam> ...
    //   A <no> <enlem> <boylam> ...
    std::string Serialize() const;
    // Onceki her seyi siler; okunamayan satirlari atlar. Okunan sekil sayisini doner.
    int Deserialize(const std::string& pText);

private:
    int& CounterFor(ShapeKind pKind);

    Mode mMode = Mode::Pan;
    std::vector<Shape> mShapes;
    Shape mCurrent;               // yarim kalan rota/bolge
    bool mHasCurrent = false;
    int mCounters[3] = {0, 0, 0};  // tur basina numara sayaci: nokta, rota, bolge
};
