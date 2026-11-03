# Harita Plugin 0.3 — X-Plane 11

X-Plane'in kendi haritasının üstüne çizim yapan, haritaya **tıklayarak** nokta,
rota ve bölge işaretlemeyi deneyen AR-GE plugini. Harita, havalimanları ve
navaid'ler X-Plane'den hazır geliyor; dış veri (Kaşif, OSM) kullanılmıyor.

## Kurulum (hazır derlenmiş)

1. `HaritaPlugin` klasörünü olduğu gibi şuraya kopyala:
   `X-Plane 11/Resources/plugins/HaritaPlugin/`
   İçinde şunlar olmalı:
   ```
   HaritaPlugin/
     harita_ikon.png
     64/win.xpl
   ```
2. X-Plane'i aç, bir uçuş başlat.
3. `X-Plane 11/Log.txt` içinde `[HaritaPlugin] yuklendi (surum 0.3)` satırını ara.

Gereksinim: X-Plane 11.20 veya üstü, Windows 64-bit.

## Kaynaktan derleme (Visual Studio 2022)

1. X-Plane SDK'yı indir: https://developer.x-plane.com/sdk/plugin-sdk-downloads/
   Zip'i örneğin `C:/dev/XPSDK` klasörüne aç (içinde `CHeaders` ve `Libraries` olmalı).
2. Bu klasörde "Developer PowerShell for VS 2022" aç:
   ```
   cmake -B build -A x64 -DXPSDK=C:/dev/XPSDK
   cmake --build build --config Release
   ```
3. Çıktı: `build/HaritaPlugin/` klasörü (win.xpl + ikon). Kurulumdaki gibi kopyala.

X-Plane'siz testler: `cmake -B build -A x64 -DXPSDK=... -DHARITA_TESTS=ON`, derleyince
`build/Release/harita_tests.exe` çalıştır.

## Kullanım

- Haritayı aç: X-Plane'in harita tuşu veya **Plugins → Harita Plugin → Haritayi ac / kapat**.
- Harita açılınca haritanın yanında **buton paneli** belirir.
  Paneli görmüyorsan haritanın katman listesinde **"Harita Plugin"** kutusunun işaretli
  olduğundan emin ol.

| Buton | Ne yapar |
|---|---|
| Gezin | İşaretleme kapalı, harita normal kullanılır (kaydır, zoom) |
| Nokta | Her sol tık bir nokta (P1, P2...) |
| Rota | Sol tıklar çizgi oluşturur; sağ tık veya **Bitir** ile biter (R1, uzunluk NM) |
| Bolge | Sol tıklar köşe ekler; sağ tık veya **Bitir** ile kapanır ve dolar (B1) |
| Bitir | Yarım rota/bölgeyi bitirir |
| Geri al | Son köşeyi, yoksa son şekli siler |
| Temizle | Tüm çizimleri siler |

- İşaretleme modundayken haritanın kenarında **mavi bir çerçeve** görünür. Bu çerçeve
  bizim görünmez tıklama penceremizin sınırıdır; haritanın kenarıyla çakışmalıdır.
- İşaretleme modunda **fare tekerleği** ile zoom yapılabilir. **Kaydırmak** için Gezin'e geç.
- İmlecin altında **beyaz artı** ve **lat/lon** yazısı görünür.
- Magenta çizgi: X-Plane'in nav verisinden bulunan LTFM → LTAC demo rotası.
- Sarı çizgi: uçağın izi.

Klavye yedeği (Settings → Keyboard, `ozdev/harita` ile ara):
`imlece_nokta` (imlecin altına nokta/köşe), `bitir`, `gezin`, `teshis`.

## Test listesi

Her maddede ne gördüğünü not al. Sorun olursa **Teşhis modunu** açıp
(Plugins → Harita Plugin → Teshis modu) aynı şeyi tekrar yap ve `Log.txt`'deki
`TESHIS` satırlarını gönder.

| # | Ne yap | Beklenen |
|---|---|---|
| 1 | Haritayı aç | Panel haritanın yanında belirir. Log'da `harita katmani olusturuldu` |
| 2 | LTFM–LTAC bölgesine git | Magenta demo rota ve etiketi. Log'da `nav verisinden bulundu` |
| 3 | Zoom ve kaydır | Demo rota haritayla birlikte hareket eder, kenarlardan taşmaz |
| 4 | Haritayı ekranda taşı, boyutunu değiştir | Panel haritanın yanında kalır |
| 5 | **Nokta**'ya bas | Haritanın kenarında mavi çerçeve. **Çerçeve haritanın kenarıyla çakışıyor mu?** |
| 6 | Fareyi haritada gezdir | **Beyaz artı imlecin tam ucunda mı?** Kayıyorsa ne kadar, hangi yöne |
| 7 | Birkaç yere tıkla | Tıkladığın yerde P1, P2... ve mavi ikon |
| 8 | Bilinen bir pistin ucuna tıkla | Panel/etiketteki lat/lon gerçek koordinata yakın mı |
| 9 | Zoom yap (tekerlek) | Noktalar yerinde kalır |
| 10 | **Rota**: 3-4 tık, sağ tık | Mavi çizgi, R1 ve uzunluk |
| 11 | **Bolge**: 4-5 tık, Bitir | Yeşil dolu alan, B1. İçbükey (L şekli) de dene |
| 12 | **Geri al**, **Temizle** | Çizimler silinir |
| 13 | Nokta modunda haritanın üst çubuğuna tıkla, sonra tekrar haritaya | Tıklama çalışmaya devam ediyor mu |
| 14 | Haritayı kapat / aç | Panel ve çerçeve kaybolur / geri gelir |
| 15 | Uçuşu duraklat (P), 5-7'yi tekrarla | Aynı şekilde çalışmalı |
| 16 | Settings → Graphics → UI ölçek %100 ve %150 | 5-7'yi her ikisinde tekrarla |
| 17 | Haritayı pop-out yap (ayrı pencere) | **Desteklenmiyor**; ne olduğunu not al |
| 18 | X-Plane'in kendi çizdikleri | Havalimanı, pist, VOR/NDB, fix, hava yolu (IFR), FMS rotası, yaklaşma prosedürleri hangileri görünüyor |

## Nasıl çalışıyor (kısaca)

```
tıklama (boxel) ──► piksel ──► harita birimi ──► lat/lon ──► çizim listesi
          ScreenState    OpenGL matrisi    XPLMMapUnproject
```

- Haritaya kendi katmanımızı ekliyoruz (`XPLMCreateMapLayer`). X-Plane her karede
  katmanı çizerken o anki projeksiyonu veriyor; çizimler lat/lon olarak saklandığı
  için zoom/pan'de kendiliğinden doğru yerde kalıyor.
- Katman çizilirken OpenGL matrisini ve haritanın görünen alanını kaydediyoruz.
  Bunlardan haritanın **ekrandaki dikdörtgenini** hesaplıyoruz (X-Plane bunu vermiyor).
- Bu dikdörtgene her karede görünmez bir XPLM penceresi oturtuyoruz. Normal pencereler
  tıklama alabildiği için tıklamayı o yakalıyor.
- Boxel ↔ piksel dönüşümünü kendi pencerelerimiz çizilirken okuyoruz; UI ölçek bu
  sayede kendiliğinden hesaba katılıyor.

## Dosyalar

| Dosya | Görev |
|---|---|
| `src/Plugin.cpp` | Giriş noktaları, menü, komutlar, her kare çalışan döngü |
| `src/MapLayer.cpp` | X-Plane haritasına eklenen katman: çizim, ikon, etiket |
| `src/Overlay.cpp` | Tıklama penceresi ve paneli haritayla senkron tutar |
| `src/ClickWindow.cpp` | Haritanın üstündeki görünmez tıklama penceresi |
| `src/ButtonPanel.cpp` | Buton paneli |
| `src/Drawings.cpp` | Nokta, rota, bölge verisi |
| `src/ScreenMath.cpp` | Koordinat dönüşüm matematiği |
| `src/ScreenState.cpp` | Boxel ↔ piksel dönüşümünün yakalanması |
| `src/Geo.cpp` | Mesafe, büyük daire, bölgeyi üçgenlere bölme |
| `tests/tests.cpp` | X-Plane'siz testler |

## Bilinen sınırlar

- **Pop-out harita desteklenmiyor**: harita ayrı işletim sistemi penceresine alınırsa
  bizim pencerelerimiz ana pencerede kalır.
- Katman listesinde "Harita Plugin" kapatılırsa panel de gizlenir.
- Çizimler hafızada; X-Plane kapanınca silinir (kaydetme sonraki adım).
- Doğrulanmamış varsayımlar: X-Plane'in haritayı ana pencereye (ekran dışı bir
  tampona değil) çizmesi; ikon yolunun X-Plane klasörüne göre verilmesi. Teşhis
  modu ilkini, ikonların görünüp görünmemesi ikincisini gösterecek.
