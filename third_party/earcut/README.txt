earcut.hpp - delikli poligon ucgenleme (dis kutuphane, DEGISTIRILMEDEN kullaniliyor)

Kaynak    : https://github.com/mapbox/earcut.hpp
Surum     : v2.2.4 (etiket), dosya include/mapbox/earcut.hpp
Lisans    : ISC (LICENSE dosyasi), Copyright (c) 2015, Mapbox
SHA-256   : earcut.hpp 3b838bc3f643d34ced850b6d53b19e2d9abdf6c2c067425b02868d131379b3cc
            LICENSE    828f2aed51b6526881a236758ec9b08cd69928fbfc70346d9d44a0b3a3444fe1
Indirildi : 2026-11-04

Ne icin kullaniliyor:
  apt.dat'taki apron / taksi yolu yuzeyleri (110) dis sinir + delikli
  poligonlardir. Ekran karti sadece ucgen doldurabildigi icin bu poligonlar
  ucgenlere bolunur (src/data/AirportShapes.cpp, Shapes::Triangulate).
  Earcut gercek veride kendini kesen sinirlarla da calisir.

Kullanim (tek baslik dosyasi, derleme gerektirmez):
  #include "mapbox/earcut.hpp"
  std::vector<uint32_t> tTriangles = mapbox::earcut<uint32_t>(tRings);
