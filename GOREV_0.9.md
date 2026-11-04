# GÖREV 0.9 — OpenGL yenileme, havalimanı yüzeyleri, harita modları

> 2026-11-04, kullanıcının isteğiyle. Başlangıç noktası: sürüm 0.8
> (bu dosyayla birlikte verilen proje klasörü).
>
> **Çalışma ortamı:** Proje klasörü iş bilgisayarına elle kopyalanır. Bu
> bilgisayar git'e / GitHub'a bağlı değildir; commit, push, pull yoktur.
> Ekip apt.dat motoru aynı bilgisayardadır ve doğrudan projeye bağlanır.

## 0. Önce oku, sonra bu sırayla çalış

1. `CLAUDE.md`: kurallar, kod standardı, mimari (zorunlu).
2. `README.md`: kullanım, katmanlar, dosya görevleri.
3. Bu dosya: üç iş paketi, bu sırayla:
   - **A — OpenGL yenileme + C tarzı temizlik** (sürüm 0.9)
   - **B — Havalimanı yüzeyleri, ekip apt.dat motoruyla** (sürüm 0.10)
   - **C — Harita modları** (sürüm 0.11)
4. `GOREV.md`: ilk görev tanımı, artık geçmiş kayıt (Bölüm 7 uygulanmadı).

**Neden bu sıra:** A bütün çizim kodunu değiştiriyor; B ve C yeni çizim
yoluyla yazılsın, aynı koda iki kez dokunulmasın. B'nin C'den önce gelmesinin
nedeni kullanıcının önceliğinin havalimanı poligonları olması.

Her paket adımlara bölündü. Her adımın sonunda Bölüm D'deki kontrol listesi
uygulanır. **X-Plane'de denenmemiş hiçbir şey "çalışıyor" diye raporlanmaz.**
Bir adım X-Plane'de geçmeden sonrakine geçilmez.

---

## 1. Değişmeyen kurallar (özet; tamamı CLAUDE.md'de)

- **Kod standardı:**
  - Önekler: `m` üye değişken (struct alanları dahil), `p` parametre, `t` yerel
    değişken (döngü sayaçları dahil), `k` sabit, `g` global.
  - Süslü parantez her zaman alt satırda açılır (lambda dahil); tek satırlık
    `if` de parantezli.
  - Tür ve fonksiyon adları PascalCase. Satır en çok 120 karakter.
  - Tanımlayıcılar, arayüz metinleri ve log İngilizce. Kod yorumları Türkçe ve
    ASCII (ş/ğ/ı yerine s/g/i). README.md ve CLAUDE.md Türkçe.
- **Sadece X-Plane 11 SDK.** Sadece XPLM200..XPLM301 tanımlı; XP12'ye özel
  API derlenmez. Dış harita servisi (OSM, Kaşif) yok.
- **Arka plan thread'leri:** içlerinde hiçbir XPLM fonksiyonu (Log dahil)
  çağrılmaz. Log'a yazılacaklar rapor metni olarak ana thread'e döner.
- **Biçim tahmin edilmez.** apt.dat, earth_nav.dat ve DSF için gerçek dosyaya
  ya da ekip motorunun belgesine bakılır. Bu dosyada **DOĞRULA** yazan her
  madde için böyle.
- **Dokunulmayacak klasörler:** `_qt_template_yedek/`, `_to_delete/`,
  `build/`, `.qtcreator/`. X-Plane klasöründe sadece
  `Resources/plugins/MapPlugin/` altına yazılır.
- **Derleme:**
  - Qt Creator ile (kit "Desktop Qt 6.10.1 MSVC2022 64bit") ya da komut
    satırından: vcvars64 + CMake + Ninja.
  - Testler ve araçlar için: `-DMAP_TESTS=ON -DMAP_PREVIEW=ON`.
  - X-Plane başka bir klasördeyse `-DXPLANE_DIR=...` ver.
  - X-Plane açıkken `win.xpl` kilitlidir; kopyalama başarısız olur.

### 1.1 Motor kodu nerede durur

- İş bilgisayarında motor ve adapter doğrudan projeye eklenebilir
  (`src/data/` altına ya da ayrı bir klasöre; `CMakeLists.txt`'ye eklenir).
- Yine de motora bağlı dosyaları ayrı tut (adapter + motor tek yerde; geri
  kalan kod sadece `IAirportGeometrySource` arayüzünü bilir). Böylece proje
  motor olmadan da derlenir (geçici `AptDatReader` ile) ve kullanıcı
  değişiklikleri kendi bilgisayarına taşırken motor dosyalarını kolayca
  dışarıda bırakabilir.

---

## 2. Şu anki durum (0.8)

**Kullanıcı 2026-11-04'te X-Plane'de denedi** (ekran görüntüsü). Görünen ve
doğru yerde olanlar:
- zemin (Seattle),
- kullanıcı çizimleri (nokta, rota, bölge),
- katman paneli, uçak, hava yolları, ILS tüyleri.

**Görülen eksikler:**
1. Havalimanı yüzeyleri (apron, taksi yolu) yok. Beklenen bir durum: geçici
   okuyucu sadece pistleri okuyor, arayüz de sadece pist taşıyor (paket B).
2. Harita X-Plane'in kendi haritasından farklı görünüyor: X-Plane'deki
   VFR/IFR modları bizde yok (paket C).

**Kullanıcının OpenGL isteği (aynen):** "OpenGL eski bir sürüm
kullanılıyordu ... Hala eski sürüm kullanılıyorsa lütfen yenisine göre yaz.
Yazan için de okuyan için de çok zor ilk sürüm. Aşırı C tarzı oluyor kodlar
da."

0.8'de bu yapılmadı. Yeni kod `gfx/Batch` ve `gfx/Texture` içinde toplandı ama
onların içi hâlâ `glBegin/glEnd`; eski katmanlar da doğrudan `gl...` çağırıyor.

---

## A. OpenGL yenileme ve C tarzı temizlik (sürüm 0.9)

### A.0 Bugün OpenGL nerede kullanılıyor

| Dosya | gl çağrısı | glBegin | Eski özellikler |
|---|---|---|---|
| `src/layers/NavaidLayer.cpp` | 23 | 5 | glLineWidth, glPointSize |
| `src/ui/UiDraw.cpp` | 20 | 3 | GL_QUADS, GL_LINE_LOOP, glLineWidth |
| `src/layers/AirportLayer.cpp` | 16 | 2 | glLineWidth |
| `src/gfx/Texture.cpp` | 14 | 1 | glTexCoord2f |
| `src/gfx/Batch.cpp` | 14 | 4 | glLineWidth, glPointSize |
| `src/layers/AircraftLayer.cpp` | 13 | 3 | GL_LINE_LOOP, glLineWidth |
| `src/layers/RunwayLayer.cpp` | 10 | 2 | |
| `src/GlScreen.cpp` | 9 | – | matris okuma, scissor (kalır) |
| `src/layers/GridLayer.cpp` | 8 | 1 | glLineWidth |
| `src/layers/MapSymbols.cpp` | 7 | – | `Emit*`: glBegin içinde köşe üretir; `SetColor` = glColor |
| `src/GLInclude.h` | 3 | – | `CullFaceOff` |
| `src/MapWindow.cpp` | 1 | – | glLineWidth(1) |
| `tools/preview/*.cpp` | 23 | | önizleme bağlamı, XPLM taklitleri |

Drawing, Track, Fms ve Airway katmanları `Gfx::LineBatch`, `TriangleBatch` ve
`PointBatch` kullanıyor. Batch'in içi değişince bu dört katman kendiliğinden
yeni yola geçer.

### A.1 Hedef

- **Tek kapı:** Katmanlar ve arayüz hiç `gl...` çağırmaz; çizim sadece
  `Gfx::Renderer` üzerinden yapılır.
- **Geometri CPU'da toplanır:** şekiller `Gfx::Mesh`'e eklenir. Mesh
  X-Plane'siz çalışır ve test edilebilir. Renderer onu vertex buffer'a yükleyip
  shader ile çizer.
- **OpenGL 3.3 düzeyi:** VBO + VAO + GLSL `#version 330 core`. Sabit fonksiyon
  yolu tamamen kalkar: glBegin/glVertex/glColor/glTexCoord, glLineWidth ile
  kalınlık, glPointSize, GL_QUADS.
- **RAII:** GL nesneleri sınıflarda durur; yıkıcı siler, sınıflar
  kopyalanamaz ama taşınabilir.
- **Metin değişmez:** X-Plane'in `XPLMDrawString`'i kalır (X-Plane'in yazı
  tipi). Onun içeride eski GL kullanması bizi ilgilendirmez; bize düşen,
  yazıdan önce GL durumunu X-Plane'e geri vermiş olmak.

### A.2 X-Plane 11'de modern OpenGL: bilinmesi gerekenler

1. **Fonksiyon yükleme:** Windows'ta `opengl32.lib` sadece OpenGL 1.1
   fonksiyonlarını verir. 1.1'den sonraki her fonksiyon (`glGenBuffers`,
   `glBindVertexArray`, `glCreateShader`, `glUniformMatrix4fv`,
   `glActiveTexture`...) çalışma anında `wglGetProcAddress` ile alınır.
   Başarısızlıkta `nullptr`, 1, 2, 3 veya -1 dönebilir; hepsini hata say.
2. **Ne zaman yüklenir:** Fonksiyonlar ancak OpenGL bağlamı etkinken
   alınabilir. Bu yüzden XPluginStart'ta değil, **ilk draw callback'inde bir
   kez** yükle.
   - `glGetString(GL_VERSION)`, `GL_RENDERER` ve
     `GL_SHADING_LANGUAGE_VERSION` değerlerini Log.txt'ye yaz.
   - Sürüm 3.3'ten küçükse ya da bir fonksiyon eksikse: Log'a açık bir hata
     yaz, harita alanına "OpenGL 3.3 required" yaz (XPLMDrawString çalışır).
     Eklenti çökmemeli.
3. **Başlık dosyası:** Windows SDK'da `glext.h` yok. Gereken yaklaşık 30
   fonksiyonun typedef'lerini kendimiz yazarız (`gfx/GlApi.h`); dış kütüphane
   gerekmez. Önerim elle yazmak: küçük ve okunur olur. glad'in ürettiği dosya
   da kullanılabilir (MIT/CC0, `third_party/` altına).
4. **GL durumunu geri vermek:** X-Plane, callback'imizden çıkınca GL durumunun
   kendi bıraktığı gibi olmasını bekler. Çizimden önce kaydet, sonra geri yükle:
   - `GL_CURRENT_PROGRAM`
   - `GL_VERTEX_ARRAY_BINDING`
   - `GL_ARRAY_BUFFER_BINDING`
   - `GL_ACTIVE_TEXTURE`
   - `GL_CULL_FACE`'in açık olup olmadığı
   - scissor

   Bunu `gfx/GlStateGuard` yapar; bugünkü `CullFaceOff` onun içine girer.
   Vertex attribute'larımız kendi VAO'muzda kalır.
5. **Dokular:**
   - Doku numarası `XPLMGenerateTextureNumbers` ile alınır.
   - Bağlamak için `XPLMBindTexture2d(tId, 0)` kullanılır, doğrudan
     `glBindTexture` değil. X-Plane bağlı dokuyu kendi önbelleğinde tutar;
     onu atlarsak X-Plane'in sonraki çizimi yanlış dokuyu kullanır.
   - Shader'daki sampler 0. birimi kullanır.
6. **`XPLMSetGraphicsState` kalır** (`Ui::UseFlatState` ve dokulu hâli):
   - Shader kullansak da X-Plane'in durum önbelleği gerçek durumla uyuşmalı.
   - Harmanlama (blend) da bu çağrıyla açılır.
7. **Koordinatlar:** X-Plane pencere çiziminde projeksiyon ve modelview
   matrislerini boxel koordinatına göre kurar (pop-out ve UI ölçeği dahil).
   - Bunları her draw callback'inde
     `glGetFloatv(GL_PROJECTION_MATRIX / GL_MODELVIEW_MATRIX)` ile oku, çarp
     ve shader'a `uMvp` uniform'u olarak ver.
   - `src/GlScreen.cpp` aynı matrisleri zaten okuyor; orayı kullan.
     Uyumluluk bağlamında bu çağrılar geçerli.
8. **Çizgi kalınlığı:** Yeni yolda `glLineWidth > 1` güvenilir değil. Kalın
   çizgiler CPU'da dörtgen (2 üçgen) olarak üretilir.
   - Kalınlık bugün piksel cinsinden, Mesh ise boxel cinsinden çalışır:
     `tWidthBoxel = tWidthPx / pContext.mPixelsPerBoxel` (bugünkü görünüm
     aynı kalsın).
   - Noktalar (NDB noktaları vb.) küçük dolu daire olarak çizilir;
     glPointSize kalkar.
9. **Vulkan:** X-Plane 11.50 ve sonrası Windows'ta Vulkan sürücüsüyle de
   çalışabilir (Settings → Graphics'teki Vulkan seçeneği; yeniden başlatma
   ister). Eklentiler o modda da OpenGL ile çizer. **İki modda da test et.**
10. **Önizleme aracı:** `tools/preview` kendi bağlamını `wglCreateContext` ile
    açıyor. Sürücüler bunda genelde uyumluluk bağlamının en yüksek sürümünü
    verir, yükleyici aynen çalışmalı. `XplmStubs.cpp`'deki
    `XPLMBindTexture2d`/`XPLMSetGraphicsState` taklitleri yeterli.
    DOĞRULA: preview matrisleri kendisi kuruyor mu (`glOrtho` vb.); Renderer
    matrisleri oradan okuyacak.

### A.3 Yeni dosyalar

Adlar değişebilir ama sorumluluklar böyle ayrılsın:

| Dosya | Bağımlılık | Görevi |
|---|---|---|
| `src/gfx/GlApi.h/.cpp` | GL | typedef'ler, fonksiyon işaretçileri, `bool LoadGlApi(std::string& pOutError)` |
| `src/gfx/GlStateGuard.h/.cpp` | GL | durumu kaydet / geri yükle (RAII), CullFace kapatma |
| `src/gfx/ShaderProgram.h/.cpp` | GL | derleme, bağlama, hata metni, uniform konumları |
| `src/gfx/GpuBuffer.h/.cpp` | GL | VBO + VAO (RAII), her karede yeniden yükleme |
| `src/gfx/Texture.h/.cpp` | GL + XPLM | `class Texture` (RAII): oluştur, güncelle, bağla |
| `src/gfx/Mesh.h/.cpp` | **yok**: CORE_SOURCES'a girer, testlenir | köşe listesi + şekil üreticileri |
| `src/gfx/Renderer.h/.cpp` | GL | Mesh çizimi, dokulu çizim, durum yönetimi |

**Mesh taslağı** (`Gfx::Point` bugün `gfx/Batch.h`'de var; ortak bir
başlığa taşınır):

```cpp
namespace Gfx
{

struct ColorVertex
{
    float mX;
    float mY;
    uint8_t mR;
    uint8_t mG;
    uint8_t mB;
    uint8_t mA;
};

struct TexturedVertex
{
    float mX;
    float mY;
    float mU;
    float mV;
};

// Renkli ucgen listesi (her 3 kose bir ucgen). Cizgiler de ucgen olarak uretilir.
// Koordinatlar boxel; kalinliklar boxel.
class Mesh
{
public:
    void Clear();
    bool IsEmpty() const;
    const std::vector<ColorVertex>& Vertices() const;

    void AddTriangle(const Point& pA, const Point& pB, const Point& pC, const Rgba& pColor);
    void AddQuad(const Point& pA, const Point& pB, const Point& pC, const Point& pD, const Rgba& pColor);
    void AddRect(const RectD& pRect, const Rgba& pColor);
    void AddLine(const Point& pFrom, const Point& pTo, float pWidth, const Rgba& pColor);
    void AddPolyline(const std::vector<Point>& pPoints, bool pClosed, float pWidth, const Rgba& pColor);
    void AddDashedPolyline(const std::vector<Point>& pPoints, float pWidth, float pDash, float pGap,
                           const Rgba& pColor);
    void AddDisc(const Point& pCenter, float pRadius, const Rgba& pColor);
    void AddRing(const Point& pCenter, float pRadius, float pWidth, const Rgba& pColor);
    void AddRegularPolygonOutline(const Point& pCenter, float pRadius, int pSides, float pStartDeg,
                                  float pWidth, const Rgba& pColor);
    void AddDottedCircle(const Point& pCenter, float pRadius, int pDots, float pDotRadius, const Rgba& pColor);
    void AddIndexed(const std::vector<Point>& pPoints, const std::vector<int>& pIndices, const Rgba& pColor);

private:
    std::vector<ColorVertex> mVertices;
};

}  // namespace Gfx
```

- **Kontur (sembol altındaki koyu çizgi):** Aynı şekli önce `kOutline`
  renginde ve daha kalın olarak ayrı bir Mesh'e ekle, önce onu çiz. Bugünkü
  `DrawOutlined` davranışı budur.
- **Köşe birleşimi:** Polyline köşelerinde miter kullan; çok sivri açıda
  bevel'e düş (miter sınırı ~4 × kalınlık). Kapalı poligonda son-ilk köşesi
  de birleştirilir.
- **Daire segment sayısı:** Yarıçapa göre, örneğin
  `std::clamp(int(std::ceil(r * 1.5f)), 12, 64)`.

**Renderer taslağı:**

```cpp
class Renderer
{
public:
    bool Init(std::string& pOutError);  // ilk draw callback'inde: GlApi + shader'lar
    void BeginFrame();                  // matrisleri okur, GL durumunu kaydeder
    void Draw(const Mesh& pMesh);       // renkli ucgenler
    void DrawTextured(const Texture& pTexture, const std::vector<TexturedVertex>& pVertices, float pOpacity);
    void EndFrame();                    // durumu geri yukler (yazidan once de cagrilir)
};
```

- **Tek VBO:** Her `Draw`'da `glBufferData(..., GL_STREAM_DRAW)` ile yeniden
  ayır ("orphaning"), sonra `glDrawArrays(GL_TRIANGLES)`. Statik önbellek
  şimdilik gerekmez: önce ölç, gerekirse sonra ekle.
- **İki shader:**
  - Renkli: `in vec2 aPosition; in vec4 aColor; uniform mat4 uMvp;`
  - Dokulu: `in vec2 aPosition; in vec2 aUv; uniform sampler2D uTexture; uniform float uOpacity;`

  Kaynak metinleri `Renderer.cpp` içinde raw string olarak durur.
- **Geometri ve yazı sırası:** Katmanlar önce geometriyi (`Draw`), sonra
  yazıları (`DrawLabels`) çizer. `LayerManager::DrawAll` geometri bitince
  Renderer'ın durumunu geri versin, yazılar ondan sonra çizilsin. Arayüzde de
  aynısı: önce kutular, sonra yazılar.
- **Sahiplik:** Renderer örneği MapWindow'da durur. `DrawContext`'e
  `Gfx::Renderer* mRenderer` eklenir. UiDraw fonksiyonları da Renderer'ı
  parametre olarak alır; global durum olmaz.

### A.4 Adımlar

**A1 — Yükleyici ve tek üçgen.**
- GlApi, GlStateGuard, ShaderProgram, GpuBuffer ve Renderer iskeletini kur.
- Teşhis modunda harita köşesine Renderer ile küçük renkli bir üçgen çiz.
- Log'a GL sürümünü yaz.
- *X-Plane testi:*
  - Üçgen hem OpenGL hem Vulkan modunda görünüyor.
  - X-Plane'in kendi arayüzü, kokpit ve diğer pencereler bozulmuyor (renk,
    doku, yazılar).
  - Log'da hata yok.
- Bu adım geçmeden devam etme. En riskli kısım burası: X-Plane'in GL
  durumunu bozmamak.

**A2 — Mesh ve Batch'in içi.**
- `Gfx::Mesh`'i testleriyle yaz.
- `LineBatch`, `TriangleBatch` ve `PointBatch`'in içini Mesh + Renderer'a
  geçir; dış API'leri şimdilik aynı kalabilir. Böylece Drawing, Track, Fms ve
  Airway katmanları da geçmiş olur.
- *X-Plane testi:* çizimler, uçuş izi, FMS ve hava yolları eskisi gibi.

**A3 — MapSymbols ve katmanlar.**
- `MapSymbols::Emit*` ve `SetColor` kalkar, yerlerine Mesh fonksiyonları
  gelir.
- Sırayla geçir: GridLayer, RunwayLayer, AirportLayer, NavaidLayer
  (VOR/NDB/ILS/Fix), AircraftLayer.
- Her katmandan sonra önizleme karşılaştırması yap (A.5).

**A4 — Arayüz.**
- `UiDraw` (FillRect, FrameRect, Line, Button, CheckBox), Toolbar,
  LayerPanel, StatusBar.
- MapWindow'daki arka plan dolgusu ve `glLineWidth`.

**A5 — Doku ve zemin.**
- `gfx/Texture` RAII sınıf olur.
- TerrainLayer Mercator şeritlerini `TexturedVertex` listesi olarak üretir
  (bugünkü `DrawTextureLatLon` mantığı CPU tarafına taşınır) ve
  `Renderer::DrawTextured` ile çizer.
- LRU'dan çıkan dokular yıkıcıyla silinir.

**A6 — Temizlik.**
- `GLInclude.h` sadece `gfx/` içinde kullanılır.
- Kabul için iki arama:
  - `src/` altında, `gfx/` hariç, `\bgl[A-Z]` araması boş olmalı.
  - `gfx/` içinde
    `glBegin|glEnd|glVertex|glColor|glTexCoord|glLineWidth|glPointSize|GL_QUADS`
    araması boş olmalı.
- Kullanılmayan Batch/MapSymbols kodu silinir: Mesh ile aynı işi yapan
  ikinci bir kod kalmasın.

**A7 — C tarzı temizlik (kullanıcının şikayeti).** Kodu baştan yazma;
dokunduğun dosyalarda ve şu listede yap. Davranış değişmez: her adımda
testler ve önizleme karşılaştırması.
- **C++20 ve std::format:** C++20'ye geç (`CMAKE_CXX_STANDARD 20`; MSVC 2022
  destekliyor). `snprintf` + `char tText[64]` yerine `std::format` kullan.
  - Dosyalar (adet): NavaidLayer 9, StatusBar 5, AptDatReader 5, UiDraw 4,
    FmsLayer 4, DrawingLayer 4, TerrainStore 4, Plugin 4, MapFormat 4, Log 4,
    AirportLayer 3, MapWindow 3, TerrainClasses 2, SceneryIndex 2,
    NavLoader 2, AirwayDatabase 2, GridLayer 1, TerrainRaster 1.
  - `Log(const char* pFormat, ...)` şuna dönüşür:
    `template <typename... TArgs> void Log(std::format_string<TArgs...> pFormat, TArgs&&... pArgs)`.
  - C++20 + `/permissive-` yeni hatalar çıkarırsa (u8 literalleri,
    karşılaştırma operatörleri) düzelt.
- **Metin parametreleri:** `const char*` yerine `std::string_view` (Layer
  kurucusu, `Ui::Text`, `DrawLabelNear` ...). `XPLMDrawString` `char*`
  istediği için dönüşüm **sadece** UiDraw içinde yapılır.
- **C dizileri:** `std::array`'e geçer (`RunwayEnd mEnds[2]`, stil
  tabloları, imza dizileri).
- **Çıkış parametresi işaretçiler:** `bool F(..., T* pOut)` yerine
  `std::optional<T>` döndür. Örnekler: `ReadBoxelToPixel(Affine2D*)`,
  `SceneryIndex::ParseTileName(name, int*, int*)`. Sadece dokunduğun
  yerlerde, testleriyle.
- **Durum tutan global fonksiyonlar** kalmaz (`MapSymbols::SetColor` gibi).
- **XPLM callback'leri** C fonksiyonu olmak zorunda; bugünkü "statik
  trampolin + refcon = this" düzeni kalır.

### A.5 Kabul ölçütleri

1. **Önizleme karşılaştırması:**
   - A'ya başlamadan önce aşağıdaki 6 referans görüntüyü al.
   - Her adımdan sonra aynılarını al ve karşılaştır:
     `magick compare -metric AE ref.png new.png diff.png`.
   - Kalın çizgi yöntemi değiştiği için 1-2 piksellik kenar farkları normal.
     Eksik sembol, renk farkı ya da yer kayması olmamalı.

   ```
   map_preview --lat 47.45 --lon -122.30 --zoom 13.6 --out ksea.ppm
   map_preview --lat 47.47 --lon -122.33 --zoom 12.3 --out seattle12.ppm
   map_preview --lat 47.45 --lon -122.9  --zoom 8.2  --no-ui --out puget8.ppm
   map_preview --lat 47.6  --lon -122.3  --zoom 10   --demo-drawings --out drawings.ppm
   map_preview --lat 47.6  --lon -122.3  --zoom 9.5  --fixes --out fixes.ppm
   map_preview --lat 21.3  --lon -157.9  --zoom 9    --out hawaii.ppm
   ```

   (`--cache <klasor>/` verilirse zemin önbelleği tekrar kullanılır.)
2. **X-Plane'de:** OpenGL ve Vulkan modlarında harita eskisiyle aynı
   görünüyor, X-Plane'in diğer arayüzleri bozulmuyor, pop-out penceresi
   çalışıyor.
3. **Performans:** Teşhis modundaki "draw x ms" değerini A'dan önce ve sonra
   aynı görünümde not et (örnek: Seattle zoom 9, fix ve hava yolları açık).
   Sonraki değer daha kötü olmamalı.
4. **Kod:** A6'daki aramalar boş, /W4 uyarısız, testler geçiyor. Yeni Mesh
   testleri: üretilen köşe sayıları, kalınlık, kapalı poligon, miter sınırı.

---

## B. Havalimanı yüzeyleri — ekip apt.dat motoru (sürüm 0.10)

### B.0 Durum

- Bugün `IAirportGeometrySource` sadece pistleri taşıyor
  (`AirportGeometry::mRunways`).
- Ekip motoru apt.dat'ı birebir sınıflara bölüyor (Airport → Runway →
  RunwayEnd ...).
- Yapılacaklar:
  1. Arayüze apt.dat satırlarına birebir karşılık gelen yapıları ekle.
  2. Adapter'da motor sınıflarından bu yapılara kopyala.
  3. Eğri düzleştirme, üçgenleme ve çizimi bizim (testli) kodumuzda yap.
- **Neden bu iş bizde:** Motor apt.dat'ı birebir verdiği için Bezier kontrol
  noktaları ham geliyor. Bunları düzleştirmek ve delikli poligonu üçgenlemek
  haritaya özgü bir iş; testleriyle bizde durmalı. Motor zaten düzleştirilmiş
  nokta listesi veriyorsa adapter düğümleri `mHasControl = false` olarak
  verir; geri kalanı aynen çalışır.

### B.1 Motoru bağlama

- Adapter: `IAirportGeometrySource`'tan türeyen sınıf (örnek ad
  `src/data/TeamAptDatSource.h/.cpp`). `LoadAll()` içinde motoru çalıştırır
  ve sonucu B.2'deki yapılara kopyalar.
- Motorun kaynakları/kütüphanesi ve adapter `CMakeLists.txt`'de
  `PLUGIN_SOURCES`'a eklenir. Bir CMake seçeneği önerilir:
  `option(MAP_TEAM_APTDAT "Use the team apt.dat engine" ON)`; kapalıyken
  proje geçici `AptDatReader` ile derlenir.
- **Fabrika dosyası:** `CreateAirportGeometrySource()` `src/Plugin.cpp`'den
  yeni bir çekirdek dosyaya taşınır: `src/data/AirportSourceFactory.h/.cpp`.
  Böylece önizleme aracı da aynı kaynağı kullanır (bugün preview
  `AptDatReader`'ı doğrudan yaratıyor: `tools/preview/PreviewMain.cpp:267`).

  ```cpp
  std::unique_ptr<IAirportGeometrySource> CreateAirportGeometrySource()
  {
  #if MAP_TEAM_APTDAT
      return std::make_unique<TeamAptDatSource>();
  #else
      return std::make_unique<AptDatReader>();  // GECICI okuyucu
  #endif
  }
  ```
- **Talimatları güncelle:** `src/data/AirportGeometry.h`'nin başındaki
  bağlama talimatı ve README'deki "apt.dat motorunu bağlama" bölümü.

### B.2 Arayüz genişletme (`src/data/AirportGeometry.h`)

Yapılar apt.dat 1100 satır kodlarıyla birebir eşleşir. Var olan `Runway` ve
`RunwayEnd` aynen kalır; RunwayLayer, NavaidLayer (ILS) ve `LocalizerMatch`
onları kullanıyor.

```cpp
// 111..116: bir dugum
struct AptNode
{
    LatLon mPosition;
    bool mHasControl = false;  // 112, 114, 116: Bezier kontrol noktasi var
    LatLon mControl;           // dosyadaki kontrol noktasi (dugumden CIKAN egri icin)
    int mLineType = 0;         // sadece 120 ozelliklerinde: cizgi turu, 0 = yok
    int mLightType = 0;        // sadece 120 ozelliklerinde: isik turu, 0 = yok
};

// Dugum zinciri: 113/114 ile biterse kapali halka, 115/116 ile biterse acik cizgi
struct AptChain
{
    std::vector<AptNode> mNodes;
    bool mClosed = false;
};

struct Pavement  // 110
{
    int mSurfaceCode = 1;          // apt.dat yuzey kodu
    double mSmoothness = 0.25;
    double mTextureHeadingDeg = 0.0;
    std::string mName;
    std::vector<AptChain> mRings;  // [0] dis sinir, digerleri delik (hepsi kapali)
};

struct LinearFeature  // 120
{
    std::string mName;
    std::vector<AptChain> mChains;
};

struct AirportBoundary  // 130
{
    std::string mName;
    std::vector<AptChain> mRings;
};

struct Helipad  // 102
{
    std::string mName;
    LatLon mCenter;
    double mHeadingDeg = 0.0;
    double mLengthM = 0.0;
    double mWidthM = 0.0;
    int mSurfaceCode = 1;
};
```

**`AirportGeometry`'ye eklenecek alanlar:**
- `int mRowCode`: 1 kara, 16 deniz üssü, 17 heliport.
- `double mElevationFt`.
- `bool mHasTower`: 1. satırdaki kule bayrağı.
- `std::vector<Pavement> mPavements`, `std::vector<LinearFeature> mLinearFeatures`,
  `std::vector<AirportBoundary> mBoundaries`, `std::vector<Helipad> mHelipads`.
- İsteğe bağlı: frekanslar (50–56 / 1050–1056). Paket C'de kule rengi için
  işe yarar.

**101 (su pisti) satırları** `Runway` olarak verilir, `mSurface = Water`.

DOĞRULA (motorun belgesinden ya da kodundan): alan sıraları, birimler (feet
mi metre mi), 1050'li satırlardaki frekans birimi.

**Adapter kuralları** (`AirportGeometry.h`'nin başındaki talimata ek):
- Birimler: derece (WGS84), metre (pist ölçüleri), feet (rakım).
- Motor "her ICAO bir kez" kuralını (Custom Scenery sırası önce)
  uyguluyorsa olduğu gibi kullan.
- Bellek: motorun nesnelerinden kopyaladıktan sonra motorun kopyasını bırak;
  bellekte iki kopya durmasın.

### B.3 Eğri düzleştirme (`src/data/AirportShapes.h/.cpp`, CORE, testli)

- **Kural (apt.dat):** Kontrol noktası düğümden çıkan eğriye aittir. Düğüme
  gelen eğri için kontrol noktasının düğüme göre aynası kullanılır:
  2·P − C.
- **Segment formülü:** A→B segmenti kübik Bezier'dir:
  - P0 = A
  - P1 = A kontrollüyse A.C, değilse A
  - P2 = B kontrollüyse 2·B − B.C, değilse B
  - P3 = B

  Kapalı halkada son→ilk segmenti de vardır; açık zincirde yoktur.
- **DOĞRULA:**
  - Kuralı ekip motorunun belgesiyle karşılaştır.
  - Görsel kontrol: KSEA'da kavisli bir taksi yolu kenarı X-Plane'in 3D
    görüntüsü ve kendi haritasıyla aynı yöne kavis yapmalı. Ters kavis
    yapıyorsa ayna kuralı ters uygulanmıştır.
- **Asimetrik kavis:** apt.dat bunun için aynı noktada iki düğüm verebilir;
  sıfır uzunluklu segmentler atlanır.
- **Yerel düzlem:** Hesap, havalimanı referans noktası etrafında
  doğu/kuzey metre cinsinden yapılır:
  - `x = (lon − lon0) · cos(lat0) · 111320`
  - `y = (lat − lat0) · 110540`

  ±50 km içinde hassasiyet yeterli.
- **Parça sayısı:** Her segment, kontrol çokgeninin uzunluğu / 2 m parçaya
  bölünür; en az 1, en çok 32.

### B.4 Üçgenleme (delikli poligon)

Bugünkü `TriangulatePolygon` (`src/Geo.h`) sadece deliksiz basit poligonlar
içindir.

- **Seçenek 1 (önerilen, şirket politikası izin verirse):** mapbox
  `earcut.hpp`. Tek başlık dosyası, ISC lisansı; `third_party/earcut/` altına
  LICENSE dosyasıyla konur. Delikleri, dejenere noktaları ve büyük
  poligonları sağlam çözer.
- **Seçenek 2: kendi uygulamamız.**
  - Delik köprüleme: her deliğin en sağdaki noktası dış sınırda görünen bir
    noktaya bağlanır (Eberly yöntemi), ardından kulak kırpma.
  - Yön düzeltme: dış sınır saat yönünün tersine, delikler saat yönüne
    çevrilir.
  - Ardışık aynı noktalar ve doğrusal noktalar atılır.
- **Hata durumunda:** O poligon için sadece kontur çizilir ve hata sayısı
  rapora yazılır. Eklenti çökmez.
- **Testler:**
  - Kare içinde kare delik: üçgen alanlarının toplamı = dış alan − delik alanı.
  - İki delik.
  - Ters yönlü girdi.
  - Tekrar eden noktalar.
  - Kendini kesen girdi: false döner.

### B.5 Performans ve bellek

- Global Airports'ta binlerce havalimanının yüzeyi var (milyonlarca düğüm).
  Hepsini açılışta üçgenleme.
- **Tembel üretim:**
  - Havalimanı ekrandayken ve zoom ≥ 11 olunca arka plan işçisine istek
    gider. `TerrainStore`'daki istek/sonuç düzeni örnek alınabilir.
  - Sonuç `AirportSurfaceMesh` olarak saklanır (yerel metre, float).
  - Ana thread her karede köşeleri ekran koordinatına çevirir.
  - LRU sınırı: örneğin 64 havalimanı ya da 64 MB.
- **Ölçüm:**
  - Açılışta Log'a: yüzeyi olan havalimanı sayısı, toplam düğüm sayısı,
    tahmini bellek.
  - Teşhis modunda: havalimanı başına üçgenleme süresi.

### B.6 Katman: `src/layers/AirportSurfaceLayer`

Kimlik `airport_surfaces`, panelde görünen ad "Airport surfaces".

- **Katman sırası** (`Plugin.cpp` `CreateLayers()` ve preview'deki liste):
  Terrain → **AirportSurfaces** → Grid → ILS → Runways → ...
- **Katman içindeki çizim sırası:**
  - sınır (130): çok soluk dolgu ya da kontur, zoom ≥ 10,
  - yüzeyler (110): yüzey rengine göre, zoom ≥ 11,
  - heliport pedleri (102): zoom ≥ 13,
  - çizgiler (120): zoom ≥ 14.
- **Renk tablosu tek yerde** dursun (paket C'de temaya taşınacak).
  Başlangıç değerleri:
  - asfalt (1): pistten biraz açık koyu gri. Pist rengi
    `src/layers/RunwayLayer.cpp`'de `kPavedColor = {0.22, 0.23, 0.26}`;
    yüzey için örnek `{0.30, 0.31, 0.34}`.
  - beton (2): daha açık gri.
  - çim (3), toprak (4), çakıl (5), kuru göl yatağı (12): toprak tonları.
  - su (13) ve saydam (15): çizilmez.
  - kar (14): açık renk.

  Son renkler X-Plane'in kendi haritasıyla karşılaştırılarak seçilir.
- **Çizgi türleri:**
  - sarı çizgiler: taksi orta hattı (1), kesikli (2), çift (3), bekleme
    çizgileri (4–6) ...
  - 51–57: siyah kenarlı sürümler.
  - 20–22: beyaz çizgiler.
  - Işıklar (101 ve sonrası) şimdilik çizilmez.

  DOĞRULA: Tam tabloyu apt.dat 1100 belgesinden ya da motorun belgesinden al.
- **AirportLayer iyileştirmesi:** Havalimanı türü motorun `mRowCode`'undan
  okunur. Bugün adın başındaki [H]/[S]/[X] önekinden tahmin ediliyor
  (`AirportKindFromName`). Nav verisinde olup motorda olmayan havalimanları
  için eski yöntem kalır.

### B.7 Adımlar

**B1 — Kanca ve arayüz.**
- Fabrika dosyasını ve arayüz genişletmesini yap.
- `FakeAirportSource`'a (`tests/tests.cpp`) delikli bir yüzey ekle: motor
  olmadan derlenmeli, testler geçmeli.
- Bu adım motorsuz (`MAP_TEAM_APTDAT=OFF`) da derlenmeli.

**B2 — Şekil hesapları.** `AirportShapes` (Bezier düzleştirme + üçgenleme)
ve testleri.

**B3 — Katman.** `AirportSurfaceLayer` + tembel üretim. Önizlemede sahte
kaynakla görüntü al.

**B4 — İşte: motoru bağla.**
- Adapter'ı yaz, motoru bağla, önizleme ve X-Plane testi yap.
- *X-Plane testi:* KSEA, KBFI ve iş bilgisayarında kurulu başka büyük
  havalimanları (örnek: LTFM), zoom 12–16:
  - apronlar pist kenarlarıyla örtüşüyor,
  - çim adaları (delikler) boş,
  - kavisler doğru yöne,
  - sarı çizgiler taksi yollarının ortasında.

  X-Plane'in kendi haritası ve 3D görüntüsüyle karşılaştır. FPS'i (teşhis
  modu) ve Log'daki üçgenleme hatası sayısını not et.

---

## C. Harita modları (sürüm 0.11)

### C.0 Ne isteniyor

Kullanıcı: "Harita X-Plane'in mapinden farklı. Bu harita da güzel ama harita
modlarını da değiştirebilmem gerekiyor."

X-Plane 11 haritasında açılır listeden seçilen üç mod var: VFR Sectional, IFR
Low Enroute ve IFR High Enroute. Bizde dört mod olacak:

| Mod | Ayar değeri | Görünüm |
|---|---|---|
| Scenery | `scenery` | Bugünkü görünüm (arazi sınıfları + kabartma); X-Plane'de yok |
| VFR | `vfr` | X-Plane VFR Sectional'a benzer |
| IFR Low | `ifr_low` | X-Plane IFR Low Enroute'a benzer |
| IFR High | `ifr_high` | X-Plane IFR High Enroute'a benzer |

### C.1 Önce referans görüntüleri

- Renkleri tahmin etme.
- X-Plane'in haritasında her moddan aynı merkez ve zoom'da ekran görüntüsü al:
  - KSEA geniş (zoom ~9),
  - KSEA yakın (~12),
  - Rainier (dağlık),
  - Pasifik kıyısı.
- Renkleri görüntüden örnekle (örnek: `magick x.png -crop 1x1+X+Y txt:`).
- Görüntüler sadece referanstır; proje klasörüne konmaz.

### C.2 Tasarım

- **`src/MapTheme.h/.cpp`** (CORE, X-Plane'siz):
  - `enum class MapMode { Scenery, Vfr, IfrLow, IfrHigh }`
  - `struct MapTheme`: Katmanlarda bugün dosya içi `const Rgba k...` olarak
    duran bütün renkleri, kalınlıkları ve zoom eşiklerini gruplar halinde
    toplar. Gruplar: arka plan, ızgara, havalimanı, pist, havalimanı yüzeyi,
    ILS, alçak/yüksek hava yolu, VOR/NDB/fix, etiket, zemin stili.
  - `const MapTheme& ThemeFor(MapMode pMode)`
- **DrawContext:** `const MapTheme* mTheme` eklenir; katmanlar renkleri
  temadan okur.
- **İlk adım görünümü değiştirmez:** Scenery teması bugünkü sabitlerin birebir
  aynısıdır. A.5'teki önizleme görüntüleri piksel piksel aynı kalmalı.
- **Mod başına katman görünürlüğü:**
  - Ayar bölümleri: `[layers.scenery]`, `[layers.vfr]`, `[layers.ifr_low]`,
    `[layers.ifr_high]`.
  - Varsayılanlar temadan gelir (örnek: IFR High'da `airways_low` kapalı).
  - Eski `[layers]` bölümü ilk açılışta `[layers.scenery]`'ye taşınır.
  - Seçili mod `[view] mode` anahtarında saklanır.
- **Arayüz:**
  - Araç şeridinde "Layers"ın yanına "Mode: VFR" düğmesi gelir; tıklayınca
    LayerPanel'e benzer küçük bir liste açılır (tek seçim).
  - Komut `ozdev/map/next_mode` eklenir (tuşa atanabilsin).
  - Önizleme aracına `--mode vfr` seçeneği eklenir.

### C.3 VFR

- **Rakıma göre zemin renkleri:**
  - `TerrainRaster`'a rakım ızgarası eklenir (örnek: 1024², int16 metre).
    DEM zaten okunuyor.
  - Önbellek dosya sürümü `kFileVersion` 1'den 2'ye çıkar
    (`src/data/TerrainRaster.cpp:15`). Sürümü tutmayan eski önbellekler
    kendiliğinden yeniden üretilmeli. DOĞRULA: Load false döndüğünde yeniden
    üretim yolu çalışıyor mu.
  - Görüntü üretimi stil alır:
    - `Classes`: bugünkü görünüm.
    - `ElevationTint`: su sınıfındaki piksel su rengi; değilse rakım
      rampasının rengi × kabartma.
  - Rampa `terrain_colors.ini`'de `[elevation]` bölümünde: `<feet> = R G B`
    satırları. Renkler VFR referans görüntüsünden.
  - Mod değişince TerrainLayer yüklü dokuları bırakır (doku anahtarına stil
    eklenir). Raster önbelleği aynı kalır.
- **MEF rakamları** (X-Plane'deki büyük mavi sayılar, örnek "149"):
  - Her 30'×30' hücre için: en yüksek rakım + pay, 100 ft'e yukarı yuvarlanır,
    yüzlerce feet olarak yazılır.
  - Hücre maksimumları raster üretilirken hesaplanıp önbelleğe yazılır.
  - DOĞRULA: Kuralı X-Plane'in aynı hücrelerdeki rakamlarıyla karşılaştır;
    payı (100/200/300 ft) değerler eşleşene kadar ayarla.
- **Havalimanları:**
  - Kuleli havalimanı mavi, kulesiz macenta (sectional kuralı).
  - Kule bilgisi motordan gelir (`mHasTower` ya da kule frekansı).
  - Motorda olmayan (sadece nav verisindeki) havalimanı macenta çizilir.
- **VOR pusula gülü:**
  - 10° çentikli çember, VOR'un manyetik sapmasıyla döndürülmüş.
  - SDK sapmayı vermiyor. `earth_nav.dat` VOR satırlarındaki "slaved
    variation" alanından okunur; hava yollarındaki gibi Custom Data önce.
  - DOĞRULA: Alan sırasını gerçek bir satırdan kontrol et.
- **Victor hava yolları:** açık mavi, kalın, yarı saydam.
- **ILS tüyleri:** varsayılan kapalı (X-Plane VFR modunda yok).

### C.4 IFR Low / IFR High

- **Zemin:** Yok; düz arka plan rengi (referans görüntüden).
- **IFR Low:** alçak hava yolları, VOR, NDB, fix (zoom ≥ 8), havalimanları.
- **IFR High:**
  - yüksek hava yolları ve VOR,
  - sadece büyük havalimanları: pist verisi varsa pist ≥ 1500 m, yoksa 4
    harfli ICAO,
  - NDB yok.
- **Renk ve kalınlıklar:** referans görüntülerden.

### C.5 Adımlar ve kabul

**C1 — Tema altyapısı.** MapTheme + DrawContext; katmanlar renkleri
temadan okur. Scenery bugünküyle aynı: önizleme farkı sıfır.

**C2 — Mod seçimi.** Mod seçici, mod başına görünürlük, eski ayarın
taşınması, komut.

**C3 — IFR Low/High temaları.** Zemin olmadığı için en kolayı.

**C4 — VFR.** Rakım ızgarası + önbellek sürümü + rampa + MEF + VOR gülü +
renkler.

**Kabul:**
- Mod değişimi anında olur. VFR'ye ilk geçişte zemin dokularının birkaç
  saniyede yenilenmesi kabul.
- Her mod, X-Plane'in aynı modunun referans görüntüsüyle yan yana konunca
  açıkça benzer.
- Ayarlar mod başına kalıcı.
- FPS kötüleşmiyor.

---

## D. Her adımın sonunda

1. Derle: /W4 uyarısız; `map_tests` geçiyor; yeni kod için test eklendi.
2. Kod standardı taraması (CLAUDE.md kuralları): satır sonunda açılan `{`,
   öneksiz değişken, 120 karakteri aşan satır.
3. Önizleme görüntüleri ve karşılaştırma (A.5).
4. X-Plane testi (adımdaki liste). Sonucu kullanıcıya göster.
5. README.md (Türkçe: katmanlar, kullanım, test listesi) ve CLAUDE.md
   (mimari, durum, sıradaki) güncellenir; `Plugin.cpp`'deki `kVersion` da.
6. Adımın sonunda yapılanları kısa bir listeyle `CHANGES_0.9.md`'ye yaz
   (kullanıcı değişiklikleri kendi bilgisayarına bununla taşıyacak; motor
   dosyalarını ayrı belirt).

## E. Bu turda kapsam dışı

- XP12'ye özel API. Mac/Linux testi: Mac'te X-Plane'in eklentiye verdiği GL
  bağlamı farklı olabilir; bu paketler Windows hedefli.
- Taksi ağı ve taksi yolu adları (1201–1204), tabelalar (20), binalar, 3D
  objeler.
- Kendi yazı tipi atlası: `XPLMDrawString` kalıyor.

## F. Takılırsan kullanıcıya sorulacaklar (varsayılanlar parantezde)

- earcut eklemek şirket politikasına uygun mu? (Uygun değilse B.4 seçenek 2.)
- Mod seçici şeritte düğme olarak mı dursun, liste olarak mı? (Varsayılan:
  "Mode: X" düğmesi + açılır liste.)
- Havalimanı yüzeyi renkleri X-Plane'e mi benzesin, bizim koyu temaya mı?
  (Varsayılan: Scenery modunda bizim tema, VFR/IFR modlarında X-Plane'e
  benzer.)
