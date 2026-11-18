# Prime XMap - Durum Raporu ve Ajan Rehberi (17-18 Kasim 2026)

> BU DEPO RESMI DEGIL. Bu, bilgisayarlar arasi aktarim reposudur (GitHub:
> efeegrkm/X-Plane-Map-Plugin). Resmi depolar SADECE Bitbucket'tir:
> - https://gobitbucket.havelsan.com.tr/scm/eps/prime-map-api.git
> - https://gobitbucket.havelsan.com.tr/scm/eps/prime-map-plugin.git
> GitHub adresi hicbir dokumana/koda yazilmaz (3. taraf kutuphane linkleri haric:
> github.com/mapbox/earcut.hpp, github.com/nothings/stb serbesttir).
> AGENTS.md yalnizca BU aktarim reposunda yasar; Bitbucket'e giden depolarda AGENTS.md,
> yapay zeka araci izleri, AI kelimesi OLMAZ (taramalar sifir olmali).

## 1. Anlik durum (17.11.2026, v0.26.0)

- Dort agac (a22=test, p22=plugin, t22=api-test, e22=texture-host) /W4 ile temiz derleniyor.
- map_tests: 30.682 kontrol / 0 hata.
- X-Plane 11'e deploy edildi ve hash dogrulandi: prime-map-plugin\64 ve plugin-api-test\64
  (prime_map.dll + win.xpl, build ciktilariyla birebir ayni).
- #pragma temizligi TAMAM: 90 adet #pragma once -> include guard; C4251 yerine /wd4251 tum
  hedeflerde; stb_truetype implementasyonu src/gfx/StbTruetypeImpl.cpp'de /W0 ile izole.
  Kodda 0 #pragma kaldi (TerrainVectors.inl dahil).
- Yorum inceltme tamam: dosya baslari 2-4 satir; anlatisal/versiyon-gecmisi yorumlar silindi.
  tests.cpp ve TerrainStore/Raster.cpp bilerek dokunulmadi.
- Repo hijyeni: AGENTS.md + docs\tools gercek reposlardan cikarildi (asistan yerelinde duruyor);
  tum dokumanlardan GitHub referansi ve AI izleri temizlendi; taramalar 0 eslesme.
- Yeni: docs/LOCAL_API_REHBERI.md eklendi (kisisal calisma notu, prime_map_api .gitignore'da;
  bu aktarim reposunda zorunlu olarak -f ile eklendi ki evde okunabilsin).

### 18 Kasim sabah eki (dokuman geri getirme + kalinti temizligi)

- SORUN VE KOK NEDEN: 17.11 aksami calisan reposdaki ESKI dokumanlar (PrimeMap_API=0.23
  icerigi, MIMARI=0.19) aktarim reposuna /MIR ile ezilip pushlandi; 60f0b86'daki dogru 0.26
  PDF'leri ezildi, eski HTML'ler de commitlendi. 0.26 HTML'leri hicbir committe yoktu (ic
  .gitignore engellemisti). Ders: /MIR oncesi dokuman surum kontrolu yapilacakti.
- DUZELTILDI (18.11): PrimeMap_API.html 0.26 icerigiyle yeniden URETILDI (build_api_doc.py
  tasınabilir Python 3.12.8 ile calistirildi; 26 sayfa PDF). MIMARI.html 0.26 PDF'i kaynak
  alinarak yeniden yazildi (21 sayfa; 14b klasor/paylasim matrisi EKLENDI; AI/AGENTS cumlesi
  ve docs/tools atifi cikarildi). PDF'ler HTML'den yeniden basildi; taramalar temiz.
- Kalintilar (18.11): MultiXMapSelfTest.cpp "prime_map.ini" metinleri -> prime_map.json;
  Settings.cpp kullanilmayan trim() silindi; XMapData.h InitConfig yorumu, XMapWindow.cpp
  XMapInstance yorumu, PreviewMain.cpp terrain_colors.ini metni guncellendi; README + SRS
  NF-MNT-02'deki docs/tools/build_api_doc.py atiflari kaldirildi; SRS SRC-04 "AI" kelimesi
  "X-Plane trafik uçakları ve çok oyunculu uçaklar" oldu; CHANGELOG'a 0.25 / 0.26 / 0.26.1
  eklendi. Dort agac /W4 temiz, map_tests 30.682 / 0.
- Tarama sonucu (18.11): AI kelimesi/arac izleri 0 (GECIS_0.22_RAPOR.md ic notu haric,
  gitignore'da); GitHub adresi yalniz 3. taraf (mapbox/earcut, nothings/stb); eski adlar
  yalnizca gecis tablosu ve gecmis raporlarinda (ZEMIN_HIZ_RAPORU 0.23 donemine ait,
  gitignore'da).

## 2. Onbellek mimarisi - bugunku arastirmanin ozeti (ONEM: yuksek)

Kullanicinin gozlemi: resmi eklentinin map1+map2 arasi cache paylasimi anlik; UI test
eklentisinin haritasiyla paylasim yok. Arastirma sonucu SIKINTI YOK, tasarim boyle:

- Paylasim birimi YONETICIDIR, surum degil. Ayni XMapManager'in haritalari (map1+map2) ayni
  TerrainStore/isci/bellek havuzunu kullanir -> anlik. Farkli manager ayri dunya (factory
  her cagrida yeni yonetici; singleton yok).
- Resmi eklenti dizin set etmiyor -> varsayilan %LOCALAPPDATA%\prime\prime-xmap.
  UI test (v0.26) initLibrary(prefix, findPluginDirectory()) -> kendi eklenti klasoru.
  Disk kaniti: LOCALAPPDATA'da 98 MB cache + maps\main + maps\second; plugin-api-test
  klasorunde maps\ui_test_texture (cache kullanicinin test icin silmesi).
- Her iki prime_map.dll v0.26, hash dogrulanmis; surum uyumsuzluğu YOK.
- Disk karo onbellegi AYAR-BAGIMSIZ: icerik DSF listesinden uretilen anahtarla dogrulanir;
  mod/stil/renk/blend goruntu uretiminde (planForImage) paletten uygulanir. Yani farkli
  harita ayarlari (IFR/Scenery/Dark, renkler) ORTAK cache'i ENGELLEMEZ.
- Yarismalar kendini onarir: okuma anahtar dogrulamali (TerrainRaster.cpp:1046), icerik
  deterministik, Windows acik dosyayi silmez. CacheTrim klasorun tamaminu kendi butcesiyle
  oIcer -> rekabet zararsiz ama isabet dusurebilir.
- Canli senkron (bir haritada yapilanin digerine aninda islenmesi) dizin paylasimiyla OLMAZ;
  ayarlar initialize'da bir kez okunur. Senkron host'un isidir (her iki haritaya ayni setX
  cagrilari). Bu bir API eksigi degil, tasarim.
- setCacheDirectory ayri bir API (XMapManager.h:33); verilmezse <veri>/cache/ varsayilan.
  Coklu uygulama senaryosunda altin kurulum: setDataDirectory uygulama basina FARKLI +
  setCacheDirectory IKISINE AYNI -> ayarlar izole, karolar ortak.

## 3. Bulunan eksikler ve onayli is kuyrugu

Onayli duzeltmeler (kod):
1. LIF-08: kapanista no-op korumalari.
2. H-01: TerrainBuiltUp rengi.
3. H-04: dalgalan (flaky) 1 sn'lik test beklemeleri.
4. YENI (onaylandi): ayni surecte ikinci bir yonetici ayni veri/obellek klasorune cozulurse
   Log'a SADE tek satir uyari (initialize'da surec-ici statik kayitla tespit). Not: surecler
   arasi tespit icin kilit dosyasi gerekecek; gereksiz agir, YAPMA.

Dokuman eksikleri:
5. [ACIK - onay bekliyor] SRS: "farkli surecler setCacheDirectory ile karo onbellegini
   paylasabilmeli (ayarlar izole)" gereksinimi + kabul testi; MUL-04 notuna "canli senkron
   host sorumlulugudur".
6. [TAMAM 18.11] PrimeMap_API.html 0.26 icerigiyle yeniden uretildi (eski API adlari yok;
   0.23->0.26 gecis tablosu bilincli olarak duruyor) + PDF (26 sayfa).
7. [TAMAM 18.11] MIMARI.html 0.26'ya cekildi (24 sayfalik PDF metni kaynak alindi) + 14b
   bolumu: klasor semasi ve "kim neyi paylasir" matrisi. PDF yeniden uretildi.

## 4. Git kurallari (BAGLAYICI)

- Calisan depolar (prime_map_api, prime_map_plugin, plugin-api-test) AJAN ICIN SALT OKUNUR:
  add/commit/reset/stage YASAK. Kullanici gorup kendisi pushlar (Bitbucket).
- plugin-api-test git reposu DEGIL (klasor duz).
- prime_map_plugin\eski = kullanicinin kendi not dosyasi, DOKUNMA.
- docs/SRS.md gercek repoda UNTRACKED - kullanici kendi git add'ini yapacak (pushlanacak 3
  dokuman: SRS, PrimeMap_API, MIMARI).
- docs/GECIS_0.22_RAPOR.md ve docs/ZEMIN_HIZ_RAPORU.md .gitignore'da ve untracked - Bitbucket'a
  ASLA gitmez. (Bu aktarim reposunda bilincli olarak tasinirlar - aktarim istisnasidir.)
- prime_map_api\docs\LOCAL_API_REHBERI.md kisisal dosya, .gitignore'da; bu aktarim reposunda
  force-eklendi (kullanici evde okuyacak).
- GitHub adresi dokumanlara/koda yazilmaz; bu dosya ve bu repo istisna (sadece aktarim).

## 5. Ortam notlari (ev makinesi icin)

- python: WindowsApps stub'i calismiyor. COZUM (18.11, ofis makinesi): tasinabilir Python
  3.12.8 %TEMP%\pyport'a indirildi; build_api_doc.py artik calistirilabilir. Ev makinesinde
  gercek Python kurulmasi ya da ayni yontem onerilir. (pypdf de PDF metin cikarmada
  kullanildi: %TEMP%\pylib)
- PDF uretimi: chrome --headless=new --disable-gpu --no-pdf-header-footer
  --print-to-pdf=<cikti.pdf> file:///<girdi.html>
- X-Plane dosya kopyalarken X-Plane KAPALI olmali; deploy build ciktilarindan yapilir.
- Kod stil: #pragma YASAK (guard'lar), Turkce ASCII yorumlar, /W4 + /wd4251, map_tests
  30.682/0 korunacak.
- X-Plane kurulumu bu makinede C:\X-Plane 11.

## 6. 1.0.0'a kalanlar

- Yukaridaki 4 kod duzeltmesi + dokuman eksikleri (3-7. maddeler).
- SRS "Taslak" -> "Yayin" ve 1.0.0 kabul testleri (SRS kabul testi 5: texture_host + test
  eklentisi ayni anda, ayri veri klasorleri).
- texture-host'un kurulup kurulmamasi karar beklemede (kullanici karari).
- Kullanici tum degisiklikleri kendisi inceleyip Bitbucket'a pushlar; ajan push ATMAMALI
  (istisna: yalnizca bu aktarim reposu).

## 7. Acik konular (UYGULAMA YAPILMADI, karar/planlama bekliyor)

- Zemin yukleme hizi: ZEMIN_HIZ_RAPORU.md onerileri HICBIRI uygulanmadi - tek isci thread'i,
  gorsel karolar icin disk onbellegi yok (sadece raster/rakim/vektor), rakim sikistirmasi
  karo uretiminin icinde. Rapordaki oneriler (ikinci isci, gorsel onbellek, kodlama
  ayrimi) tartisilmali; 13c'deki "ikinci isci yok" karariyla celisir, once karar.
- SRS coklu-surec onbellek gereksinimi (yukarida 5. madde) onay bekliyor.
- texture-host dagitimi karari kullanici bekleniyor (6. bolum).
