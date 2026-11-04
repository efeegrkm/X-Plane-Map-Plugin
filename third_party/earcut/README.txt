earcut.hpp - delikli poligon ucgenleme (dis kutuphane, TEK BIR YAMA ile)

Kaynak    : https://github.com/mapbox/earcut.hpp
Surum     : v2.2.4 (etiket), dosya include/mapbox/earcut.hpp
Lisans    : ISC (LICENSE dosyasi), Copyright (c) 2015, Mapbox
SHA-256   : earcut.hpp 3b838bc3f643d34ced850b6d53b19e2d9abdf6c2c067425b02868d131379b3cc (yamadan ONCEKI orijinal)
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

Yama (2026-11-04, MapPlugin):
  Degisen tek yer: Earcut<N>::isValidDiagonal (dosyada "MapPlugin patch" notu).
  Kosullar orijinalin AYNISI, sadece degerlendirme sirasi degisti: once O(1)
  gorunurluk testleri, en son tum kenarlari tarayan intersectsPolygon (O(n)).
  Sonuc (uretilen ucgenler) ayni; sadece hiz degisir.
  Neden: splitEarcut() her kose ciftini dener; orijinal sirada her cift icin
  O(n) tarama yapiliyordu. Gercek veride (Global Airports, bu PC):
    en yavas havalimani WAHI  2591 ms -> ~480 ms
    YSSY                      543 ms -> ~200 ms
    tum 37 bin havalimani     38.5 s -> 21.7 s (ayni ucgen sayilari)
  Yeni earcut surumune gecilirse bu yama tekrar uygulanmali (ya da upstream'e onerilebilir).
