# Prime XMap API — Yazılım Gereksinimleri Belirtimi (SRS)

| | |
|---|---|
| Belge | SRS-PXM-001 |
| Ürün | Prime XMap API (kütüphane `prime_map.dll`, CMake hedefi `prime-map-api`) |
| Belgenin anlattığı sürüm | 1.1.1 |
| Tarih | 2026-11-19 |
| Durum | Yayın 1.1.1: X-Plane kabul testleri tamamlandı |
| Yapı | ISO/IEC/IEEE 29148:2018 "Software Requirements Specification" içerik yapısı (IEEE 830'un yerini alan standart) |

## Değişiklik geçmişi

| Sürüm | Tarih | Değişiklik |
|---|---|---|
| 0.1 | 2026-11-17 | İlk taslak: 0.26.0 kodundan çıkarılan gereksinimler ve durumları |
| 0.2 | 2026-11-18 | 0.26.2: LIF-08 / NF-REL-01 (shutdown sonrası güvenli no-op), STY-06 (TerrainBuiltUp rengi zemine bağlandı), NF-REL-05 (koşul bazlı test beklemeleri), MUL-05 (ortak önbellek, izole ayarlar) eklendi; MUL-04'e canlı senkron notu |
| 0.3 | 2026-11-18 | 1.1.0: MODE-03 (varsayılan mod Dark), ZOOM-01 ve ZOOM-02 (yerleşik eşik tablosu, host aralığı, sert alt sınır), TER-05 (açık harita yokken ön yükleme), LIF-09 (sürüm denetimi), NF-PRF-06 (eşik maliyeti), NF-REL-06 (atomik yazım dayanıklılığı); kabul testleri 1–11 |
| 0.4 | 2026-11-18 | 1.1.0 düzeltmeleri: VOR, NDB, ILS, fix ve havalimanı sembolleri zoom tablosuna alındı; sert alt sınırlar katman başına ölçüldü (NF-PRF-06); haritasız ön yüklemenin uçak verisi başlatıldı (TER-05); STY-01 renk verilebilen katman sayısı 15 olarak düzeltildi; KIS-04 yorum kuralı güncellendi; kabul testleri 12–14 eklendi |
| 0.5 | 2026-11-19 | 1.1.1: ayar dosyasından yüklenen modun teması çizime uygulanıyor (panel-çizim tutarlılığı); texture-host örneğine cull face koruması; durum "Yayın" olarak güncellendi |

---

## 1. Giriş

### 1.1 Amaç

Bu belge Prime XMap API'nin gereksinimlerini tanımlar ve her gereksinim için **bugünkü durumu** (1.1.1) kaydeder. Belgenin
hedefi şudur:

> **Bir X-Plane eklentisi, API ile haritanın her öğesi üzerinde tam kontrole sahip olmalıdır:** haritayı istediği yere ve
> boyuta koymak, her katmanın ne zaman ve nasıl görüneceğini, her öğenin rengini ve stilini, verinin kaynağını ve kullanıcı
> etkileşimini belirlemek.

Belge hem hedefi (ne olmalı) hem durumu (ne var, ne eksik) gösterir; eksikler bölüm 5'teki yol haritasına bağlanır.

### 1.2 Kapsam

- **Kapsamda:** `prime_map_api` kütüphanesi ve genel API'si (`PrimeMap.h`, `src/api/*`), ürettiği / okuduğu dosyalar, X-Plane
  ile etkileşimi, örnek ve test eklentileri ile doğrulanma yolu.
- **Kapsam dışı:** resmî eklenti `prime_map_plugin`'in menü ve komut tasarımı (yalnızca API tüketicisi olarak anılır), X-Plane'in
  kendisi, dış harita servisleri (ürün bunları kullanmaz).

### 1.3 Tanımlar ve kısaltmalar

| Terim | Anlam |
|---|---|
| Host | API'yi kullanan X-Plane eklentisi |
| Yönetici (manager) | `prime::XMapManager`: ortak veri, önbellek ve haritaların sahibi |
| Harita | `prime::XMap`: tek bir haritanın her şeyi |
| Texture (doku) yolu | `WindowStyle::Texture`: harita kütüphanenin framebuffer'ında bir OpenGL dokusuna çizilir, host basar |
| LEGACY | Kodu korunan ama yeni kullanımda önerilmeyen, kaldırılması planlanan yol (`Standalone`, `Embedded`) |
| Boxel | X-Plane'in arayüz koordinat birimi (UI ölçeğinden bağımsız) |
| Katman / alt katman / grup | `XMapLayer` ağacındaki öğeler (56 öğe) |
| Mod | Hazır tema: Scenery, VFR, IFR Low, IFR High, Dark |
| DSF, apt.dat, earth_awy.dat | X-Plane'in sahne, havalimanı ve hava yolu veri dosyaları |
| IOS | Instructor Operating Station (eğitmen istasyonu) |
| COTS | Repoda tutulmayan üçüncü taraf bileşenler |

### 1.4 Gereksinim yazımı ve durum değerleri

- Her gereksinim tek bir doğrulanabilir ifadedir; "-meli / -malı" zorunluluk bildirir.
- **Öncelik:** Z (Zorunlu), Ö (Önemli), İ (İsteğe bağlı).
- **Durum (1.1.1):**

| Kod | Anlamı |
|---|---|
| **K** | Karşılanıyor ve doğrulandı (otomatik test ya da X-Plane'de gözlem) |
| **U** | Uygulandı; X-Plane'de kapsamlı olarak doğrulanmadı |
| **KS** | Kısmen karşılanıyor (sınır "Not" sütununda) |
| **Y** | Karşılanmıyor (API'de yok) |
| **H** | Bilinen hata: API var ama davranış yanlış |

- **Doğrulama:** T (otomatik test, `map_tests`), X (X-Plane içinde test / gözlem), S (X-Plane içi öz-test, `MultiXMapSelfTest`),
  İ (kod / belge incelemesi), A (analiz / ölçüm).

### 1.5 Referanslar

| Belge | Ne |
|---|---|
| `docs/PrimeMap_API.pdf` | Kullanım kılavuzu ve API referansı (1.1.1) |
| `docs/MIMARI.pdf` | Mimari |
| `README.md` | Derleme, testler, araçlar |
| `examples/texture_host/` | En küçük tam host |
| `../plugin-api-test/` | Test eklentisi ve X-Plane içi öz-test |
| X-Plane SDK 3.0 (XPLM301) | developer.x-plane.com/sdk |
| ISO/IEC/IEEE 29148:2018 | Gereksinim mühendisliği, SRS içeriği |

---

## 2. Genel tanım

### 2.1 Ürün perspektifi

Prime XMap, X-Plane 11 sürecinin içinde çalışan bir SHARED kütüphanedir. Host eklenti kütüphaneyi kendi CMake oturumunda derler,
`prime_map.dll`'yi kendi `win.xpl` dosyasının yanına koyar ve yalnızca genel API ile kullanır.

```
X-Plane 11 süreci
 ├─ host eklenti (win.xpl) ── genel API (PrimeMap.h) ──► prime_map.dll
 │     kendi penceresi / paneli                          ├─ XMapManager: nav, havalimanı, hava yolu, zemin işçisi, önbellek
 │     renderTexture → dokuyu basar                      ├─ XMap × n: görünüm, mod, katmanlar, renkler, çizimler, dosyalar
 │     girdi → inject*                                   └─ arka plan thread'leri (veri yükleme, zemin üretimi)
 └─ X-Plane verisi: DSF, apt.dat, nav, earth_awy.dat, dataref'ler, FMS
```

### 2.2 Ürün işlevleri (özet)

Haritayı host'un istediği yere doku olarak çizmek; birden çok bağımsız harita; katman görünürlüğü, zoom aralığı ve renk kontrolü;
zemin görünümü; 5 hazır mod; uçak / rota (X-Plane ya da host); kullanıcı çizimleri; sorgu ve dönüşüm; ayarların kalıcılığı;
ortak önbellek ve önceden yükleme; haritanın kendi arayüzü (isteğe bağlı).

### 2.3 Kullanıcı sınıfları

| Sınıf | İhtiyaç |
|---|---|
| Host geliştirici (birincil) | Haritayı kendi eklentisine koymak ve tamamen API ile yönetmek |
| Son kullanıcı (pilot, eğitmen) | Haritayı host'un arayüzü ya da haritanın kendi arayüzüyle kullanmak |
| Kütüphane geliştiricisi | Kodu anlamak, genişletmek, test etmek |

### 2.4 İşletim ortamı

X-Plane 11 (11.50+), Windows x64, OpenGL 3.3+ (X-Plane 11 uyumluluk bağlamı), MSVC 2022 (C++20, `/MD`), X-Plane SDK 3.0.

### 2.5 Tasarım ve uygulama kısıtları

| Kimlik | Kısıt |
|---|---|
| KIS-01 | Veri yalnızca yerel X-Plane kurulumundan gelir; ağ erişimi yoktur. |
| KIS-02 | Bütün API çağrıları X-Plane ana thread'inden yapılır; arka plan thread'leri XPLM ve Log çağırmaz. |
| KIS-03 | Hata modeli istisnasızdır: geçersiz değer → işlem yok, tek satır Log uyarısı, `false` / boş dönüş. |
| KIS-04 | Kod standardı: önekler `m p t k g`, metotlar camelCase, türler PascalCase, süslü parantez alt satırda, satır ≤ 120, `/W4` uyarısız. Kaynak kodda yorum yalnızca başlık dosyalarının başında, dosya çiftinin görevini anlatan kısa bir açıklama olarak bulunur (Türkçe, ASCII). Arayüz metinleri ve log İngilizcedir. |
| KIS-05 | Çekirdek (veri, görünüm, katman kuralları) X-Plane'e bağımlı değildir ve X-Plane'siz testlerde çalışır; GL çağrıları yalnızca `src/gfx/`'tedir. |
| KIS-06 | Üçüncü taraf kodu repoda tutulmaz (COTS). |
| KIS-07 | Geliştirme aşamasında sürümler arası geriye uyumluluk sağlanmaz; ayar dosyaları yalnızca JSON'dur. |

### 2.6 Varsayımlar ve bağımlılıklar

- Host ve kütüphane aynı MSVC sürümü ve `/MD` ile derlenir; aynı süreçteki bütün host'lar aynı `prime_map.dll` sürümünü kullanır
  (Windows aynı adlı DLL'i bir kez yükler).
- Zemin yalnızca kurulu sahnede görünür; sahnesi kurulu olmayan karo deniz / veri yok olarak çizilir.

---

## 3. Belirli gereksinimler

### 3.1 Dış arayüz gereksinimleri

| Kimlik | Gereksinim | Ö | Durum | Doğ. | Not / kanıt |
|---|---|---|---|---|---|
| ARY-01 | API tek bir toplama başlıkla (`#include <prime_map_api/PrimeMap.h>`) ve tek CMake hedefiyle (`prime-map-api`) tüketilebilmeli. | Z | K | İ, X | `examples/texture_host`, `plugin-api-test`, `prime_map_plugin` |
| ARY-02 | Genel başlıklar host'un `/W4` derlemesinde uyarı üretmemeli. | Ö | K | İ | C4251 sınıf boyunca kapatıldı; host derlemeleri 0 uyarı |
| ARY-03 | Genel başlıklar kütüphanenin iç türlerini açığa çıkarmamalı (ABI / bağımlılık izolasyonu). | Ö | Y | İ | `XMap.h` iç başlıkları içeriyor; pimpl planlı |
| ARY-04 | Kütüphane X-Plane ile yalnızca XPLM SDK 3.0 üzerinden konuşmalı (pencere, çizim, dataref, FMS, nav, flight loop). | Z | K | İ | |
| ARY-05 | Texture yolunda çıktı, host'un kendi GL çiziminde kullanabileceği bir `GL_TEXTURE_2D` olmalı; kütüphane host'un GL durumunu (framebuffer, viewport, scissor, program, VAO, buffer, doku bağlama önbelleği) bozmamalı. | Z | U | İ, X | `Canvas::beginOffscreen/endOffscreen`, `BindingGuard`, `XPLMBindTexture2d` |
| ARY-06 | Dosya arayüzü: merkezi ayarlar `prime_map.json`, harita başına `maps/<ad>/{map_settings.json, terrain_colors.json, map_drawings.txt}`, önbellek `cache/`; JSON dışı biçim okunmamalı. | Z | K | T, İ | `Settings` yalnızca JSON (0.26) |
| ARY-07 | Log: bütün kütüphane mesajları Log.txt'ye host'un verdiği önekle yazılmalı; host isterse mesajları callback ile de almalı. | Ö | KS | İ | Önek süreç genelinde tek: birden çok host'ta son önek geçerli (bkz. NF-REL-04) |

### 3.2 İşlevsel gereksinimler

#### 3.2.1 Yaşam döngüsü ve yönetici (LIF)

| Kimlik | Gereksinim | Ö | Durum | Doğ. | Not / kanıt |
|---|---|---|---|---|---|
| LIF-01 | Host bir yönetici oluşturabilmeli ve bırakabilmeli (`createXMapManager` / `destroyXMapManager`); her çağrı bağımsız bir yönetici vermeli. | Z | K | X | |
| LIF-02 | Yönetici `initialize` öncesi yapılandırılabilmeli: veri klasörü, önbellek klasörü, Log öneki / callback, ilk önbellek sınırları. | Z | K | İ, X | |
| LIF-03 | Veri yüklemesi (nav, havalimanı indeksi, hava yolları, zemin indeksi) arka planda yapılmalı ve X-Plane'in karesini bekletmemeli. | Z | K | X | Nav dilimli (karede ≤ 4 ms) |
| LIF-04 | Yükleme durumu sorgulanabilmeli (`getStatus`: hazır bayrakları, bekleyen zemin, ön önbellek ilerlemesi, sınırlar). | Ö | K | İ | |
| LIF-05 | Harita nesnesi açılmadan oluşturulabilmeli; her ayar açılıştan önce ve sonra verilebilmeli. | Z | K | S | `MultiXMapSelfTest` |
| LIF-06 | Harita kapatılıp aynı durumla yeniden açılabilmeli; bırakılınca ayarları yazılmalı. | Z | K | S | |
| LIF-07 | Yol sorguları (veri / harita / ayar / önbellek klasörü) `initialize` öncesi de doğru dönmeli. | Ö | U | İ | 0.26 |
| LIF-08 | `shutdown` açık haritaları güvenle kapatmalı; sonrasında harita metotları çökmemeli (no-op). | Z | K | S | 0.26.2: tüm genel metotlar tek koruma mekanizmasında (`XMap::alive`): tek seferlik Log uyarısı + false / boş / varsayılan dönüş. `removeXMap` / `destroyXMapManager` sonrası işaretçi geçersizdir (nesne silinir); bu durum kod tarafında korunamaz, host işaretçiyi kullanmamalı |
| LIF-09 | Host'un derlendiği sürümle yüklü DLL'in sürümü uyuşmuyorsa fabrika tek satır WARNING yazmalı ve host sürümü kendisi denetleyebilmeli (`isLibraryVersionMatching()`); ready satırı yüklü DLL'in yolunu yazmalı. | Z | K | İ, S | 1.1.1: inline eşleştirme host tarafında derlenir; `libraryPath()`; öz-test `isLibraryVersionMatching` kontrolü |

#### 3.2.2 Yerleştirme ve çizim (EMB) — Texture hedef yolu

| Kimlik | Gereksinim | Ö | Durum | Doğ. | Not / kanıt |
|---|---|---|---|---|---|
| EMB-01 | Harita penceresiz bir OpenGL dokusuna çizilebilmeli ve host bu dokuyu istediği yere basabilmeli (`WindowStyle::Texture`, `renderTexture`, `getTextureId`). | Z | U | X | İlk X-Plane denemesi olumlu; kapsamlı test yok |
| EMB-02 | Harita alanının boyutu boxel cinsinden verilmeli; doku piksel yoğunluğu host'un UI ölçeğine göre otomatik ya da açıkça seçilebilmeli. | Z | U | İ | `setTextureSize(w, h, pixelsPerUnit)` |
| EMB-03 | Host penceresi taşınınca / boyutlanınca harita aynı karede yeni yer / boyutta görünmeli. | Z | U | X | Doku host'un çiziminde basıldığı için yapısal olarak sağlanır |
| EMB-04 | Doku yalnızca gerektiğinde yeniden çizilmeli (sahne, görünüm, boyut, girdi değişince ya da en çok `max_fps`); gerek yoksa çağrı ucuz olmalı. | Ö | U | İ | |
| EMB-05 | Bir host birden çok haritayı farklı dokular olarak aynı anda basabilmeli. | Ö | U | İ | |
| EMB-06 | Texture haritası pop-out edilmiş (ayrı işletim sistemi penceresine alınmış) host penceresinde de çizilebilmeli. | Ö | Y | İ | Ayrı GL bağlamında shader'lar paylaşılmıyor; bugün yalnızca LEGACY Standalone pop-out yapabiliyor |
| EMB-07 | Kütüphane, Texture üstüne kurulu hazır bir pencere yardımcısı sunmalı (host'un 100 satırlık pencere kodunu yazmasına gerek kalmasın). | İ | Y | — | LEGACY Standalone'un yerini alacak |
| EMB-08 | LEGACY yollar (`Standalone`, `Embedded`) Texture kapsamlı doğrulanana kadar çalışır kalmalı; sonra kaldırılmalı. | Ö | K | X | Resmî eklenti Standalone ile çalışıyor |

#### 3.2.3 Girdi (INP)

| Kimlik | Gereksinim | Ö | Durum | Doğ. | Not / kanıt |
|---|---|---|---|---|---|
| INP-01 | Host fare olaylarını (sol / sağ; bas, sürükle, bırak) doku koordinatıyla iletebilmeli; harita kendi arayüzünü ve kaydırmayı buna göre işlemeli. | Z | U | X | `injectMouse` |
| INP-02 | İmleç konumu iletilebilmeli (tooltip, vurgu). | Ö | U | İ | `injectMouseMove`; 0.25 s gelmezse imleç "dışarıda" |
| INP-03 | Tekerlek iletilebilmeli; imleç altındaki nokta yerinde kalarak zoom yapılmalı. | Z | U | X | `injectWheel` |
| INP-04 | Klavye: harita yazı beklediğinde (arama kutusu) host'a bildirmeli ve tuşları iletme yolu sunmalı. | Ö | U | İ | `wantsKeyboard`, `injectKey` |
| INP-05 | Haritanın kendi fare / tekerlek işlemesi kapatılabilmeli (kontrol tamamen host'ta). | Z | K | X | `setIsMouseInputEnabled(false)` |
| INP-06 | Host, kullanıcının haritada tıkladığı nesne / konum için olay (callback) alabilmeli. | Ö | Y | — | Bugün host `queryAtScreen` ile kendisi sorar |

#### 3.2.4 Görünüm (VIEW)

| Kimlik | Gereksinim | Ö | Durum | Doğ. | Not / kanıt |
|---|---|---|---|---|---|
| VIEW-01 | Merkez, zoom, zoom sınırları, zoom / kaydırma adımı API ile verilebilmeli ve okunabilmeli. | Z | K | T, S | |
| VIEW-02 | Görünüm bir coğrafi alana sığdırılabilmeli; görünen alan okunabilmeli. | Ö | K | İ | `fitBounds`, `getVisibleBounds` |
| VIEW-03 | Uçak takibi açılıp kapanabilmeli; elle kaydırınca kapanmalı. | Ö | K | X | |
| VIEW-04 | Havalimanı ICAO / ad ile aranabilmeli ve ona gidilebilmeli. | Ö | K | T, X | |
| VIEW-05 | Ekran (doku) ↔ coğrafi koordinat dönüşümü yapılabilmeli. | Z | K | T | |
| VIEW-06 | Harita kuzey yukarı dışında (iz yukarı / yön yukarı) döndürülebilmeli. | Ö | Y | — | |

#### 3.2.5 Mod ve tema (MODE)

| Kimlik | Gereksinim | Ö | Durum | Doğ. | Not / kanıt |
|---|---|---|---|---|---|
| MODE-01 | En az 5 hazır mod sunulmalı (Scenery, VFR, IFR Low, IFR High, Dark); her modun katman durumu ayrı saklanmalı. | Z | K | T, X | |
| MODE-03 | Yeni haritanın varsayılan modu Dark olmalı; host kendi modunu `XMapConfig::mMode` ile verebilmeli. | Z | K | T, S | 1.1.1: `XMapConfig::mMode` (Dark); öz-test "default mode is Dark"; `map_tests` Dark varsayılan testleri; ayarlardan yüklenen modun teması çizime uygulanır |
| MODE-02 | Host kendi temasını tanımlayabilmeli (bütün renkler, kalınlıklar, katman varsayılanları) ve adlandırıp seçebilmeli. | Ö | Y | — | Temalar `XMapTheme.cpp`'de sabit |

#### 3.2.6 Katman görünürlüğü (LAY)

| Kimlik | Gereksinim | Ö | Durum | Doğ. | Not / kanıt |
|---|---|---|---|---|---|
| LAY-01 | Her katman ve alt katman (56 öğe) mod başına açılıp kapanabilmeli; durum okunabilmeli (Off / On / Mixed). | Z | K | T, S | |
| LAY-02 | Katman ağacı (üst / alt, tür, ad, dosya anahtarı, mod başına varsayılan) sorgulanabilmeli. | Ö | K | T | |
| LAY-03 | Toplu işlemler: birden çok katmanı açma / kapama, yalnızca verilenleri gösterme, varsayılana dönme. | Ö | K | T | |
| LAY-04 | Host kendi katmanını (sembol, çizgi, alan, etiket) haritaya ekleyebilmeli; bu katman çizim sırasına, etiket çakışma önlemeye ve sorguya katılmalı. | Z | Y | — | Geçici çözüm: host `geoToScreen` ile dokunun üstüne kendisi çizer |

#### 3.2.7 Zoom aralığı (ZOOM)

| Kimlik | Gereksinim | Ö | Durum | Doğ. | Not / kanıt |
|---|---|---|---|---|---|
| ZOOM-01 | Her katman / alt katman için en küçük ve en büyük görünme zoom'u verilebilmeli, okunabilmeli, kaldırılabilmeli. | Z | K | T, S | 1.1.1: host aralığı yerleşik eşiğin **yerine geçer**; `getLayerZoomRange` etkili değeri verir; `clearLayerZoomRange` yerleşik eşiğe döner; `(0, 99)` host aralığını kaldırır; `min > max` reddedilir |
| ZOOM-02 | Yerleşik görünme eşiği API ile değiştirilebilmeli (aşağı çekme ölçülmüş sert alt sınıra kadar). | Z | K | T, S, A | Bütün katmanların eşikleri `engine/LayerTable.cpp` `kZoomTable` tablosundadır; VOR, NDB, ILS, fix ve havalimanı sembolleri dahil. Sert alt sınırın altındaki istek sınıra çekilir ve uyarı yazılır. Sert alt sınırlar Seattle ve New York'ta katman başına ölçüldü (NF-PRF-06). `getDefaultLayerZoomRange`, `getLayerHardMinZoom` |
| ZOOM-03 | Katman içi ayrıntı eşikleri (etiketler, oklar, işaretler, küçük / büyük havalimanı) ayrı ayarlanabilmeli. | Ö | Y | — | 1.1.1'de bilinçli kapsam dışı: ayrıntı eşikleri (etiket, ok, gösterge, lamba boyu) kodda kalır |

#### 3.2.8 Görsel stil (STY)

| Kimlik | Gereksinim | Ö | Durum | Doğ. | Not / kanıt |
|---|---|---|---|---|---|
| STY-01 | Her katmanın ana rengi API ile verilebilmeli, okunabilmeli, kaldırılabilmeli. | Z | KS | İ | 15 katmanda çalışır: ızgara, 6 yol türü, yerleşim alanı, VOR, NDB, ILS, fix, alçak ve yüksek hava yolu, pist kenarı |
| STY-02 | Havalimanı sembolleri (kuleli / kulesiz / askerî ...), pist dolgusu, yüzeyler, nesneler, uçak, rota, iz, çizimler renklendirilebilmeli. | Z | Y | — | |
| STY-03 | Etiket yazılarının rengi, gölge / hale rengi ve arka plan / su rengi (zeminsiz modlarda) verilebilmeli. | Ö | Y | — | |
| STY-04 | Çizgi kalınlığı, sembol boyu, yazı boyu katman başına verilebilmeli. | Ö | Y | — | |
| STY-05 | Renkler mod başına farklı verilebilmeli. | İ | Y | — | Bugün renk yazması bütün modlarda ortak |
| STY-06 | `TerrainBuiltUp` rengi görüntüye yansımalı. | Ö | K | T | 0.26.2: renk zemin paletine bağlandı (`TerrainPalette` yerleşim ezmesi); değişince ilgili dokular yeni palet numarasıyla yeniden üretilir; mod değişimi ve dosya yüklemesinde de geçerli. Scenery'de yerleşim sınıf renkleriyle, VFR/IFR'da bu renkle karışır |
| STY-07 | Yazı tipi seçilebilmeli (kütüphanenin atlası / X-Plane yazısı). | İ | KS | İ | Texture'da X-Plane yazısı etiketleri yanlış yere düşürebilir |

#### 3.2.9 Zemin (TER)

| Kimlik | Gereksinim | Ö | Durum | Doğ. | Not / kanıt |
|---|---|---|---|---|---|
| TER-01 | Zemin kurulu DSF'lerden, X-Plane'in sahne önceliğiyle üretilmeli ve sahne değişince yenilenmeli. | Z | K | T, X | |
| TER-02 | 15 arazi sınıfının rengi verilebilmeli ya da X-Plane'in kendi dokularından türetilmeli. | Z | K | T | `TerrainSettings::mClassColors`, `mUseXPlaneColors` |
| TER-03 | Kabartma, kıyı çizgisi, su şeridi, derinlik tonu, sınıf geçişi, sınır kıvrılması, VFR yükseklik bantları ve konturlar harita başına ayarlanabilmeli ve hemen uygulanmalı. | Z | K | T, X | |
| TER-04 | Zemin önbelleği önceden hazırlanabilmeli (görünen alan / verilen alan) ve iptal edilebilmeli. | Ö | K | X | Aynı anda tek alan |
| TER-05 | Açık harita yokken de uçak konumu çevresindeki karolar arka planda yüklenmeli; kapatılabilmeli. | Ö | U | X | Uçağın konumu ve 40 knot üstünde uçuş yönünde 15–120 NM; `setPrefetchTiles(0)` kapatır; tek satır Log |

#### 3.2.10 Veri kaynakları (SRC)

| Kimlik | Gereksinim | Ö | Durum | Doğ. | Not / kanıt |
|---|---|---|---|---|---|
| SRC-01 | Uçak konumu / yönü / hızı / irtifası X-Plane'den ya da host'tan alınabilmeli. | Z | K | X | `setAircraftSource`, `setAircraft` |
| SRC-02 | Rota X-Plane FMS'inden ya da host'tan alınabilmeli. | Z | K | İ | `setRouteSource`, `setRoute` |
| SRC-03 | Uçuş izi silinebilmeli. | İ | K | İ | |
| SRC-04 | Birden çok uçak (X-Plane trafik uçakları ve çok oyunculu uçaklar) gösterilebilmeli. | Ö | Y | — | |

#### 3.2.11 Çizimler (DRW)

| Kimlik | Gereksinim | Ö | Durum | Doğ. | Not / kanıt |
|---|---|---|---|---|---|
| DRW-01 | Nokta, rota ve bölge çizimleri API ve arayüzle eklenebilmeli, geri alınabilmeli, silinebilmeli, listelenebilmeli. | Z | K | T, S | |
| DRW-02 | Çizimler metin olarak dışa / içe aktarılabilmeli ve kalıcı olmalı. | Ö | K | T | |
| DRW-03 | Çizim rengi, kalınlığı ve etiketi şekil başına verilebilmeli. | Ö | Y | — | |

#### 3.2.12 Sorgu (QRY)

| Kimlik | Gereksinim | Ö | Durum | Doğ. | Not / kanıt |
|---|---|---|---|---|---|
| QRY-01 | Bir coğrafi nokta ya da ekran noktasındaki nesneler (katman + açıklama) sorgulanabilmeli. | Ö | K | İ | `queryAt`, `queryAtScreen` |
| QRY-02 | Sorgu sonucu yapısal veri vermeli (kimlik, tür, konum, frekans ...), yalnızca metin değil. | Ö | Y | — | Bugün `XMapObject{mLayer, mText}` |

#### 3.2.13 Kalıcılık (PER)

| Kimlik | Gereksinim | Ö | Durum | Doğ. | Not / kanıt |
|---|---|---|---|---|---|
| PER-01 | Harita ayarları harita başına adlandırılmış bir klasörde JSON olarak saklanmalı; kalıcılık kipi seçilebilmeli (Off / SaveOnly / LoadAndSave). | Z | K | S | |
| PER-02 | Ayarlar dosyasız olarak metin halinde dışa / içe aktarılabilmeli (host profilleri). | Ö | K | T | |
| PER-03 | Ayar değişince otomatik kayıt, en sık belirli aralıkla yapılmalı. | Ö | K | İ | |

#### 3.2.14 Çoklu harita ve önbellek (MUL)

| Kimlik | Gereksinim | Ö | Durum | Doğ. | Not / kanıt |
|---|---|---|---|---|---|
| MUL-01 | Sınırsız sayıda harita birbirinden tamamen bağımsız olmalı (görünüm, mod, katman, renk, çizim, zemin, dosyalar). | Z | K | S | |
| MUL-02 | Veri ve zemin dokuları bir yöneticinin haritaları arasında tek kopya olarak paylaşılmalı. | Z | K | İ | `TerrainHub` |
| MUL-03 | Önbellek disk ve doku bütçeleri açık harita sayısıyla büyümeli ve tavanla sınırlanmalı. | Ö | K | S | |
| MUL-04 | Aynı süreçteki birden çok host birbirinin dosyalarına ve önbelleğine dokunmadan çalışabilmeli. | Ö | KS | İ | Her host `setDataDirectory` vermeli; Log öneki ortak. Canlı senkron (bir haritadaki değişikliğin diğerine işlenmesi) host'un sorumluluğudur; dizin paylaşımıyla olmaz |
| MUL-05 | Farklı yöneticiler `setCacheDirectory` ile karo önbelleğini paylaşabilmeli; ayarları izole kalmalı. | Ö | K | İ | 0.26.2: önbellek yazımları atomik (geçici dosya + yeniden adlandırma), okumada anahtar doğrulanır, yarım dosya görünmez; CacheTrim yarışması kendini onarır. Aynı veri klasörü gerçek çakışmadır: `initialize` tek satır WARNING yazar (çözüm: `setDataDirectory` ile ayrı klasör ya da farklı harita adları); yalnızca önbellek ortaksa bilgi satırı yazar |

#### 3.2.15 Haritanın kendi arayüzü (UI)

| Kimlik | Gereksinim | Ö | Durum | Doğ. | Not / kanıt |
|---|---|---|---|---|---|
| UI-01 | Haritanın kendi arayüzü (üst şerit, katman paneli, mod listesi, arama, çizim paleti, durum şeridi) açılıp kapanabilmeli. | Ö | K | X | |
| UI-02 | Arayüz öğeleri tek tek gizlenebilmeli, yerleşimi ve renkleri ayarlanabilmeli. | İ | Y | — | |
| UI-03 | Katman panelinde zoom aralığı ve renk düzenlenebilmeli. | İ | Y | — | Bugün yalnızca API / dosya |

#### 3.2.16 Teşhis (DIAG)

| Kimlik | Gereksinim | Ö | Durum | Doğ. | Not / kanıt |
|---|---|---|---|---|---|
| DIAG-01 | Teşhis kipi Log'a çizim süresi, FPS, yeniden çizim sayısı ve zemin aşama sürelerini yazmalı. | İ | K | X | |

### 3.3 Performans gereksinimleri

| Kimlik | Gereksinim | Ö | Durum | Doğ. | Not |
|---|---|---|---|---|---|
| NF-PRF-01 | Harita, açık olduğu her karede X-Plane ana thread'ine ortalama ≤ 2 ms ek yük getirmeli (tek harita, 1000×700 boxel, zoom 11). | Z | U | A | Teşhis kipi `draw ... ms` verir; hedef değerle ölçülmedi |
| NF-PRF-02 | Texture yolunda değişiklik yokken `renderTexture` ≤ 0.1 ms sürmeli. | Ö | U | A | Ölçülmedi |
| NF-PRF-03 | Havalimanı indeksi ilk açılışta ≤ 30 s, önbellekten ≤ 1 s içinde hazır olmalı. | Ö | K | X | Log'da önbellekten 0.05 s |
| NF-PRF-04 | Zoom 8–13 arası gezinmede, önbelleği hazır bölgede zemin ≤ 1 s içinde ince kalitede görünmeli. | Ö | KS | X | Önbellekteki bölgede karşılanıyor; ilk üretim yavaş, iyileştirme yol haritasında |
| NF-PRF-05 | Disk önbelleği ve doku belleği `prime_map.json` sınırlarını aşmamalı. | Z | K | T, X | |
| NF-PRF-06 | Bir katman, host'un aşağı çektiği en küçük zoom'da tek başına 5 ms'den uzun çizilmemeli; katmanın kendi eşiğindeki süresi bunu aşıyorsa o süreyi aşmamalı. | Ö | K | A | `map_preview --bench`, 1200×800 boxel, Seattle ve New York; sonuçlar MIMARI belgesinde ve CHANGELOG'da |

### 3.4 Güvenilirlik

| Kimlik | Gereksinim | Ö | Durum | Doğ. | Not |
|---|---|---|---|---|---|
| NF-REL-01 | Geçersiz API çağrısı çökme yerine uyarı + no-op olmalı. | Z | K | İ | 0.26.2: `shutdown` sonrası tüm harita metotları korumalı (LIF-08); geçersiz değerler sınırlanır |
| NF-REL-02 | Bozuk / eksik veri dosyası (DSF, apt.dat, ayar dosyası) atlanmalı, harita çalışmaya devam etmeli. | Z | K | T | |
| NF-REL-03 | Önbellek silinince hiçbir şey bozulmamalı; yeniden üretilmeli. | Z | K | X | |
| NF-REL-04 | Aynı süreçte birden çok host'un Log satırları kendi önekiyle ayırt edilebilmeli. | İ | Y | İ | Önek süreç genelinde tek |
| NF-REL-05 | X-Plane'siz testler (`map_tests`) zamanlamaya bağlı olmadan tekrarlanabilir geçmeli. | Ö | K | T | Arka plan işi bekleyen testler koşul ve 30 saniyelik üst sınırla bekler; art arda ve işlemci yükü altında tekrar edildi |
| NF-REL-06 | Önbellek yazımları çökme anında dahi yarım dosya bırakmamalı; çökmeden kalan geçici dosyalar temizlenmeli. | Z | K | T | Benzersiz adlı geçici dosya ve yeniden adlandırma; okuyucu kilidinde 10 kez yeniden deneme; `CacheTrim` bir saatten eski geçici dosyaları siler; `map_tests` iki thread × 1000 yazım |

### 3.5 Sürdürülebilirlik ve taşınabilirlik

| Kimlik | Gereksinim | Ö | Durum | Doğ. | Not |
|---|---|---|---|---|---|
| NF-MNT-01 | Kod KIS-04 standardına uymalı ve `/W4` uyarısız derlenmeli (kütüphane, örnek, eklentiler). | Z | K | İ | 0 uyarı |
| NF-MNT-02 | Her genel API değişikliği kullanım kılavuzuna (`docs/PrimeMap_API.pdf`) ve bu SRS'e işlenmeli. | Z | K | İ | Kılavuz HTML + PDF olarak `docs/`'ta tutulur; örnek kod derlenen dosyadan |
| NF-MNT-03 | Sürüm numarası kütüphane, CMake ve resmî eklentide tutarlı olmalı (configure denetimi). | Z | K | İ | |
| NF-POR-01 | X-Plane 12 (Vulkan / Metal arka uçları, OpenGL uyumluluk katmanı) desteklenmeli. | İ | Y | — | Denenmedi |
| NF-POR-02 | macOS / Linux desteklenmeli. | İ | Y | — | Yalnızca Windows x64 |

---

## 4. Doğrulama

| Düzey | Araç | Kapsam |
|---|---|---|
| Birim / bileşen | `map_tests` (`-DMAP_TESTS=ON`, X-Plane'siz) | Görünüm, katman kuralları, ayarlar (JSON), çizimler, apt.dat, DSF, zemin raster / rakım / renk, temalar, `rectFromTopLeft`, zoom eşik tablosu, `Layer` etkin aralık modeli, atomik yazım ... (1.1.1: 34.768 kontrol, 0 hata) |
| Görsel | `map_preview` | Katman kodu gerçek X-Plane verisiyle resme çizilir; `--bench` katman başına çizim süresini ölçer |
| X-Plane içi otomatik | `plugin-api-test` + `selftest.flag` → `MultiXMapSelfTest` | Fabrika, bağımsız harita, ayar dosyaları, sınırlar; Dark varsayılanı, zoom kırpma / ezme / temizleme, sürüm denetimi; Log'da `SELFTEST DONE` |
| X-Plane içi elle | `plugin-api-test` (Texture penceresi), `examples/texture_host`, `prime_map_plugin` | Yerleştirme, takip, girdi, arayüz, kalıcılık |

**1.1.0 için X-Plane'de yapılacak kabul testleri** (U durumundaki gereksinimler bu testlerle K'ya geçer):

| # | Test | Gereksinimler |
|---|---|---|
| 1 | Test eklentisi penceresini taşı / boyutlandır: harita aynı karede gelir, kenarlara taşmaz, keskin | EMB-01..03, ARY-05 |
| 2 | Haritayı sürükle, tekerlekle zoom, sağ tık ile rota bitir, imleçle tooltip | INP-01..03 |
| 3 | Go to airport: kutuya yaz, Enter / Esc, X-Plane'e tıklayınca klavye geri döner | INP-04 |
| 4 | Katman paneli, mod değişimi, `setColor` ile renk | LAY-01, STY-01 |
| 5 | Zoom aralıkları: alçak hava yollarına 5.0 verilince zoom 5'ten çizilir; 3.0 verilince Log'da `clamped to 5` görünür; `clearLayerZoomRange` sonrası 6.5'e döner | ZOOM-01, ZOOM-02 |
| 6 | `texture_host` ve test eklentisi aynı anda: iki doku, ayrı veri klasörleri, ortak karo önbelleği (`setCacheDirectory`); Log'da yönetici başına tek bilgi satırı; aynı veri klasörü verilirse tek satır WARNING | EMB-05, MUL-04, MUL-05 |
| 7 | Teşhis kipinde `draw ... ms` ve `renderTexture` süresi | NF-PRF-01, NF-PRF-02 |
| 8 | `selftest.flag` ile öz-test: `SELFTEST DONE: N passed, 0 failed` | LIF-05/06, MUL-01/03, PER-01, MODE-03, ZOOM-01/02, LIF-09 |
| 9 | Harita açıkken Dark varsayılanı: yeni harita Dark açılır, host `mMode = Vfr` verirse VFR açılır; iki haritanın modu bağımsız değişir | MODE-01, MODE-03 |
| 10 | Harita kapalıyken uçuş: Log'da `prefetch without open maps` satırı görünür; harita açıldığında zemin beklemeden gelir | TER-05 |
| 11 | Ready satırı `dll: ...` yolu yazsın; eski DLL'li kurulumda fabrika tek satır `host built with X, loaded DLL is Y` uyarısı versin | LIF-09, NF-MNT-03 |
| 12 | Önbelleksiz Dark açılış: önbellek klasörleri silinir, X-Plane Seattle'da açılır, iki dakika sonra harita açılır; açılış süresi, bekleyen zemin dokusu ve FPS kaydedilir | TER-05, NF-PRF-04 |
| 13 | Siyah zeminde yalnızca havalimanları: Dark mod, Terrain kapalı, yalnızca havalimanı sembolleri ve pistler; arka plan düz siyah, zemin karosu istenmez | MODE-03, LAY-03 |
| 14 | Navaid zoom aralığı: fix'lere 7.0 verilince zoom 7'de fix'ler çizilir; 5.0 verilince Log'da `clamped to 7` görünür | ZOOM-02 |

---

## 5. Durum özeti ve yol haritası

### 5.1 Özet (1.1.0)

| Alan | K | U | KS | Y | H |
|---|---|---|---|---|---|
| Arayüz (ARY) | 4 | 1 | 1 | 1 | — |
| Yaşam döngüsü (LIF) | 8 | 1 | — | — | — |
| Yerleştirme (EMB) | 1 | 5 | — | 2 | — |
| Girdi (INP) | 1 | 4 | — | 1 | — |
| Görünüm, mod (VIEW, MODE) | 7 | — | — | 2 | — |
| Katman, zoom (LAY, ZOOM) | 5 | — | — | 2 | — |
| Stil (STY) | 1 | — | 2 | 4 | — |
| Zemin, kaynak, çizim, sorgu (TER, SRC, DRW, QRY) | 10 | 1 | — | 3 | — |
| Kalıcılık, çoklu harita, arayüz, teşhis (PER, MUL, UI, DIAG) | 9 | — | 1 | 2 | — |
| İşlev dışı (NF-PRF, NF-REL, NF-MNT, NF-POR) | 11 | 2 | 1 | 3 | — |
| **Toplam (96)** | **57** | **14** | **5** | **20** | **0** |

**Değerlendirme:** 1.1.0 ile "her yerde tam kontrol" hedefinin zoom ayağı tamamlandı: yerleşik eşikler tek tabloda,
host aralığı yerleşik eşiğin yerine geçiyor ve ölçülmüş sert alt sınıra kadar aşağı çekilebiliyor (ZOOM-01/02, NF-PRF-06).
Yerleştirme (Texture), görünüm, katman görünürlüğü, zemin, veri kaynakları, kalıcılık ve çoklu harita hedefe büyük ölçüde
ulaştı; Texture yolu X-Plane'de kapsamlı doğrulanmayı bekliyor. "Her yerde tam kontrol" hedefinden en uzak kalan alanlar
**görsel stil** (STY), **host katmanları** (LAY-04), **katman içi ayrıntı eşikleri** (ZOOM-03), **tema tanımı**
(MODE-02) ve **olay bildirimi** (INP-06).

### 5.2 Yol haritası (öneri sırası)

| Adım | İş | Gereksinimler |
|---|---|---|
| 1 | Texture kabul testleri (bölüm 4, test 1–14); bulunan hataların düzeltilmesi | EMB-01..05, INP-01..04, ARY-05 |
| 2 | Stil modeli: katman başına renk / kalınlık / boy / yazı tablosu (`XMapStyle`), mod başına; tema = stil kümesi | STY-01..07, MODE-02 |
| 3 | Katman içi ayrıntı eşiklerinin (etiket, ok, gösterge) API'ye açılması | ZOOM-03 |
| 4 | Host katmanları: host'un sembol / çizgi / alan / etiket eklediği katman API'si (çizim sırası, etiket çakışması, sorgu) | LAY-04, DRW-03 |
| 5 | Olaylar: tıklama, görünüm değişimi, veri hazır callback'leri; yapısal sorgu sonucu | INP-06, QRY-02 |
| 6 | Texture pop-out desteği ve Texture üstüne hazır pencere yardımcısı; resmî eklentinin Texture'a geçişi; LEGACY stillerin kaldırılması | EMB-06..08 |
| 7 | Pimpl ile genel başlıkların iç türlerden ayrılması; Log önekinin yönetici başına olması | ARY-03, NF-REL-04 |
| 8 | Performans ölçümü ve zemin hızı iyileştirmeleri | NF-PRF-01..04 |

---

## Ek A — İzlenebilirlik (gereksinim → API / kod / test)

| Gereksinim | API | Kod | Test |
|---|---|---|---|
| LIF-01..04 | `XMapManagerFactory`, `XMapManager::set*`, `initialize`, `getStatus` | `src/api/XMapManager*`, `src/engine/XMapData*` | X |
| LIF-05..06 | `addXMap`, `XMap::initialize`, `close`, `removeXMap` | `src/api/XMap.cpp` | `MultiXMapSelfTest` |
| EMB-01..05 | `setTextureSize`, `renderTexture`, `getTextureId` | `XMapWindow::renderTexture`, `gfx/Canvas`, `gfx/RenderTarget` | `plugin-api-test`, `examples/texture_host` |
| INP-01..04 | `injectMouse`, `injectMouseMove`, `injectWheel`, `injectKey`, `wantsKeyboard` | `XMapWindow::inject*`, `onClick`, `onWheel`, `onKey` | aynı |
| VIEW-01..05 | `setCenter`, `setZoom`, `setZoomLimits`, `fitBounds`, `goToAirport`, `screenToGeo`, `geoToScreen` | `XMapView`, `data/AirportSearch` | `map_tests` |
| LAY-01..03 | `setIsLayerVisible`, `getLayerState`, `showOnlyLayers`, `XMapManager::childrenOf ...` | `engine/LayerTable`, `LayerManager` | `map_tests` |
| ZOOM-01, ZOOM-02 | `setLayerZoomRange`, `getLayerZoomRange`, `clearLayerZoomRange`, `getDefaultLayerZoomRange`, `getLayerHardMinZoom` | `engine/LayerTable.cpp` (`kZoomTable`, `applyBuiltInZoomRanges`), `Layer::minZoomEff/maxZoomEff`, `XMap::setLayerZoomRange` (kırpma uyarısı) | `map_tests` (tablo ve etkin aralık), öz-test (zemin ve fix aralıkları), `map_preview --bench`, kabul testleri 5 ve 14 |
| MODE-03 | `XMapConfig::mMode`, `setMode` | `XMapConfig`, `XMapWindow::mMapMode/mTheme` | `map_tests` (Dark varsayılan), öz-test, kabul testi 9 |
| TER-05 | `setPrefetchTiles` (0 = kapalı) | `XMapData::mAircraft` haritasız ön yükleme | kabul testi 10 |
| LIF-09 | `isLibraryVersionMatching()`, `libraryPath()` | `XMapTypes.h` (inline), `XMapManagerFactory` (inline sarmalayıcı + WARNING) | öz-test, kabul testi 11 |
| STY-01, STY-06 | `setColor`, `getColor` | `XMapTheme::themeLayerColor`, `XMapWindow::applyLayerColors`, `TerrainPalette::setBuiltUpColor` | `map_tests` (yerleşim ezmesi), X-Plane kabul testi 4 |
| TER-01..04 | `getTerrainSettings`, `setTerrainSettings`, `precache*` | `data/Terrain*`, `layers/TerrainHub` | `map_tests` |
| SRC-01..03 | `setAircraftSource`, `setAircraft`, `setRouteSource`, `setRoute`, `clearTrack` | `data/AircraftData`, `layers/FmsLayer`, `layers/TrackLayer` | X |
| DRW-01..02 | `addDrawingPoint`, `finishDrawing`, `exportDrawings` ... | `Drawings` | `map_tests` |
| PER-01..03 | `setPersistence`, `exportSettings`, `importSettings`, `saveSettings` | `XMap::loadFiles / saveNow`, `Settings` | `map_tests`, öz-test |
| MUL-01..03 | `addXMap` × n, `getSettings / setSettings` | `XMapData::applyLimits`, `TerrainHub` | öz-test |

## Ek B — Bilinen hatalar ve açık konular

| Kimlik | Konu |
|---|---|
| H-03 | Log öneki süreç genelinde tek (NF-REL-04). |
| AK-01 | LEGACY `Standalone` / `Embedded` ne zaman kaldırılacak: Texture kabul testleri + pop-out (EMB-06) + resmî eklentinin geçişi sonrası. |
| AK-02 | Stil modelinin biçimi (kod API'si ve `map_settings.json` şeması) tasarlanacak. |
| AK-03 | Performans hedeflerinin (NF-PRF-01, NF-PRF-02) X-Plane içinde ölçüm yöntemi ve referans donanımı belirlenecek. |
