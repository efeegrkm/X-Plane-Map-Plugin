// Drawings.h
// Kullanicinin haritaya cizdikleri: noktalar, rotalar, bolgeler.
// Hepsi lat/lon olarak saklanir; her karede harita koordinatina cevrilip
// cizilir, bu yuzden zoom/pan'de kendiliginden dogru yerde kalir.
//
// X-Plane'e bagimli DEGIL (sadece Log kullanir).
#pragma once

#include <vector>

#include "Geo.h"

namespace Drawings {

// Isaretleme modu: haritaya tiklayinca ne olacagi
enum class Mode {
    Pan,     // "Gezin": isaretleme kapali, harita normal kullanilir
    Point,   // her tiklama tek bir nokta
    Route,   // tiklamalar acik bir cizgi (rota) olusturur
    Region,  // tiklamalar kapali bir alan (bolge) olusturur
};

enum class ShapeKind { Point, Route, Region };

struct Shape {
    ShapeKind kind = ShapeKind::Point;
    int number = 0;                // P1, R2, B3 gibi etiket numarasi
    std::vector<LatLon> pts;       // koseler
    std::vector<int> triangles;    // bolge dolgusu icin ucgen indeksleri (bossa sadece kenar cizilir)
};

Mode GetMode();
void SetMode(Mode mode);          // mod degisince yarim kalan cizim bitirilir
const char* ModeName(Mode mode);  // "Gezin", "Nokta", ...

// Mevcut moda gore tiklanan noktayi ekler
void AddVertex(const LatLon& p);

// Yarim kalan rota/bolgeyi bitirir (rota >= 2, bolge >= 3 nokta ister)
void Finish();

// Son koseyi, yarim cizim yoksa son sekli geri alir
void Undo();

// Her seyi siler
void Clear();

const std::vector<Shape>& Shapes();  // bitmis sekiller
const Shape* Current();              // yarim kalan cizim (yoksa nullptr)
int ShapeCount();

}  // namespace Drawings
