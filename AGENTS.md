# Prime XMap - Durum Raporu ve Ajan Rehberi (17 Kasim 2026 aksam)

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

Dokuman eksikleri (onaya sunuldu, kullanici onaylamadiysa sormadan kod yazma):
5. SRS: "farkli surecler setCacheDirectory ile karo onbellegini paylasabilmeli (ayarlar
   izole)" gereksinimi + kabul testi; MUL-04 notuna "canli senkron host sorumlulugudur".
6. PrimeMap_API.html'in klasor/onbellek bolumu ESKI API adlariyla yazilmis (InitConfig,
   prime_map.ini, GetDataDirectory) - kaynakta YOK. Guncel: XMapManager::setCacheDirectory,
   prime_map.json. DUZELTILMELI + PDF yeniden uretim.
7. MIMARI.html: klasor semasi (prime_map.json / maps\<ad>\ / cache\) ve "kim neyi paylasir"
   matrisi eklenmeli.

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

- python: WindowsApps stub'i calismiyor ("Python bulunamadi") -> build_api_doc.py
  CALISTIRILAMAZ; HTML duzenlemeleri dogrudan yapilir.
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
