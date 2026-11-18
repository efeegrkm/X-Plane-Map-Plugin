# PrimeMap_API.html ureticisi: belge metni burada, kod ornekleri derlenen dosyalardan (examples/texture_host)
# okunur ve HTML'e kacirilarak girer. Kullanim: python docs/tools/build_api_doc.py  (prime_map_api kokunden)
# Sonra PDF: chrome --headless=new --disable-gpu --no-pdf-header-footer --print-to-pdf=docs\PrimeMap_API.pdf
#            docs\PrimeMap_API.html
import html
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
OUT = os.path.join(ROOT, 'docs', 'PrimeMap_API.html')
VERSION = '1.0.0'
DATE = '2026-11-18'


def read(rel):
    with open(os.path.join(ROOT, rel), encoding='utf-8') as f:
        return f.read().replace('\r\n', '\n')


def pre(code):
    return '<pre>' + html.escape(code.strip('\n')) + '</pre>\n'


def table(header, rows, cls=None):
    out = ['<table>' if not cls else f'<table class="{cls}">', '<tr>' + ''.join(f'<th>{h}</th>' for h in header) + '</tr>']
    for r in rows:
        if isinstance(r, str):
            out.append(f'<tr class="group"><td colspan="{len(header)}">{r}</td></tr>')
        else:
            out.append('<tr>' + ''.join(f'<td>{c}</td>' for c in r) + '</tr>')
    out.append('</table>')
    return '\n'.join(out) + '\n'


def c(text):
    return '<code>' + html.escape(text) + '</code>'


# --------------------------------------------------------------------------------------------------
# Katman tablosu: enum, anahtar, panel adi, yerlesik esik, varsayilan (S V L H D), renk verilebilir mi
# --------------------------------------------------------------------------------------------------
ON, OFF = '&#9679;', '&#9675;'


def dots(s):
    return '<span class="dots">' + ''.join(ON if ch == '1' else OFF for ch in s) + '</span>'


LAYERS = [
    'XMap &mdash; <code>XMapLayer::XMapGroup</code> (grup)',
    ('Terrain', 'terrain', 'Terrain (scenery)', '5 (sert 4)', '11111', 'zemin: <code>TerrainSettings</code>'),
    ('TerrainForests', 'terrain.forests', 'Forests', '', '01110', '&mdash;'),
    ('TerrainBuiltUp', 'terrain.built_up', 'Built-up areas', '', '11000', 'evet (VFR/IFR yerleşim karışımı)'),
    ('Roads', 'roads', 'Roads and railways', '7.5', '00000', '&mdash; (alt katmanlar)'),
    ('RoadsMotorways', 'roads.motorways', 'Motorways', '7.5', '01110', 'evet'),
    ('RoadsPrimary', 'roads.primary', 'Primary roads', '9 (sert 8)', '01110', 'evet'),
    ('RoadsSecondary', 'roads.secondary', 'Secondary roads', '10.5 (sert 9.5)', '01110', 'evet'),
    ('RoadsLocal', 'roads.local', 'Local roads', '12 (sert 11)', '01110', 'evet'),
    ('RoadsRailways', 'roads.railways', 'Railways', '9.5 (sert 8.5)', '01110', 'evet'),
    ('RoadsPowerLines', 'roads.power_lines', 'Power lines', '11 (sert 10)', '01000', 'evet'),
    ('Grid', 'grid', 'Lat / lon grid', 'her zoom', '11110', 'evet (çizgi)'),
    'Airports &mdash; <code>XMapLayer::AirportsGroup</code> (grup)',
    ('AirportSymbols', 'airports', 'Airport symbols', '4 (büyük 4, küçük 8, deniz üssü 9, heliport / kapalı 11)', '11111', 'hayır'),
    ('Runways', 'runways', 'Runways', '11 (sert 10)', '11111', 'evet (pist kenarı)'),
    ('RunwaysPaved', 'runways.paved', 'Paved (land)', '', '11111', '&mdash;'),
    ('RunwaysUnpaved', 'runways.unpaved', 'Unpaved (land)', '', '11111', '&mdash;'),
    ('RunwaysWater', 'runways.water', 'Water runways', '', '11111', '&mdash;'),
    ('RunwaysOverruns', 'runways.overruns', 'Blast pads / overruns', '12', '11111', '&mdash;'),
    ('RunwaysMarkings', 'runways.markings', 'Markings', '13', '11111', '&mdash;'),
    ('RunwaysNumbers', 'runways.numbers', 'Runway numbers', '11.5', '11111', '&mdash;'),
    ('Surfaces', 'airport_surfaces', 'Airport surfaces', '10.5 (sert 9.5)', '11111', 'hayır'),
    ('SurfacesPavement', 'airport_surfaces.pavement', 'Pavement', '11 (sert 10.5)', '11111', 'hayır'),
    ('SurfacesPavementEdges', 'airport_surfaces.pavement_edges', 'Pavement edges', '', '10111', 'hayır'),
    ('SurfacesBoundary', 'airport_surfaces.boundary', 'Airport boundary', '10.5 (sert 9.5)', '11110', 'hayır'),
    ('SurfacesMarkings', 'airport_surfaces.markings', 'Painted lines', '13.5 (sert 12.5)', '00000', 'hayır'),
    ('Objects', 'airport_objects', 'Airport objects', '12.5 (sert 11.5)', '01010', 'hayır'),
    ('ObjectsLighting', 'airport_objects.lighting', 'Edge / taxi lights', '14.5 (sert 13.5)', '00000', 'hayır'),
    ('ObjectsHelipads', 'airport_objects.helipads', 'Helipads', '12.5 (sert 11.5)', '01010', 'hayır'),
    ('ObjectsBeacons', 'airport_objects.beacons', 'Beacons', '12.5 (sert 11.5)', '00000', 'hayır'),
    ('ObjectsWindsocks', 'airport_objects.windsocks', 'Windsocks', '13.5 (sert 12.5)', '00000', 'hayır'),
    ('ObjectsGates', 'airport_objects.gates', 'Gates', '14 (sert 13)', '00000', 'hayır'),
    ('ObjectsTieDowns', 'airport_objects.tie_downs', 'Tie-downs', '14 (sert 13)', '00000', 'hayır'),
    ('ObjectsHangars', 'airport_objects.hangars', 'Hangars', '14 (sert 13)', '00000', 'hayır'),
    ('ObjectsOtherStarts', 'airport_objects.other_starts', 'Other start locations', '14 (sert 13)', '00000', 'hayır'),
    ('ObjectsStartNames', 'airport_objects.start_names', 'Parking names', '15.5 (sert 14.5)', '00000', 'hayır'),
    ('ObjectsApproachLights', 'airport_objects.approach_lights', 'PAPI / VASI', '14.5 (sert 13.5)', '00000', 'hayır'),
    ('ObjectsSigns', 'airport_objects.signs', 'Taxi signs', '15.5 (sert 14.5)', '00000', 'hayır'),
    ('TaxiRoutes', 'taxi_routes', 'Taxi routes', '13.5 (sert 12.5)', '00000', 'hayır'),
    'Navaids &mdash; <code>XMapLayer::NavaidsGroup</code> (grup)',
    ('Vor', 'vor', 'VOR / DME', '5', '11111', 'evet'),
    ('Ndb', 'ndb', 'NDB', '7', '11111', 'evet'),
    ('Ils', 'ils', 'ILS / LOC', '8.5', '00001', 'evet'),
    ('Fixes', 'fix', 'Fixes', '9', '00011', 'evet'),
    'Airways &mdash; <code>XMapLayer::AirwaysGroup</code> (grup)',
    ('AirwaysLow', 'airways_low', 'Airways (low)', '6.5 (sert 5; adlar 8, oklar 9)', '11101', 'evet'),
    ('AirwaysHigh', 'airways_high', 'Airways (high)', '6.5 (sert 5; adlar 8, oklar 9)', '10011', 'evet'),
    'Flight &mdash; <code>XMapLayer::FlightGroup</code> (grup)',
    ('Aircraft', 'aircraft', 'Aircraft', 'her zoom', '11111', 'hayır'),
    ('FmsRoute', 'fms', 'FMS route', 'her zoom', '11111', 'hayır'),
    ('FlightTrack', 'track', 'Flight track', 'her zoom', '11111', 'hayır'),
    'Grupsuz',
    ('Drawings', 'drawings', 'User drawings', 'her zoom', '11111', 'hayır'),
    ('DrawingsPoints', 'drawings.points', 'Points', '', '11111', 'hayır'),
    ('DrawingsRoutes', 'drawings.routes', 'Routes', '', '11111', 'hayır'),
    ('DrawingsRegions', 'drawings.regions', 'Regions', '', '11111', 'hayır'),
    ('DrawingsLabels', 'drawings.labels', 'Names and distances', '', '11111', 'hayır'),
]
SUBS = {'TerrainForests', 'TerrainBuiltUp'} | {n for n in
        ['RoadsMotorways', 'RoadsPrimary', 'RoadsSecondary', 'RoadsLocal', 'RoadsRailways', 'RoadsPowerLines',
         'RunwaysPaved', 'RunwaysUnpaved', 'RunwaysWater', 'RunwaysOverruns', 'RunwaysMarkings', 'RunwaysNumbers',
         'SurfacesPavement', 'SurfacesPavementEdges', 'SurfacesBoundary', 'SurfacesMarkings', 'ObjectsLighting',
         'ObjectsHelipads', 'ObjectsBeacons', 'ObjectsWindsocks', 'ObjectsGates', 'ObjectsTieDowns', 'ObjectsHangars',
         'ObjectsOtherStarts', 'ObjectsStartNames', 'ObjectsApproachLights', 'ObjectsSigns', 'DrawingsPoints',
         'DrawingsRoutes', 'DrawingsRegions', 'DrawingsLabels']}


def layer_rows():
    rows = []
    for e in LAYERS:
        if isinstance(e, str):
            rows.append(e)
            continue
        name, key, panel, zoom, defaults, color = e
        label = ('&nbsp;&nbsp;&#8627; ' if name in SUBS else '') + c(name)
        rows.append([label, c(key), panel, zoom, dots(defaults), color])
    return rows


H = []
add = H.append

add(f'''<!doctype html>
<html lang="tr">
<head>
<meta charset="utf-8">
<title>Prime XMap API {VERSION} Referansı</title>
<style>
  body {{ font-family: "Segoe UI", Arial, sans-serif; font-size: 10pt; line-height: 1.4; color: #1d2329; margin: 26px 36px; }}
  h1 {{ font-size: 20pt; margin: 0; color: #0b3d5c; }}
  h2 {{ font-size: 13.5pt; color: #0b3d5c; border-bottom: 2px solid #0b3d5c; padding-bottom: 2px; margin-top: 22px; page-break-after: avoid; }}
  h3 {{ font-size: 11pt; color: #0b3d5c; margin: 14px 0 4px; page-break-after: avoid; }}
  code, pre {{ font-family: Consolas, monospace; font-size: 8.6pt; }}
  pre {{ background: #f2f5f8; border: 1px solid #d5dde5; padding: 7px 9px; white-space: pre-wrap; }}
  table {{ border-collapse: collapse; width: 100%; margin: 6px 0 10px; font-size: 8.8pt; }}
  th, td {{ border: 1px solid #c8d1da; padding: 3px 5px; vertical-align: top; text-align: left; }}
  th {{ background: #e4ecf3; }}
  tr {{ page-break-inside: avoid; }}
  tr.group td {{ background: #0b3d5c; color: #fff; font-weight: bold; }}
  .sub {{ color: #555; margin-top: 2px; }}
  .note {{ background: #fff7e0; border: 1px solid #e3c96b; padding: 6px 9px; margin: 8px 0; page-break-inside: avoid; }}
  .ok {{ color: #1b6e2a; font-weight: bold; }} .part {{ color: #9a6a00; font-weight: bold; }} .no {{ color: #a3262a; font-weight: bold; }}
  .dots {{ letter-spacing: 3px; }}
  ol.toc {{ columns: 2; font-size: 9.5pt; }}
</style>
</head>
<body>

<h1>Prime XMap API {VERSION} &mdash; Kullanım Kılavuzu ve Referans</h1>
<div class="sub">Herhangi bir X-Plane 11 eklentisine gömülebilen katmanlı harita kütüphanesi &middot; {DATE}</div>

<ol class="toc">
<li>Tanım ve ne sunar</li>
<li>Hızlı başlangıç (10 dakika)</li>
<li>Sıfırdan entegrasyon (adım adım)</li>
<li>Kavramlar: yaşam döngüsü, görüntüleme yolları, dosyalar</li>
<li>Koordinatlar ve birimler</li>
<li>XMapManagerFactory ve XMapManager</li>
<li>XMap: bütün fonksiyonlar</li>
<li>Doku modu (WindowStyle::Texture) ayrıntıları</li>
<li>Özelleştirme: neyi ne kadar kontrol edebilirsiniz</li>
<li>Katmanlar: tam liste</li>
<li>Ayar dosyaları</li>
<li>Tipler</li>
<li>Tarifler</li>
<li>Örnek projeler</li>
<li>Sınırlar ve bilinen hatalar</li>
<li>Sürüm ve geçiş</li>
</ol>
''')

# 1 -------------------------------------------------------------------------------------------------
add('<h2>1. Tanım ve ne sunar</h2>')
add('''<p>Prime XMap API, bir X-Plane 11 eklentisinin (host) kendi arayüzüne katmanlı, hareketli bir harita koymasını sağlayan
C++ kütüphanesidir (<code>prime_map.dll</code>). Harita verisini sadece o bilgisayardaki X-Plane kurulumundan alır: DSF sahneleri
(zemin, su, kabartma, yollar), apt.dat (havalimanları), nav veritabanı, <code>earth_awy.dat</code> (hava yolları), dataref'ler
(uçak) ve FMS (rota). Dış servis, internet ya da ek veri yoktur; simülatörde ne kuruluysa haritada o görünür.</p>''')
add(table(['Özellik', 'Değer'], [
    ['Platform', 'X-Plane 11 (11.50+), Windows x64, X-Plane SDK 3.0 (XPLM301). Host her zaman bir X-Plane eklentisidir.'],
    ['Dil / derleyici', 'C++20, MSVC 2022, CRT <code>/MD</code> (host da <code>/MD</code> ile derlenmek zorunda)'],
    ['Başlık / ad alanı', '<code>#include &lt;prime_map_api/PrimeMap.h&gt;</code> &middot; <code>namespace prime</code>'],
    ['CMake hedefi', '<code>prime-map-api</code> (çıktı <code>prime_map.dll</code> + <code>prime_map.lib</code>)'],
    ['Giriş noktası', '<code>prime::XMapManagerFactory::createXMapManager()</code> &rarr; <code>XMapManager</code> &rarr; <code>addXMap()</code> &rarr; <code>XMap</code>'],
    ['Görüntüleme yolu', '<b>Texture</b>: harita host\'un istediği yere basılan bir openGL dokusu (framebuffer). API\'nin hedef ve tek önerilen yoludur. <b>Standalone</b> ve <b>Embedded</b> pencere stilleri <b>LEGACY</b>\'dir (bölüm 4.3): kodları korunur, yeni host kodunda kullanılmaz.'],
    ['Harita sayısı', 'Sınırsız; her harita tamamen bağımsız (görünüm, mod, katmanlar, renkler, çizimler, zemin ayarları, dosyalar). Veri ve önbellek bir yöneticinin haritaları arasında ortak.'],
    ['Thread', 'Bütün fonksiyonlar X-Plane ana thread\'inden çağrılır. Veri yükleme ve zemin üretimi kütüphanenin kendi arka plan thread\'lerindedir.'],
    ['Hata modeli', 'İstisna yok. Geçersiz değer: işlem yapılmaz, Log.txt\'ye tek satır <code>WARNING</code>, dönüş <code>false</code> / boş / varsayılan.'],
]))
add('''<p><b>Özetle API ile yapabilecekleriniz</b> (ayrıntı ve sınırlar bölüm 9'da): haritayı istediğiniz yere ve boyuta koymak;
birden çok bağımsız harita; 5 harita modu (Scenery, VFR, IFR Low, IFR High, Dark); 56 katman / alt katmanın her birini mod başına
açıp kapamak; katmanlara zoom aralığı vermek; 14 katmanın rengini değiştirmek; zeminin bütün renklerini, kabartmasını, suyunu,
VFR yükseklik bantlarını ayarlamak; uçak ve rotayı X-Plane'den ya da host'tan almak; çizim (nokta / rota / bölge) eklemek,
okumak, kaydetmek; ekran &harr; coğrafya dönüşümü ve imleç altındaki nesneyi sorgulamak; ayarları metin olarak dışa / içe
aktarmak; zemin önbelleğini önceden hazırlamak.</p>''')

# 2 -------------------------------------------------------------------------------------------------
add('<h2>2. Hızlı başlangıç (10 dakika)</h2>')
add('''<p>Zaten derlenen bir X-Plane eklentiniz varsa haritayı kendi pencerenizin içine koymak için gereken her şey bu
bölümdedir. Hiç eklentiniz yoksa bölüm 3'ten başlayın (tam, derlenen bir örnek proje verir).</p>
<h3>2.1 CMake'e üç satır</h3>''')
add(pre('''add_subdirectory(${CMAKE_SOURCE_DIR}/../prime_map_api prime_map_build)   # kutuphane ayni oturumda derlenir
target_link_libraries(my_plugin PRIVATE prime-map-api)                    # include dizini ve /MD gelir
add_custom_command(TARGET my_plugin POST_BUILD                            # prime_map.dll win.xpl'nin yanina
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:prime-map-api> $<TARGET_FILE_DIR:my_plugin>)'''))
add('<h3>2.2 Açılış (XPluginEnable)</h3>')
add(pre('''#include <prime_map_api/PrimeMap.h>

prime::XMapManager* gManager = prime::XMapManagerFactory::createXMapManager();
gManager->setDataDirectory(myPluginFolder);       // ayar + onbellek klasoru (bos: %LOCALAPPDATA%/prime/prime-xmap)
gManager->initialize();                           // nav / havalimani / zemin indeksi arka planda yuklenir

prime::XMapConfig tConfig;
tConfig.mName = "main";                           // ayarlar <veri>/maps/main/
tConfig.mStyle = prime::WindowStyle::Texture;     // pencere yok: harita bir dokuya cizilir
tConfig.mUiVisible = true;                        // haritanin kendi seritleri / panelleri
tConfig.mMouseInput = true;
prime::XMap* gMap = gManager->addXMap(tConfig);
gMap->setTextureSize(600, 400);                   // boxel
gMap->initialize();
gMap->setMode(prime::XMapMode::Vfr);              // istediginiz kadar ozellestirme (bolum 9)'''))
add('<h3>2.3 Çizim (pencerenizin draw callback\'i)</h3>')
add(pre('''gMap->setTextureSize(mapWidth, mapHeight);        // pencere boyutlaninca; ayni boyutta is yapmaz
if (gMap->renderTexture())                        // gerekiyorsa yeniden cizer
{
    XPLMSetGraphicsState(0, 1, 0, 0, 0, 0, 0);
    XPLMBindTexture2d(static_cast<int>(gMap->getTextureId()), 0);
    // (mapLeft, mapBottom) - (mapLeft + mapWidth, mapBottom + mapHeight) dortgenine bas; doku (0,0) sol-alt
}'''))
add('<h3>2.4 Girdi (click / cursor / wheel / key callback\'leri)</h3>')
add(pre('''gMap->injectMouse(x - mapLeft, y - mapBottom, prime::MouseAction::Down);   // Drag, Up; MouseButton::Right
gMap->injectMouseMove(x - mapLeft, y - mapBottom);                         // cursor callback: her karede
gMap->injectWheel(x - mapLeft, y - mapBottom, clicks);
if (gMap->wantsKeyboard()) { XPLMTakeKeyboardFocus(myWindow); }            // tuslar: injectKey(...)'''))
add('<h3>2.5 Kapanış (XPluginDisable)</h3>')
add(pre('''gManager->removeXMap(gMap);                        // ayarlar yazilir
gManager->shutdown();
prime::XMapManagerFactory::destroyXMapManager(gManager);'''))
add('<p>Kurulum: <code>&lt;X-Plane&gt;/Resources/plugins/&lt;eklentiniz&gt;/64/</code> altında <b>win.xpl ve prime_map.dll yan yana</b>. Log.txt\'de <code>Prime Map API 1.0.0 ready (... dll: ...)</code> satırını görün — satır yüklü DLL\'in yolunu da yazar (bölüm 16).</p>')

# 3 -------------------------------------------------------------------------------------------------
add('<h2>3. Sıfırdan entegrasyon (adım adım)</h2>')
add('''<p>Hiç eklentisi olmayan bir bilgisayardan, içinde harita olan çalışan bir X-Plane eklentisine kadar. Buradaki kod
depodaki <code>prime_map_api/examples/texture_host</code> projesinin <b>kendisidir</b> (derlenir, uyarısız).</p>
<h3>Adım 0 &mdash; Ön koşullar</h3>''')
add(table(['Gereken', 'Not'], [
    ['X-Plane 11 (11.50+), Windows x64', 'Varsayılan klasör <code>C:\\X-Plane 11</code>'],
    ['Visual Studio 2022 ("Desktop development with C++", MSVC v143)', 'C++20; <code>vcvars64.bat</code> ortamı'],
    ['CMake 3.16+ ve Ninja', 'Qt Creator kuruluysa <code>C:\\Qt\\Tools\\CMake_64</code> ve <code>C:\\Qt\\Tools\\Ninja</code> hazırdır (Qt kütüphanesi gerekmez)'],
    ['Üçüncü taraf bileşenler', 'Adım 1 (repoda tutulmaz)'],
]))
add('<h3>Adım 1 &mdash; Üçüncü taraf bileşenler (COTS)</h3>')
add('<p>Tek klasöre açın. CMake sırayla <code>-DCOTS_DIR=&lt;klasör&gt;</code>, <code>C:\\workspace\\cots</code> ve <code>prime_map_api</code>\'nin kardeşi <code>cots</code> klasörüne bakar.</p>')
add(table(['Bileşen', 'Sürüm', 'İndirme', 'Klasör (içinde ne olmalı)', 'Kim için'], [
    ['X-Plane SDK', '3.0 (XPLM301)', 'developer.x-plane.com/sdk', '<code>cots/X-Plane-SDK</code> (<code>CHeaders/XPLM/*.h</code>, <code>Libraries/Win/XPLM_64.lib</code>)', 'kütüphane + host'],
    ['LZMA SDK', '24.05', '7-zip.org/sdk.html', '<code>cots/lzma</code> (SDK\'nın <code>C/</code> klasöründeki <code>*.c</code>, <code>*.h</code> doğrudan burada)', 'sadece kütüphane'],
    ['earcut.hpp', '2.2.4', 'github.com/mapbox/earcut.hpp', '<code>cots/earcut/mapbox/earcut.hpp</code>', 'sadece kütüphane'],
    ['stb_truetype.h', '1.26', 'github.com/nothings/stb', '<code>cots/stb/stb_truetype.h</code>', 'sadece kütüphane'],
]))
add('<h3>Adım 2 &mdash; Klasör yerleşimi</h3>')
add(pre('''<calisma klasoru>/
  cots/                 Adim 1
  prime_map_api/        kutuphane (klasor adi include onekidir: degistirmeyin)
  my_plugin/            sizin eklentiniz
    CMakeLists.txt      Adim 3
    src/MyPlugin.cpp    Adim 4'''))
add(pre('''git clone https://gobitbucket.havelsan.com.tr/scm/eps/prime-map-api.git
rem depodaki prime_map_api klasorunu kendi eklentinizin yanina kopyalayin (ya da depoyu calisma klasoru yapin)'''))
add('<h3>Adım 3 &mdash; <code>CMakeLists.txt</code> (tamamı)</h3>')
add('<p>Kendi projenizde tek fark: <code>PRIME_MAP_DIR</code>\'in varsayılanı <code>${CMAKE_CURRENT_SOURCE_DIR}/../prime_map_api</code> olur ve proje adını değiştirirsiniz.</p>')
add(pre(read('examples/texture_host/CMakeLists.txt')))
add('<h3>Adım 4 &mdash; Eklenti kaynağı (tamamı)</h3>')
add('''<p>Bir X-Plane penceresi açar (taşınır, boyutlanır), haritayı içine doku olarak basar, fare / tekerlek / klavyeyi
haritaya iletir ve haritayı özelleştirir. Bütün harita işi <code>enable</code>, <code>customizeMap</code>, <code>draw</code>,
<code>forwardClick</code> ve callback'lerdedir.</p>''')
add(pre(read('examples/texture_host/src/TextureHost.cpp')))
add('<h3>Adım 5 &mdash; Derleme</h3>')
add(pre('''call "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat"
cmake -S my_plugin -B build_my_plugin -G Ninja -DCMAKE_BUILD_TYPE=Release -DMAP_DEPLOY=ON
ninja -C build_my_plugin'''))
add('<p>Seçenekler: <code>-DCOTS_DIR=</code>, <code>-DXPSDK=</code>, <code>-DPRIME_MAP_DIR=</code>, <code>-DXPLANE_DIR=</code>, <code>-DMAP_DEPLOY=ON|OFF</code>. Qt Creator\'da <code>CMakeLists.txt</code>\'yi açıp derlemek de yeter.</p>')
add('<h3>Adım 6 &mdash; X-Plane\'e kurulum</h3>')
add(pre('''<X-Plane 11>/Resources/plugins/my-plugin/
  64/win.xpl          eklentiniz
  64/prime_map.dll    kutuphane (win.xpl ile AYNI klasorde olmak zorunda)'''))
add('<p><code>MAP_DEPLOY=ON</code> bunu derleme sonunda yapar. X-Plane açıkken dosyalar kilitlidir: önce kapatın. Başka bir bilgisayarda <code>VC_redist.x64</code> (MSVC 2022) gerekir.</p>')
add('<h3>Adım 7 &mdash; Doğrulama</h3>')
add('<p><code>&lt;X-Plane 11&gt;/Log.txt</code> içinde sırayla (önek <code>setLogPrefix</code> ile verdiğiniz):</p>')
add(pre('''[texture-host] central settings created: ...\\plugins\\texture-host\\prime_map.json
[texture-host] cache limits for 0 open map(s): terrain disk 1024 MB, terrain textures 512 MB, memory scale 1.00
[texture-host] Prime Map API 1.0.0 ready (data: ...\\plugins\\texture-host\\, cache: ...\\texture-host\\cache/, dll: ...\\plugins\\texture-host\\64\\prime_map.dll)
[texture-host] map 1 'main': no settings file yet, using the values given through the API
[texture-host] map texture created (584 x 534 boxels)
[texture-host] map 1 'main' opened (settings: ...\\texture-host\\maps/main/)
[texture-host] openGL ready: 4.6.0 ...
[texture-host] text: font atlas from ...\\Resources/fonts/DejaVuSans.ttf (cap height 10 boxels)'''))
add('<p>İlk saniyelerde veri yüklenir (havalimanı indeksi ilk açılışta 10&ndash;30 s sürebilir, sonra önbellekten); zemin zoom 5\'ten itibaren ve sadece kurulu sahnede görünür. Eklenti klasöründe <code>prime_map.json</code>, <code>maps/main/</code> ve <code>cache/</code> oluşur.</p>')
add('<h3>Sorun giderme</h3>')
add(table(['Belirti', 'Neden / çözüm'], [
    ['CMake: <code>X-Plane SDK bulunamadi</code>', 'Adım 1 eksik ya da <code>-DCOTS_DIR</code> / <code>-DXPSDK</code> yanlış'],
    ['CMake: <code>prime_map_api bulunamadi</code>', '<code>-DPRIME_MAP_DIR</code> verin; include <code>&lt;prime_map_api/PrimeMap.h&gt;</code> biçiminde olmalı'],
    ['CMake: <code>Surum uyusmazligi</code>', 'Kütüphaneyi değiştirdiyseniz <code>src/api/XMapTypes.h</code> <code>kVersion*</code> ile <code>project(VERSION)</code>\'ı birlikte güncelleyin'],
    ['Link: <code>unresolved external</code> (prime:: sembolleri) ya da tuhaf çökmeler', 'Host <code>/MT</code> ile derleniyor: <code>CMAKE_MSVC_RUNTIME_LIBRARY</code> satırını ekleyin (<code>/MD</code>)'],
    ['X-Plane eklentiyi yüklemiyor (Log: <code>could not be loaded</code>, hata 126)', '<code>prime_map.dll</code> <code>64/</code>\'da yok ya da <code>VC_redist.x64</code> kurulu değil'],
    ['İki eklenti aynı süreçte farklı <code>prime_map.dll</code> sürümü kullanıyor', 'Windows aynı adlı DLL\'i bir kez yükler: bütün host\'lara <b>aynı</b> <code>prime_map.dll</code>\'yi koyun. Uyuşmazlıkta fabrika tek satır WARNING yazar: <i>host built with X, loaded DLL is Y at ...</i>; <code>isLibraryVersionMatching()</code> ile kendiniz de denetleyebilirsiniz'],
    ['<code>addXMap</code> <code>nullptr</code> döndü', '<code>XMapManager::initialize()</code> çağrılmadı (Log: <code>addXMap before initialize</code>)'],
    ['Doku siyah / boş', 'İlk saniyelerde veri yükleniyor (<code>getStatus()</code>); <code>renderTexture</code> host\'un draw callback\'i <b>içinde</b> çağrılmalı; <code>false</code> döndüyse basmayın'],
    ['Tıklama haritaya gitmiyor', '<code>mMouseInput = true</code> mı; koordinatı dokuya göre (sol-alt orijin) mi verdiniz; basma haritada başladıysa sürükleme / bırakmayı da iletin'],
    ['"Go to airport" kutusuna yazılamıyor', '<code>wantsKeyboard()</code> true iken <code>XPLMTakeKeyboardFocus(pencereniz)</code> ve tuşları <code>injectKey</code> ile iletin'],
    ['Kodda verdiğim ayar açılışta uygulanmıyor', 'Dosyadaki değer kazanır (bölüm 4.4): ayarı <code>initialize()</code>\'dan sonra verin ya da <code>Persistence::SaveOnly</code>'],
    ['Dosyalar başka eklentinin klasöründe / önbellek karışıyor', 'Her host <code>setDataDirectory</code> ile kendi klasörünü vermeli (varsayılan klasör bütün host\'larda aynıdır)'],
    ['Log satırları başka eklentinin önekiyle', 'Log öneki süreçte tektir: son <code>setLogPrefix</code> çağrısı geçerli (bölüm 15)'],
    ['Taze derlenmiş dll / exe siliniyor', 'Antivirüs yanlış alarmı: derleme klasörünü istisna yapın'],
]))

# 4 -------------------------------------------------------------------------------------------------
add('<h2>4. Kavramlar</h2>')
add('<h3>4.1 Nesneler ve sahiplik</h3>')
add(table(['Nesne', 'Kim oluşturur / bırakır', 'Ne tutar'], [
    ['<code>XMapManager</code>', '<code>XMapManagerFactory::createXMapManager()</code> / <code>destroyXMapManager()</code>. Her çağrı <b>yeni</b> yönetici verir (singleton yok).', 'Ortak veri (nav, havalimanları, hava yolları, zemin işçisi), önbellek, merkezi ayarlar, haritalar'],
    ['<code>XMap</code>', '<code>XMapManager::addXMap()</code> / <code>removeXMap()</code> (yönetici bırakılınca kalanlar da)', 'Bir haritanın her şeyi: görünüm, mod, katmanlar, renkler, çizimler, uçak / rota kaynağı, zemin ayarları, dosyalar'],
]))
add('<h3>4.2 Yaşam döngüsü</h3>')
add(table(['Durum', 'Geçiş', 'Ne yapılabilir'], [
    ['Yönetici başlatılmadı', '<code>createXMapManager()</code>', '<code>setDataDirectory</code>, <code>setCacheDirectory</code>, <code>setLogPrefix</code>, <code>setLogCallback</code>, <code>setTerrainCacheMb</code>, <code>setPrefetchTiles</code>, yol sorguları'],
    ['Yönetici çalışıyor', '<code>initialize()</code>', '<code>addXMap</code>, <code>getStatus</code>, <code>getSettings</code> / <code>setSettings</code> ...'],
    ['Harita <b>oluşturuldu</b> (çizilmiyor)', '<code>addXMap()</code>', 'Bütün <code>set...</code> / <code>get...</code>; değerler nesnede durur'],
    ['Harita <b>açık</b>', '<code>XMap::initialize()</code>', 'Her şey; değişiklik anında görünür, dosyaya yazılır'],
    ['Harita <b>kapalı</b> (nesne duruyor)', '<code>XMap::close()</code>', '"Oluşturuldu" ile aynı; <code>initialize()</code> ile aynı haliyle yeniden açılır'],
    ['Harita yok', '<code>removeXMap()</code>', 'Ayarlar yazılır, nesne silinir (işaretçi geçersiz)'],
]))
add('<div class="note"><b>Kural:</b> aksi yazılmadıkça her <code>set...</code> harita açılmadan önce de, açıkken de çalışır. İstisnalar: <code>setName</code> ve <code>setStyle</code> sadece kapalıyken; <code>queryAt...</code>, <code>bringToFront</code>, <code>setPoppedOut</code>, <code>renderTexture</code> sadece açıkken anlamlıdır.</div>')
add('<h3>4.3 Üç görüntüleme yolu (<code>WindowStyle</code>)</h3>')
add(table(['Stil', 'Durum', 'Nasıl'], [
    ['<b>Texture</b>', '<span class="ok">Hedef / önerilen</span>', 'XPLM penceresi yok. Harita kütüphanenin framebuffer\'ında bir openGL dokusuna çizilir; host kendi draw callback\'inde <code>renderTexture()</code> çağırır ve dokuyu istediği dörtgene basar (pencere, panel, kokpit ekranı). Girdi host\'tan <code>inject*</code> ile gelir. Konum eşleme, pencere sırası ve tıklama çakışması sorunu yoktur. <code>XMapConfig</code>\'in varsayılanı.'],
    ['<b>Standalone</b>', '<span class="part">LEGACY</span>', 'Haritanın kendi X-Plane penceresi (çerçeve, taşıma, boyutlama, pop-out). Resmî Prime XMap eklentisi hâlâ bunu kullanır.'],
    ['<b>Embedded</b>', '<span class="part">LEGACY</span>', 'Çerçevesiz ayrı bir XPLM penceresi; host her karede <code>setGeometry</code> ile kendi penceresinin üstüne oturtur. İki pencere olduğundan sıralama ve tıklama sorunları çıkar.'],
]))
add('''<div class="note"><b>Neden Standalone da LEGACY?</b> Texture varken ayrı bir pencere çizim yoluna gerek yoktur: host kendi XPLM
penceresini açıp dokuyu basarak Standalone'un yaptığını yaklaşık 100 satırda yapar (<code>examples/texture_host</code>).
Standalone'un bugün Texture'dan fazla verdiği tek şey <b>pop-out</b>'tur (ayrı işletim sistemi penceresi / ikinci monitör):
Texture haritası pop-out edilmiş bir host penceresinde ayrı GL bağlamı yüzünden çizilemez. Plan: Texture'a pop-out desteği ve
kütüphanede Texture üstüne kurulu hazır bir pencere yardımcısı; ardından resmî eklenti Texture'a geçer ve LEGACY stiller
kaldırılır (<code>docs/SRS.md</code>). Texture kapsamlı denenene kadar LEGACY stillerin kodu ve davranışı korunur.</div>''')
add('<h3>4.4 Dosyalar ve klasörler</h3>')
add('<p>Hepsi yöneticinin <b>veri klasörünün</b> altındadır: <code>setDataDirectory()</code>; verilmezse <code>%LOCALAPPDATA%\\prime\\prime-xmap\\</code>. Yollar <code>initialize()</code>\'dan önce de sorgulanabilir (<code>getDataDirectory</code>, <code>getXMapsDirectory</code>, <code>getSettingsFilePath</code>, <code>getCacheDirectory</code>).</p>')
add(pre('''<veri klasoru>/
  prime_map.json               merkezi ayarlar (bu yoneticinin butun haritalari; bolum 11.4)
  maps/
    main/                      setName("main") verilen harita
      map_settings.json        gorunum, mod, katmanlar, zoom araliklari, renkler, kaynaklar (11.1)
      map_drawings.txt         kullanici cizimleri (11.3)
      terrain_colors.json      bu haritanin zemin renkleri ve ayarlari (11.2)
    map_2/                     ad verilmeyen harita: map_<kimlik>
  cache/                       ORTAK onbellek (silinebilir; setCacheDirectory ile baska yere)
    terrain/                   *.terrain *.elevation *.vectors, xplane_palette.txt
    airports/                  airport_index.bin'''))
add(table(['Konu', 'Kural'], [
    ['Harita adı', '<code>setName</code> ile. Klasör adında harf, rakam, <code>- _ .</code> dışındakiler <code>_</code> olur. Verilmezse <code>map_&lt;kimlik&gt;</code> (oluşturulma sırası). Kalıcı ayar isteyen host <b>ad vermelidir</b>. İki canlı harita aynı adı alamaz (<code>setName</code> <code>false</code>).'],
    ['Ne zaman okunur', '<code>XMap::initialize()</code> sırasında (sadece <code>Persistence::LoadAndSave</code>).'],
    ['Ne zaman yazılır', 'İlk açılışta; bir ayar değişince (<code>maps.autosave</code>, en sık <code>autosave_seconds</code> arayla); <code>close()</code>, <code>removeXMap()</code>, <code>shutdown()</code>; <code>saveSettings()</code>.'],
    ['Birden çok host', 'Her host kendi yöneticisini kurar. Varsayılan veri klasörü herkes için aynı olduğundan her host <code>setDataDirectory</code> ile kendi klasörünü vermelidir (aksi halde iki zemin işçisi aynı önbellek dosyalarına yazar).'],
]))
add('<h3>4.5 Kod mu, dosya mı? (<code>Persistence</code>)</h3>')
add(table(['Değer', '<code>initialize()</code> sırasında', 'Değişince', 'Ne zaman'], [
    ['<code>LoadAndSave</code> (varsayılan)', 'Dosyalar okunur; <b>dosyadaki değer, önceden kodla verilenin yerine geçer</b>. Dosyada olmayan ayar koddan kalır.', 'Yazılır', 'Kullanıcının son oturumu geri gelsin; koddaki değerler "ilk açılış varsayılanı".'],
    ['<code>SaveOnly</code>', 'Okunmaz: kodla verilen her açılışta geçerli', 'Yazılır', 'Host her şeyi belirliyor ama dosyada görmek istiyor.'],
    ['<code>Off</code>', 'Okunmaz', 'Yazılmaz', 'Host kendi profil sistemini kullanıyor (<code>exportSettings</code> / <code>importSettings</code>).'],
]))
add('<p><code>initialize()</code>\'dan <b>sonra</b> yapılan çağrı her zaman uygulanır. Bir ayarın her açılışta kodun dediği gibi olması isteniyorsa o çağrıyı <code>initialize()</code>\'dan sonra yapın (örnek projedeki <code>customizeMap</code> böyle).</p>')

# 5 -------------------------------------------------------------------------------------------------
add('<h2>5. Koordinatlar ve birimler</h2>')
add(table(['Büyüklük', 'Birim'], [
    ['Ekran (Standalone / Embedded)', 'X-Plane global ekran birimi (boxel; <code>XPLMGetWindowGeometry</code> ile aynı). x sağa, y <b>yukarı</b>. <code>Geometry</code>: sol-üst köşe + genişlik / yükseklik.'],
    ['Doku (Texture)', 'Haritanın kendi alanı: <b>orijin sol-alt köşe</b>, birim boxel, y yukarı, alan <code>0 .. getTextureWidth()</code> &times; <code>0 .. getTextureHeight()</code>. <code>inject*</code>, <code>screenToGeo</code>, <code>geoToScreen</code>, <code>queryAtScreen</code> bu koordinatı kullanır. Doku pikseli = boxel &times; ölçek (varsayılan: host\'un UI ölçeği).'],
    ['Konum', 'Derece; kuzey ve doğu pozitif'],
    ['Zoom', 'Web Mercator: zoom 0\'da dünya 256 boxel; her adım 2 kat; sürekli sayı. Varsayılan aralık 3 &hellip; 16.5 (<code>minZoom()</code>, <code>maxZoom()</code>), mutlak üst sınır 18 (<code>hardMaxZoom()</code>).'],
    ['Yön / hız / irtifa', 'Gerçek (true) derece / knot / feet MSL'],
    ['Renk', '<code>Rgb</code> 0&ndash;255 tamsayı'],
]))
add('<p>Zoom &harr; yaklaşık görünen genişlik (1000 boxel, 45&deg; enlem): 5 &asymp; 2100 NM, 8 &asymp; 260 NM, 10 &asymp; 65 NM, 12 &asymp; 16 NM, 14 &asymp; 4 NM, 16 &asymp; 1 NM. Tam değer: <code>getMetersPerBoxel()</code>.</p>')

# 6 -------------------------------------------------------------------------------------------------
add('<h2>6. <code>XMapManagerFactory</code> ve <code>XMapManager</code></h2>')
add(table(['Fonksiyon', 'Açıklama'], [
    ['<code>static XMapManager* XMapManagerFactory::createXMapManager()</code>', 'Yeni yönetici (ayrı veri, ayrı önbellek bütçesi).'],
    ['<code>static void XMapManagerFactory::destroyXMapManager(XMapManager*)</code>', 'Kalan haritalar yazılır ve silinir, yönetici silinir.'],
    ['<code>void setDataDirectory(const std::string&amp;)</code>', '<code>initialize</code> öncesi. Boş: <code>%LOCALAPPDATA%/prime/prime-xmap/</code>.'],
    ['<code>void setCacheDirectory(const std::string&amp;)</code>', '<code>initialize</code> öncesi. Boş: <code>&lt;veri&gt;/cache/</code>.'],
    ['<code>void setLogPrefix(const std::string&amp;)</code>', 'Log.txt satır öneki (süreçte tek; bölüm 15).'],
    ['<code>void setLogCallback(std::function&lt;void(std::string_view)&gt;)</code>', 'Log satırları host\'a da iletilir.'],
    ['<code>void setTerrainCacheMb(int)</code>, <code>void setPrefetchTiles(int)</code>', '<code>prime_map.json</code> henüz yoksa ilk değerler. Verilmezse 1024 MB / 20 karo. 0 = sınırsız / kapalı (<code>setPrefetchTiles(0)</code> ön yüklemeyi tamamen kapatır). Açık harita yokken de uçak konumu çevresindeki karolar arka planda yüklenir (tek satır Log; ilk harita hızlı açılır).'],
    ['<code>bool initialize()</code>', 'Ortak veriyi ve arka plan işlerini başlatır; zaten çalışıyorsa <code>true</code>.'],
    ['<code>void shutdown()</code>, <code>bool isInitialized() const</code>', 'Durdurur (açık haritalar kapatılır, dosyalar yazılır).'],
    ['<code>Status getStatus() const</code>', 'Nav / havalimanı / hava yolu / zemin indeksi hazır mı, bekleyen zemin resmi, ön önbellek ilerlemesi, açık harita sayısı, geçerli önbellek / doku sınırları, durum ve performans metni.'],
    ['<code>XMap* addXMap(const XMapConfig&amp; = {})</code>', 'Yeni harita nesnesi (açılmaz). <code>initialize</code> yapılmadıysa <code>nullptr</code>.'],
    ['<code>void removeXMap(XMap*)</code>', 'Ayarları yazar, haritayı siler.'],
    ['<code>XMap* findXMap(int id)</code>, <code>XMap* findXMap(const std::string&amp; name)</code>', 'Yoksa <code>nullptr</code>.'],
    ['<code>std::vector&lt;XMap*&gt; getXMaps()</code>, <code>int getOpenXMapCount()</code>', 'Bütün / açık haritalar.'],
    ['<code>XMapManagerSettings getSettings()</code>, <code>bool setSettings(const XMapManagerSettings&amp;)</code>', 'Merkezi ayarlar (bölüm 11.4); hemen uygulanır ve yazılır.'],
    ['<code>getDataDirectory()</code>, <code>getSettingsFilePath()</code>, <code>getXMapsDirectory()</code>, <code>getCacheDirectory()</code>', 'Yollar; <code>initialize</code> öncesi de doğru.'],
    ['<code>parentOf</code>, <code>childrenOf</code>, <code>topLevelLayers</code>, <code>kindOf</code>, <code>layerName</code>, <code>layerKey</code>, <code>layerFromKey</code>, <code>isLayerDefaultVisible(layer, mode)</code>', 'Katman ağacı, adları, dosya anahtarları ve mod başına varsayılanları (bölüm 10).'],
    ['<code>std::string prime::versionString()</code>', '<code>"1.0.0"</code>'],
    ['<code>std::string prime::libraryPath()</code>', 'Yüklü <code>prime_map.dll</code>\'nin tam yolu (ready satırında da yazar; alınamazsa boş).'],
    ['<code>bool prime::isLibraryVersionMatching()</code>', 'Host\'un derlendiği sürümle yüklü DLL\'in sürümü aynı mı (inline, DLL çağrısı gerektirmez). Uyuşmazlıkta fabrika yine de çalışır ama tek satır WARNING yazar (bölüm 16).'],
]))

# 7 -------------------------------------------------------------------------------------------------
add('<h2>7. <code>XMap</code>: bütün fonksiyonlar</h2>')
G = [
    'Yaşam ve kimlik',
    ['<code>bool initialize()</code>', 'Haritayı açar (Texture: doku hazırlanır; diğerleri: pencere). <code>LoadAndSave</code>\'de dosyaları okur.'],
    ['<code>void close()</code>, <code>bool isInitialized()</code>', 'Kapatır; nesne ve ayarları durur.'],
    ['<code>bool isDataLoaded()</code>', 'Nav + havalimanı + zemin indeksi hazır ve bekleyen doku yok.'],
    ['<code>shutdown()</code> sonrası çağrılar', 'Yönetici <code>shutdown()</code> ettikten sonra harita işaretçisi geçerli kalır ama tüm metotlar güvenli no-op\'tur: ilk çağrıda tek satır Log uyarısı, sonra sessiz; <code>false</code> / boş / varsayılan döner. <code>removeXMap</code> ya da yönetici yıkıcısı sonrası işaretçi GECERSIZDIR (nesne silinir); bu korunamaz, host işaretçiyi bırakmalı.'],
    ['<code>int getId()</code>, <code>bool setName(name)</code>, <code>getName()</code>, <code>getSettingsDirectory()</code>', 'Kimlik ve ayar klasörü. <code>setName</code> sadece kapalıyken; kullanılan ad reddedilir.'],
    ['<code>setPersistence(Persistence)</code>, <code>getPersistence()</code>, <code>bool saveSettings()</code>', 'Bölüm 4.5; hemen yaz.'],
    'Pencere (LEGACY Standalone / Embedded; Texture\'da çoğu etkisiz)',
    ['<code>setStyle(WindowStyle)</code>, <code>getStyle()</code>', 'Sadece kapalıyken.'],
    ['<code>setTitle</code>, <code>getTitle</code>, <code>setGeometry(Geometry)</code>, <code>getGeometry()</code>', 'Başlık, yer / boyut. Texture\'da <code>setGeometry</code> yok sayılır, <code>getGeometry</code> doku alanını verir.'],
    ['<code>setVisible(bool)</code>, <code>isVisible()</code>', 'Texture: görünmezken <code>renderTexture()</code> <code>false</code> döner.'],
    ['<code>bringToFront()</code>, <code>setPoppedOut(bool)</code>, <code>isPoppedOut()</code>', 'Sadece pencereli stillerde.'],
    ['<code>setIsUiVisible(bool)</code>, <code>isUiVisible()</code>', 'Haritanın kendi arayüzü: üst şerit (Layers, Map modu, Follow, Go to airport, Draw tools), katman paneli, durum şeridi.'],
    ['<code>setIsMouseInputEnabled(bool)</code>, <code>isMouseInputEnabled()</code>', 'Kapalıyken haritaya tıklama / tekerlek alttakine (Texture\'da: <code>inject*</code> <code>false</code>) geçer.'],
    'Doku (WindowStyle::Texture; bölüm 8)',
    ['<code>bool setTextureSize(int w, int h, double pixelsPerUnit = 0)</code>', 'Harita alanı (boxel). 0 = doku pikseli host\'un UI ölçeğinden.'],
    ['<code>getTextureWidth()</code>, <code>getTextureHeight()</code>', 'Boxel.'],
    ['<code>bool renderTexture()</code>', 'Host\'un draw callback\'i içinde; gerekiyorsa yeniden çizer. <code>false</code>: basmayın.'],
    ['<code>unsigned int getTextureId()</code>', '<code>GL_TEXTURE_2D</code> adı; yoksa 0.'],
    ['<code>bool injectMouse(x, y, MouseAction, MouseButton = Left)</code>', 'Down / Drag / Up; tüketildiyse <code>true</code>.'],
    ['<code>void injectMouseMove(x, y)</code>', 'İmleç dokunun üstündeyken her karede (tooltip, vurgu).'],
    ['<code>bool injectWheel(x, y, clicks)</code>', 'İmleç altında zoom (katman panelinin üstündeyse liste kayar).'],
    ['<code>bool injectKey(char key, int flags, char virtualKey)</code>, <code>bool wantsKeyboard()</code>', 'XPLM key callback değerleri aynen. <code>wantsKeyboard</code>: arama kutusu açık, host klavye odağını almalı.'],
    'Görünüm',
    ['<code>setCenter(lat, lon)</code>, <code>getCenter()</code>, <code>setZoom</code>, <code>getZoom</code>', 'Varsayılan merkez 39.0, 35.0, zoom 6.'],
    ['<code>static minZoom()</code>, <code>maxZoom()</code>, <code>hardMaxZoom()</code>', '3, 16.5, 18.'],
    ['<code>bool setZoomLimits(min, max)</code>, <code>getMinZoomLimit()</code>, <code>getMaxZoomLimit()</code>', 'Bu haritanın zoom sınırı.'],
    ['<code>zoomIn()</code>, <code>zoomOut()</code>, <code>setZoomStep</code>, <code>getZoomStep</code>', 'Adım varsayılanı 0.5 (tekerlek tıkı da).'],
    ['<code>panLeft()</code>, <code>panRight()</code>, <code>panUp()</code>, <code>panDown()</code>, <code>setPanStep(fraction)</code>, <code>getPanStep()</code>, <code>panBy(dx, dy)</code>', 'Adım görünen alanın oranı (0.25); <code>panBy</code> boxel.'],
    ['<code>fitBounds(GeoBounds)</code>, <code>getVisibleBounds()</code>, <code>getMetersPerBoxel()</code>', ''],
    ['<code>setFollowAircraft(bool)</code>, <code>isFollowingAircraft()</code>, <code>centerOnAircraft()</code>', 'Takip varsayılan açık; elle kaydırınca kapanır.'],
    ['<code>bool goToAirport(ident)</code>, <code>findAirports(query, max)</code>', 'ICAO / kimlik / ad; havalimanı ekrana sığar.'],
    ['<code>LatLon screenToGeo(x, y)</code>, <code>bool geoToScreen(lat, lon, &amp;x, &amp;y)</code>', 'Ekran (Texture\'da doku) koordinatı.'],
    'Mod',
    ['<code>setMode(XMapMode)</code>, <code>getMode()</code>, <code>nextMode()</code>', 'Scenery, Vfr, IfrLow, IfrHigh, Dark. Katman durumu mod başına ayrı saklanır.'],
    'Katmanlar (bölüm 9, 10)',
    ['<code>setIsLayerVisible(XMapLayer, bool)</code>, <code>isLayerVisible</code>, <code>toggleLayer</code>', 'Geçerli mod için.'],
    ['<code>setLayersVisible({..}, bool)</code>, <code>showOnlyLayers({..})</code>, <code>resetLayersToDefaults()</code>', ''],
    ['<code>LayerState getLayerState(XMapLayer)</code>', 'Off / On / Mixed (alt katmanların bir kısmı açık).'],
    ['<code>bool setLayerZoomRange(layer, min, max)</code>, <code>getLayerZoomRange</code>, <code>clearLayerZoomRange</code>, <code>clearAllLayerZoomRanges</code>, <code>getDefaultLayerZoomRange</code>, <code>getLayerHardMinZoom</code>', 'Grup verilirse bütün katmanlarına uygulanır. 1.0.0: host aralığı yerleşik eşiğin <b>yerine geçer</b> — aşağı da çekilebilir, katman başına ölçülmüş sert alt sınıra kadar (bölüm 9.2); <code>(0, 99)</code> vermek host aralığını kaldırır (yerleşik eşiğe döner).'],
    ['<code>bool setColor(layer, Rgb)</code>, <code>Rgb getColor(layer)</code>, <code>clearColor</code>, <code>clearAllColors</code>', 'Renk verilebilen katmanlar bölüm 9.3. Desteklenmeyen katman: <code>false</code>. <code>getColor</code>: yazma yoksa <code>mR &lt; 0</code>.'],
    'Çizimler',
    ['<code>setDrawTool(DrawTool)</code>, <code>getDrawTool()</code>', 'Pan, Point, Route, Region.'],
    ['<code>bool addDrawingPoint(lat, lon)</code>, <code>bool finishDrawing()</code>, <code>bool undoDrawing()</code>, <code>clearDrawings()</code>', 'Koddan çizim.'],
    ['<code>hasUnfinishedDrawing()</code>, <code>getShapes()</code>, <code>bool removeShape(id)</code>', ''],
    ['<code>std::string exportDrawings()</code>, <code>bool importDrawings(text)</code>', '<code>map_drawings.txt</code> biçimi (11.3).'],
    'Uçak, iz, rota',
    ['<code>setAircraftSource(DataSource)</code>, <code>getAircraftSource()</code>, <code>setAircraft(AircraftState)</code>, <code>getAircraft()</code>', '<code>XPlane</code>: dataref\'ler; <code>Host</code>: <code>setAircraft</code> ile.'],
    ['<code>clearTrack()</code>', 'Uçuş izini siler.'],
    ['<code>setRouteSource(DataSource)</code>, <code>getRouteSource()</code>, <code>setRoute(waypoints, activeIndex)</code>', '<code>XPlane</code>: FMS; <code>Host</code>: <code>setRoute</code> ile.'],
    'Sorgu',
    ['<code>queryAt(lat, lon)</code>, <code>queryAtScreen(x, y)</code>', 'O noktadaki nesneler (<code>XMapObject</code>: katman + metin), en üstteki önce.'],
    'Çizim seçenekleri',
    ['<code>setImageCache(bool)</code>, <code>isImageCacheEnabled()</code>, <code>setMaxRenderFps(int)</code>, <code>getMaxRenderFps()</code>, <code>setXPlaneFont(bool)</code>, <code>isXPlaneFont()</code>', 'Harita resmi önbelleği (pencereli stiller), en çok yeniden çizim / s (varsayılan 15), yazı tipi.'],
    'Ayarlar',
    ['<code>exportSettings()</code>, <code>bool importSettings(text)</code>', '<code>map_settings.json</code> içeriği (görünüm, mod, katmanlar, zoom aralıkları, renkler, kaynaklar).'],
    ['<code>getTerrainSettings()</code>, <code>setTerrainSettings(TerrainSettings)</code>, <code>exportTerrainSettings()</code>, <code>importTerrainSettings(text)</code>', 'Bu haritanın zemini (11.2); hemen uygulanır.'],
    ['<code>getSettingsRevision()</code>, <code>getDrawingsRevision()</code>', 'Değişince artar (host kendi kaydını tetiklemek için).'],
    'Durum, ön önbellek, teşhis',
    ['<code>getStatusText()</code>, <code>getTerrainPendingImages()</code>', ''],
    ['<code>bool precacheVisibleArea()</code>, <code>bool precacheArea(GeoBounds)</code>, <code>cancelPrecache()</code>', 'Alanın kurulu DSF karolarını diske hazırlar; sınırın %80\'ini aşacaksa başlamaz.'],
    ['<code>setDiagnostics(bool)</code>, <code>isDiagnosticsOn()</code>', 'Log\'a periyodik teşhis, alt şeritte FPS.'],
    ['<code>onRender()</code>', 'Bir kare çizer (Texture\'da <code>renderTexture</code> ile aynı); normalde gerekmez.'],
]
add(table(['Fonksiyon', 'Açıklama'], G))

# 8 -------------------------------------------------------------------------------------------------
add('<h2>8. Doku modu (<code>WindowStyle::Texture</code>) ayrıntıları</h2>')
add(table(['Konu', 'Kural'], [
    ['Nerede çağrılır', '<code>renderTexture()</code> sadece host\'un bir XPLM draw callback\'i <b>içinde</b> (GL bağlamı etkin). Host penceresi pop-out edilmişse (ayrı GL bağlamı) çizim yapılmaz, Log\'a bir kez yazılır.'],
    ['Ne zaman yeniden çizer', 'İlk kez; doku boyutu, sahne (mod, katman, renk, çizim), görünüm (merkez, zoom) değişince; girdi gelince hemen. Aksi halde en çok <code>maxRenderFps</code> kez / s (uçak ve iz hareket eder). Gerek yoksa eski doku geçerlidir, çağrı ucuzdur.'],
    ['Doku', 'RGBA, opak (zemin her zaman dolu). (0,0) sol-alt, (1,1) sağ-üst. Boyut: <code>setTextureSize</code> &times; ölçek. Host saydamlıksız basmalı.'],
    ['Ölçek', '<code>pixelsPerUnit = 0</code>: host\'un o anki UI ölçeği (keskin görüntü). Sabit değer de verilebilir (ör. dokuyu bir kokpit ekranına 1:1 basmak için 1.0).'],
    ['GL durumu', 'Kütüphane program / VAO / tampon bağlamalarını, framebuffer, viewport, scissor ve clear color\'ı geri koyar; dokuları <code>XPLMBindTexture2d</code> ile bağlar. Host <code>renderTexture</code> sonrası kendi çiziminden önce <code>XPLMSetGraphicsState</code> çağırmalıdır.'],
    ['Girdi', 'Koordinat dokuya göre (sol-alt orijin, boxel). Basma haritada başladıysa sürükleme ve bırakma da (alan dışına taşsa bile) iletilmeli. <code>injectMouseMove</code> imleç haritanın üstündeyken her karede (cursor callback) çağrılmalı; 0.25 s gelmezse imleç "dışarıda" sayılır.'],
    ['Klavye', 'Haritanın arama kutusu açılınca <code>wantsKeyboard()</code> <code>true</code> olur; host haritaya tıklandığında <code>XPLMTakeKeyboardFocus(pencere)</code> yapar, tuşları <code>injectKey</code> ile iletir, <code>wantsKeyboard()</code> <code>false</code> olunca odağı bırakır. Her karede odak almayın (kullanıcı X-Plane\'e tıklayınca klavye geri dönmez).'],
    ['Yazı', 'Atlas yazı tipi (varsayılan) gerekir; <code>setXPlaneFont(true)</code> ile etiketler dokuda yanlış yere düşebilir (Log uyarır).'],
    ['Birden çok doku', 'Her Texture harita kendi dokusunu tutar; aynı host birden çok haritayı farklı yerlere basabilir.'],
]))

# 9 -------------------------------------------------------------------------------------------------
add('<h2>9. Özelleştirme: neyi ne kadar kontrol edebilirsiniz</h2>')
add('<p>Hedef, haritanın her öğesinin API ile kontrol edilmesidir. Bu sürümün durumu aşağıda; <span class="ok">Var</span> tam, <span class="part">Kısmen</span> sınırlı, <span class="no">Yok</span> henüz API\'de yok. Eksikler gereksinim belgesinde (<code>docs/SRS.md</code>) izlenir.</p>')
add(table(['Alan', 'Durum', 'Nasıl / sınır'], [
    ['Yerleşim: istenen yer, boyut, birden çok harita', '<span class="ok">Var</span>', 'Texture (bölüm 8); sınırsız bağımsız harita'],
    ['Harita modu (tema)', '<span class="part">Kısmen</span>', '5 hazır mod; host yeni tema tanımlayamaz'],
    ['Katman / alt katman görünürlüğü', '<span class="ok">Var</span>', '56 öğe, mod başına ayrı (bölüm 10)'],
    ['Katman zoom aralığı', '<span class="ok">Var</span>', 'Host aralığı yerleşik eşiğin yerine geçer; aşağı çekme katman başına ölçülmüş sert alt sınıra kadar serbest (1.0.0, bölüm 9.2)'],
    ['Katman rengi', '<span class="part">Kısmen</span>', '14 katman (9.3); havalimanı sembolleri, yüzeyler, nesneler, uçak, rota, iz, çizimler, etiket yazıları, arka plan / su (Dark / IFR) renklendirilemez; renkler modlar arası ortak'],
    ['Çizgi kalınlığı, sembol boyu, yazı boyu / tipi', '<span class="no">Yok</span>', 'Tema içinde sabit; sadece atlas / X-Plane yazı tipi seçimi'],
    ['Zemin renkleri ve görünümü', '<span class="ok">Var</span>', '15 arazi sınıfı rengi, X-Plane paleti / elle, kabartma, kıyı, su şeridi, derinlik, VFR bantları ve konturları (11.2)'],
    ['Zoom sınırları, adımları, kaydırma adımı', '<span class="ok">Var</span>', ''],
    ['Haritanın kendi arayüzü', '<span class="part">Kısmen</span>', 'Tümü açık / kapalı; tek tek düğme gizleme, arayüz renkleri ve yerleşimi yok'],
    ['Girdi', '<span class="ok">Var</span>', 'Fare / tekerlek / klavye açık-kapalı; Texture\'da tam iletim'],
    ['Uçak ve rota verisi', '<span class="ok">Var</span>', 'X-Plane ya da host (<code>setAircraft</code>, <code>setRoute</code>)'],
    ['Kullanıcı çizimleri', '<span class="part">Kısmen</span>', 'Ekle / sil / dışa-içe aktar; çizim rengi ve stili yok'],
    ['Host\'un kendi katmanı / sembolleri', '<span class="no">Yok</span>', 'Host haritanın üstüne kendi çizimini koyamaz (geçici çözüm: <code>geoToScreen</code> ile dokunun üstüne host çizer)'],
    ['Sorgu ve dönüşüm', '<span class="ok">Var</span>', '<code>queryAt</code>, <code>screenToGeo</code>, <code>geoToScreen</code>, <code>getVisibleBounds</code>, <code>findAirports</code>'],
    ['Olay bildirimi (tıklanan nesne, görünüm değişti ...)', '<span class="no">Yok</span>', 'Host revizyon sayaçlarını ve getter\'ları sorgular'],
    ['Ayarların saklanması', '<span class="ok">Var</span>', 'Dosya (Persistence) ya da metin (export / import)'],
]))
add('<h3>9.1 Görünürlük</h3>')
add('<ul><li>Üst öğeyi kapatmak alt öğeleri gizler; alt öğelerin kendi durumu korunur.</li><li>Kapalı katmanın bir alt katmanını açmak sadece o alt katmanı açar (katman <code>Mixed</code>).</li><li>Son alt katman kapanınca katman da kapanır; bütün alt katmanları kapalı bir katman açılınca hepsi açılır.</li><li>Grup açmak o modda varsayılan açık katmanları açar. Durum mod başına ve harita başına saklanır; mod değişince o modun durumu gelir.</li></ul>')
add('<h3>9.2 Zoom aralığı</h3>')
add('<p>Her katmanın yerleşik bir görünme eşiği vardır (bölüm 10, "Eşik" sütunu). 1.0.0\'da <code>setLayerZoomRange</code> '
    'artık bu eşiğin üstüne pencere koymak yerine <b>yerine geçer</b>: host aralığı verildiyse katman yalnızca host '
    'aralığına göre çizilir. Yerleşik eşiği yukarı çekmek serbesttir; <b>aşağı çekmek</b> katman başına ölçülmüş '
    '<b>sert alt sınıra</b> (<code>getLayerHardMinZoom</code>, bölüm 10\'da "sert") kadar serbesttir — daha agresif '
    'istek kırpılır ve Log\'a tek satır uyarı yazılır (<i>min ... clamped to ...</i>). <code>(0, 99)</code> vermek host '
    'aralığını kaldırır: katman yerleşik eşiğine döner. Kutu durumu (<code>isLayerVisible</code>) değişmez; etkili '
    'aralık değiştiği için harita resmi yenilenir. Yerleşik değerler <code>getDefaultLayerZoomRange</code> ile okunur.</p>')
add(pre('''map->setLayerZoomRange(prime::XMapLayer::AirwaysGroup, 8.0, 99.0);  // hava yollari ancak zoom 8'den yakinda
map->setLayerZoomRange(prime::XMapLayer::AirwaysLow, 5.0, 99.0);    // yerlesik 6.5'i sert sinir 5.0'a kadar asagi cek
map->setLayerZoomRange(prime::XMapLayer::Fixes, 10.5, 99.0);        // fix'ler 10.5'ten itibaren
map->setLayerZoomRange(prime::XMapLayer::Terrain, 0.0, 12.0);       // zemin 12'den yakinda cizilmez
map->setLayerZoomRange(prime::XMapLayer::RoadsLocal, 13.0, 99.0);   // alt katman: yerel yollar 13+
map->setLayerZoomRange(prime::XMapLayer::Runways, 3.0, 99.0);       // WARNING: 10.0'a (sert sinir) kirpilir
map->clearLayerZoomRange(prime::XMapLayer::Fixes);                  // (0, 99) ile ayni: yerlesik esige don'''))
add('<h3>9.3 Renk verilebilen katmanlar</h3>')
add(table(['Katman', 'Neyi boyar'], [
    ['<code>Grid</code>', 'Enlem / boylam çizgileri'],
    ['<code>RoadsMotorways</code>, <code>RoadsPrimary</code>, <code>RoadsSecondary</code>, <code>RoadsLocal</code>, <code>RoadsRailways</code>, <code>RoadsPowerLines</code>', 'İlgili DSF yol / demiryolu / enerji hattı çizgileri'],
    ['<code>Vor</code>, <code>Ndb</code>, <code>Ils</code>, <code>Fixes</code>', 'Navaid sembolleri ve etiketleri (ILS tüyü)'],
    ['<code>AirwaysLow</code>, <code>AirwaysHigh</code>', 'Hava yolu çizgileri'],
    ['<code>Runways</code>', 'Pist kenar çizgisi'],
    ['<code>TerrainBuiltUp</code>', '<b>Bilinen hata:</b> <code>setColor</code> <code>true</code> döner ama zemin dokusu modun sabit rengini kullanır, değişiklik görünmez'],
]))
add(pre('''map->setColor(prime::XMapLayer::AirwaysLow, prime::Rgb{255, 140, 0});   // turuncu alcak hava yollari
map->setColor(prime::XMapLayer::Vor, prime::Rgb{220, 40, 40});
prime::Rgb c = map->getColor(prime::XMapLayer::Vor);                       // yazma yoksa c.mR < 0
map->clearColor(prime::XMapLayer::Vor);                                    // modun kendi rengine don
bool ok = map->setColor(prime::XMapLayer::AirportSymbols, prime::Rgb{0, 0, 0});  // false: desteklenmiyor'''))
add('<h3>9.4 Zemin</h3>')
add(pre('''prime::TerrainSettings t = map->getTerrainSettings();
t.mUseXPlaneColors = false;                                   // sinif renkleri asagidan
t.mClassColors = {{"water", {20, 60, 120}}, {"forest_conifer", {40, 80, 50}}};
t.mShadingStrength = 0.9;                                     // guclu kabartma
t.mVfrContourLines = true;
t.mVfrBandsFeet = {{0, {190, 216, 166}}, {1000, {210, 226, 172}}};  // bant alt siniri (ft) + renk
map->setTerrainSettings(t);                                   // hemen; sadece bu harita'''))
add('<p>Sınıf adları: <code>water, urban, forest_conifer, forest_broadleaf, shrub, grass, crop, wetland, rock, snow_ice, tundra, sand, airport, land</code>.</p>')

# 10 ------------------------------------------------------------------------------------------------
add('<h2>10. Katmanlar: tam liste</h2>')
add('<p>Son sütunlardan ilki mod başına varsayılan (Scenery, VFR, IFR Low, IFR High, Dark: &#9679; açık, &#9675; kapalı). '
    '"Eşik": katmanın yerleşik görünmeye başladığı zoom (boşsa üst katmanınki); parantezdeki <b>sert</b> değeri, host '
    'aralığının kaçak aşağı çekilebileceği alt sınırdır (<code>getLayerHardMinZoom</code>; yalnızca bölüm 9.2\'deki '
    'tablodaki katmanlar için tanımlı) ve <code>getDefaultLayerZoomRange</code> yerleşik eşik çiftini verir. Anahtar: '
    '<code>map_settings.json</code>\'daki ad (<code>XMapManager::layerKey</code>). Grupların anahtarı yoktur, durumu '
    'katmanlarından gelir. Bir alt katmanın çizilmesi için katmanının da açık olması gerekir.</p>')
add(table(['XMapLayer', 'Anahtar', 'Panelde adı', 'Eşik (sert alt sınır)', 'S V L H D', 'Renk'], layer_rows()))

# 11 ------------------------------------------------------------------------------------------------
add('<h2>11. Ayar dosyaları</h2>')
add('<p>Biçim JSON: <code>{ "bölüm": { "anahtar": "değer" } }</code>; değerler tırnak içinde metin, sayılar nokta ile, mantıksal <code>"1"</code> / <code>"0"</code>. Kütüphane yazar; X-Plane kapalıyken elle düzenlenebilir. Dosyada olmayan anahtar kodla verilen (ya da varsayılan) değeri alır. Silmek varsayılanları getirir.</p>')
add('<h3>11.1 <code>maps/&lt;ad&gt;/map_settings.json</code></h3>')
add(table(['Bölüm', 'Anahtarlar', 'API'], [
    ['<code>window</code>', '<code>left, top, right, bottom, visible</code>', '<code>setGeometry</code>, <code>setVisible</code> (sadece Standalone\'da geri yüklenir)'],
    ['<code>view</code>', '<code>mode</code> (<code>scenery|vfr|ifr_low|ifr_high|dark</code>), <code>center_lat, center_lon, zoom, follow_aircraft</code>', '<code>setMode</code>, <code>setCenter</code>, <code>setZoom</code>, <code>setFollowAircraft</code>'],
    ['<code>view</code>', '<code>show_controls, mouse_input, min_zoom, max_zoom, zoom_step, pan_step</code>', '<code>setIsUiVisible</code>, <code>setIsMouseInputEnabled</code>, <code>setZoomLimits</code>, <code>setZoomStep</code>, <code>setPanStep</code>'],
    ['<code>render</code>', '<code>cache, max_fps</code>', '<code>setImageCache</code>, <code>setMaxRenderFps</code>'],
    ['<code>text</code>', '<code>renderer</code> (<code>atlas|xplane</code>)', '<code>setXPlaneFont</code>'],
    ['<code>tools</code>', '<code>visible, collapsed, offset_right, offset_top</code>', 'Çizim paleti (arayüzden)'],
    ['<code>layers.&lt;mod&gt;</code> &times; 5', '<code>&lt;anahtar&gt;: "1"|"0"</code>', '<code>setIsLayerVisible</code>'],
    ['<code>layer_zoom</code>', '<code>&lt;anahtar&gt;: "&lt;min&gt; &lt;max&gt;"</code> (sadece sınır verilenler)', '<code>setLayerZoomRange</code>'],
    ['<code>layer_colors</code>', '<code>&lt;anahtar&gt;: "R G B"</code> (sadece yazılanlar)', '<code>setColor</code>'],
    ['<code>sources</code>', '<code>aircraft, route</code> (<code>xplane|host</code>)', '<code>setAircraftSource</code>, <code>setRouteSource</code>'],
]))
add(pre('''{
  "layer_colors": { "airways_low": "255 140 0", "vor": "220 40 40" },
  "layer_zoom":   { "fix": "10.50 99.00", "airways_low": "8.00 99.00" },
  "layers.vfr":   { "aircraft": "1", "airports": "1", "fix": "1", "roads": "0", "runways": "1", "terrain": "1" },
  "render":       { "cache": "1", "max_fps": "15" },
  "sources":      { "aircraft": "xplane", "route": "xplane" },
  "text":         { "renderer": "atlas" },
  "tools":        { "collapsed": "0", "offset_right": "8", "offset_top": "8", "visible": "1" },
  "view":         { "center_lat": "47.45", "center_lon": "-122.3", "follow_aircraft": "0", "max_zoom": "15",
                    "min_zoom": "6", "mode": "vfr", "mouse_input": "1", "pan_step": "0.25",
                    "show_controls": "1", "zoom": "10", "zoom_step": "0.5" },
  "window":       { "bottom": "200", "left": "100", "right": "1000", "top": "800", "visible": "1" }
}'''))
add('<p>(<code>layers.&lt;mod&gt;</code> bölümlerinde her katman ve alt katman için bir satır bulunur; burada kısaltıldı.)</p>')
add('<h3>11.2 <code>maps/&lt;ad&gt;/terrain_colors.json</code> ve <code>TerrainSettings</code></h3>')
add(table(['Bölüm', 'Anahtar (varsayılan)', '<code>TerrainSettings</code>', 'Ne'], [
    ['<code>palette</code>', '<code>source</code> (<code>xplane</code>)', '<code>mUseXPlaneColors</code>', '<code>xplane</code>: sınıf renkleri X-Plane\'in arazi dokularından; <code>colors</code>: <code>colors</code> bölümündeki değerler'],
    ['<code>palette</code>', '<code>version</code>', '&mdash;', 'Elle değiştirmeyin'],
    ['<code>colors</code>', '<code>&lt;sınıf&gt;: "R G B"</code>', '<code>mClassColors</code>', 'Arazi sınıfı renkleri; su rengi her zaman buradan'],
    ['<code>shading</code>', '<code>strength</code> (0.45)', '<code>mShadingStrength</code>', 'Kabartma gölgesi 0&ndash;1'],
    ['<code>blend</code>', '<code>meters</code> (150), <code>max_pixels</code> (5)', '<code>mBlendMeters</code>, <code>mBlendMaxPixels</code>', 'Scenery\'de sınıflar arası yumuşak geçiş (0 = kapalı)'],
    ['<code>warp</code>', '<code>meters</code> (65), <code>wavelength</code> (250)', '<code>mWarpMeters</code>, <code>mWarpWavelength</code>', 'Sınıf sınırlarının kıvrılması (0 = düz)'],
    ['<code>closeup</code>', '<code>class_mix</code> (0.3)', '<code>mCloseupClassMix</code>', 'Yakın zoomda sınıf renklerinin ortak kara tonuna yaklaşması'],
    ['<code>relief</code>', '<code>multi_directional</code> (1), <code>exaggeration</code> (2.5), <code>coarse_boost</code> (0.35), <code>slope_tone</code> (0.12), <code>flat_slope</code> (0.02)', '<code>mRelief*</code>', 'Dört yönlü ışık, eğim abartması, uzakta ek abartma, dik yamaç koyuluğu, düz ova eşiği'],
    ['<code>water</code>', '<code>coastline</code> (1), <code>coastline_strength</code> (0.5)', '<code>mCoastline</code>, <code>mCoastlineStrength</code>', 'Kıyı çizgisi'],
    ['<code>water</code>', '<code>band_meters</code> (200), <code>band_max_pixels</code> (24), <code>band_strength</code> (0.1)', '<code>mWaterBand*</code>', 'Kıyıdan açığa açık şerit'],
    ['<code>water</code>', '<code>depth_tone</code> (0.25), <code>depth_full_meters</code> (200)', '<code>mDepthTone</code>, <code>mDepthFullMeters</code>', 'Derinliğe göre koyulaşma'],
    ['<code>vfr</code>', '<code>bands</code> (1), <code>contour_lines</code> (0), <code>contour_strength</code> (0.35)', '<code>mVfrBands</code>, <code>mVfrContourLines</code>, <code>mVfrContourStrength</code>', 'Sectional yükseklik bantları, bant sınırında kontur'],
    ['<code>vfr</code>', '<code>band_1</code> &hellip; <code>band_8</code>: <code>"&lt;alt sınır ft&gt; R G B"</code>', '<code>mVfrBandsFeet</code>', 'Bant sınırları (artan) ve renkleri'],
]))
add('<h3>11.3 <code>maps/&lt;ad&gt;/map_drawings.txt</code></h3>')
add('<p>Her satır bitmiş bir şekil: <code>&lt;tür&gt; &lt;numara&gt; &lt;enlem&gt; &lt;boylam&gt; [...]</code>. Tür <code>P</code> nokta, <code>R</code> rota (2+ köşe), <code>A</code> bölge (3+ köşe). <code>#</code> yorum. <code>Shape::mId</code> = tür &times; 1&nbsp;000&nbsp;000 + numara (nokta 0, rota 1, bölge 2).</p>')
add(pre('''# Prime Map Plugin drawings: P = point, R = route, A = area (region). <type> <number> <lat> <lon> ...
P 1 47.4502 -122.3088
R 1 47.45 -122.31 47.53 -122.30 47.61 -122.33
A 1 47.40 -122.40 47.40 -122.20 47.50 -122.20 47.50 -122.40'''))
add('<h3>11.4 <code>prime_map.json</code> (<code>XMapManagerSettings</code>)</h3>')
add('<p>Önbellek bir yöneticinin haritaları arasında ortaktır; sınırları açık harita sayısıyla (n) büyür: <code>sınır(n) = taban &times; (1 + artış% &times; (n &minus; 1) / 100)</code>, tavan verilmişse onu geçmez.</p>')
add(table(['Bölüm / anahtar', '<code>XMapManagerSettings</code>', 'Varsayılan', 'Açıklama'], [
    ['<code>cache.terrain_mb</code>', '<code>mTerrainCacheMb</code>', '1024', 'Zemin disk önbelleği (tek harita, MB; 0 = sınırsız)'],
    ['<code>cache.growth_percent_per_map</code>', '<code>mCacheGrowthPercentPerXMap</code>', '25', 'Her ek açık harita için'],
    ['<code>cache.max_terrain_mb</code>', '<code>mTerrainCacheMaxMb</code>', '0', 'Tavan (0 = yok)'],
    ['<code>cache.prefetch_tiles</code>', '<code>mPrefetchTiles</code>', '20', 'Uçağın önü + FMS için önceden hazırlanan karo'],
    ['<code>memory.texture_mb</code>', '<code>mTextureMemoryMb</code>', '512', 'Zemin dokuları için video belleği (tek harita)'],
    ['<code>memory.growth_percent_per_map</code>', '<code>mMemoryGrowthPercentPerXMap</code>', '50', ''],
    ['<code>memory.max_texture_mb</code>', '<code>mTextureMemoryMaxMb</code>', '2048', ''],
    ['<code>maps.autosave</code>', '<code>mAutoSave</code>', '1', 'Değişince kendiliğinden yaz'],
    ['<code>maps.autosave_seconds</code>', '<code>mAutoSaveSeconds</code>', '2', 'Aynı haritanın iki yazımı arası en kısa süre'],
]))
add(pre('''prime::XMapManagerSettings s = manager->getSettings();
s.mTerrainCacheMb = 4096;
manager->setSettings(s);       // hemen uygulanir + prime_map.json'a yazilir'''))

# 12 ------------------------------------------------------------------------------------------------
add('<h2>12. Tipler</h2>')
add(table(['Tip', 'Alanlar / değerler'], [
    ['<code>XMapConfig</code>', '<code>mName</code>, <code>mPersistence</code> (LoadAndSave), <code>mGeometry</code>, <code>mStyle</code> (Texture), <code>mMode</code> (Dark; 1.0.0\'da varsayılan mod Dark, host eskisi gibi <code>Vfr</code> verebilir), <code>mUiVisible</code> (false), <code>mMouseInput</code> (false), <code>mVisible</code> (true), <code>mImageCache</code> (true), <code>mMaxRenderFps</code> (15), <code>mXPlaneFont</code> (false)'],
    ['<code>WindowStyle</code>', '<code>Texture</code>; LEGACY: <code>Embedded</code>, <code>Standalone</code>'],
    ['<code>MouseAction</code> / <code>MouseButton</code>', '<code>Down, Drag, Up</code> / <code>Left, Right</code>'],
    ['<code>XMapMode</code>', '<code>Scenery, Vfr, IfrLow, IfrHigh, Dark</code>'],
    ['<code>Persistence</code>', '<code>Off, SaveOnly, LoadAndSave</code>'],
    ['<code>DrawTool</code>', '<code>Pan, Point, Route, Region</code>'],
    ['<code>DataSource</code>', '<code>XPlane, Host</code>'],
    ['<code>LayerKind</code> / <code>LayerState</code>', '<code>Group, Layer, SubLayer</code> / <code>Off, On, Mixed</code>'],
    ['<code>LatLon</code>, <code>GeoBounds</code>, <code>Geometry</code>, <code>Rgb</code>', '<code>mLat, mLon</code>; <code>mSouth, mWest, mNorth, mEast</code>; <code>mLeft, mTop, mWidth, mHeight</code>; <code>mR, mG, mB</code>'],
    ['<code>Status</code>', '<code>mNavReady, mAirportsReady, mAirwaysReady, mTerrainIndexReady, mTerrainPendingImages, mPrecacheRunning, mPrecacheDone, mPrecacheTotal, mText, mPerformanceText, mOpenMaps, mTerrainCacheLimitMb, mTextureLimitMb</code>'],
    ['<code>AirportMatch</code>, <code>XMapObject</code>, <code>Shape</code>', '<code>mIdent, mName, mPosition</code>; <code>mLayer, mText</code>; <code>mId, mKind, mLabel, mPoints</code>'],
    ['<code>AircraftState</code>, <code>RouteWaypoint</code>', '<code>mValid, mPosition, mHeadingDeg, mGroundSpeedKt, mAltitudeFt</code>; <code>mIdent, mPosition, mAltitudeFt</code>'],
    ['<code>TerrainSettings</code>, <code>XMapManagerSettings</code>', 'Bölüm 11.2, 11.4'],
]))

# 13 ------------------------------------------------------------------------------------------------
add('<h2>13. Tarifler</h2>')
add('<h3>Aynı uçuşun iki görünümü (iki doku)</h3>')
add(pre('''overview->setMode(prime::XMapMode::IfrHigh);  overview->setZoom(6);   overview->setFollowAircraft(true);
detail->setMode(prime::XMapMode::Scenery);    detail->setZoom(14);    detail->setFollowAircraft(true);
detail->showOnlyLayers({prime::XMapLayer::Terrain, prime::XMapLayer::Runways, prime::XMapLayer::Surfaces,
                        prime::XMapLayer::Aircraft});'''))
add('<h3>Uçak ve rota host\'tan (ör. IOS / başka simülasyon)</h3>')
add(pre('''map->setAircraftSource(prime::DataSource::Host);
prime::AircraftState a;  a.mValid = true;  a.mPosition = {41.27, 28.75};  a.mHeadingDeg = 350;  a.mAltitudeFt = 3000;
map->setAircraft(a);                                         // her guncellemede
map->setRouteSource(prime::DataSource::Host);
map->setRoute({{"LTFM", {41.26, 28.74}, 0}, {"YAA", {41.10, 29.00}, 9000}}, 1);'''))
add('<h3>Haritaya tıklanan yeri ve nesneyi bulmak (Texture)</h3>')
add(pre('''const prime::LatLon where = map->screenToGeo(x - mapLeft, y - mapBottom);
for (const prime::XMapObject& o : map->queryAtScreen(x - mapLeft, y - mapBottom))
{
    // o.mLayer (XMapLayer), o.mText ("KSEA Seattle Tacoma Intl", "SEA 116.80" ...)
}'''))
add('<h3>Sadece harita (arayüzsüz), kontrol host\'ta</h3>')
add(pre('''map->setIsUiVisible(false);  map->setIsMouseInputEnabled(false);
// host dugmeleri: map->zoomIn(); map->panLeft(); map->nextMode(); map->goToAirport("KSEA");'''))
add('<h3>Ayar kopyalama, profil</h3>')
add(pre('''mapB->importSettings(mapA->exportSettings());               // gorunum, mod, katmanlar, zoom araliklari, renkler
mapB->importTerrainSettings(mapA->exportTerrainSettings()); // zemin
mapB->importDrawings(mapA->exportDrawings());               // cizimler
map->setPersistence(prime::Persistence::SaveOnly);          // her acilista koddaki ayarlar'''))
add('<h3>Kimlikle erişim</h3>')
add(pre('''if (prime::XMap* m = manager->findXMap("overview")) { m->setVisible(false); }
for (prime::XMap* m : manager->getXMaps()) { m->setMaxRenderFps(10); }'''))

# 14 ------------------------------------------------------------------------------------------------
add('<h2>14. Örnek projeler</h2>')
add(table(['Proje', 'Ne gösterir'], [
    ['<code>prime_map_api/examples/texture_host</code>', 'En küçük tam host: tek pencere, içinde doku harita, girdi iletimi, özelleştirme (bölüm 3\'ün kodu).'],
    ['<code>plugin-api-test</code>', 'Taşınır / boyutlanır pencere içinde doku harita + host düğmeleri; <code>src/XMapApiCalls.cpp</code>: her API çağrısı ayrı metot; <code>src/MultiXMapSelfTest.cpp</code>: çoklu harita özelliklerinin X-Plane içindeki otomatik testi (eklenti klasöründe <code>selftest.flag</code> varsa).'],
    ['<code>prime_map_plugin</code>', 'Resmî eklenti: LEGACY Standalone ana harita (<code>maps/main</code>), menüden ikinci harita, komut &harr; API eşleşmesi. Texture\'a geçişi planlıdır.'],
]))

# 14b -----------------------------------------------------------------------------------------------
add('<h2>14b. Klasörler ve paylaşım senaryoları</h2>')
add('<p>Her yöneticinin iki klasörü vardır: <b>veri klasörü</b> (<code>setDataDirectory</code>; içinde merkez '
    '<code>prime_map.json</code> ve harita başına <code>maps/&lt;ad&gt;/</code> ayarları) ve <b>karo önbellek '
    'klasörü</b> (<code>setCacheDirectory</code>; verilmezse <code>&lt;veri&gt;/cache/</code>). Kural: ayarlar '
    'her zaman veri klasörüne bağlıdır, karo önbelleği ise ayrı seçilebilir. Aynı süreçte ikinci bir yönetici '
    'aynı veri klasörüne çözülürse <code>initialize</code> tek satır WARNING yazar; yalnızca önbellek klasörü '
    'ortaksa tek satır bilgi yazar (yazımlar atomiktir, paylaşım güvenlidir).</p>')

add('<h3>a) Tek yönetici, n harita: ayarlar ayrı, önbellek ortak (önerilen)</h3>')
add(pre('''prime::XMapManager* mgr = prime::XMapManagerFactory::createXMapManager();
mgr->setDataDirectory("C:/X-Plane/prime-xmap");   // hepsi icin tek veri klasoru (isteden)
mgr->initialize();
prime::XMap* a = mgr->addXMap();  a->setName("kaptan");   a->initialize();
prime::XMap* b = mgr->addXMap();  b->setName("plan");     b->initialize();'''))
add(pre('''&lt;veri&gt;/prime_map.json      merkez: onbellek butceleri
&lt;veri&gt;/maps/kaptan/        harita a ayarlari (ayri)
&lt;veri&gt;/maps/uydu/          harita b ayarlari (ayri)
&lt;veri&gt;/cache/              ortak karo onbellegi'''))
add('<p>Haritalar arası karo ve veri paylaşımı anlıktır (tek yönetici, tek işçi). Dikkat: yönetici içinde '
    'harita adları eşsiz olmalıdır; aynı ad ikinci haritaya verilmez.</p>')

add('<h3>b) İki host eklentisi: ayarlar izole, karolar ortak</h3>')
add(pre('''// host A (veri klasoru kendine ozel):
mgr->setDataDirectory("C:/X-Plane/pluginA/prime-xmap");
mgr->setCacheDirectory("C:/X-Plane/prime-cache");   // ikisi de ayniyi verir
mgr->initialize();

// host B:
mgr->setDataDirectory("C:/X-Plane/pluginB/prime");  // farkli
mgr->setCacheDirectory("C:/X-Plane/prime-cache");   // ayni
mgr->initialize();'''))
add(pre('''&lt;veri-A&gt;/prime_map.json + maps/&lt;ad&gt;/   ayarlar izole
&lt;veri-B&gt;/prime_map.json + maps/&lt;ad&gt;/   ayarlar izole
&lt;ortak&gt;/terrain/...                    karo onbellegi paylasilir'''))
add('<p>Güvenlik notu: önbellek yazımları <b>benzersiz adlı</b> geçici dosya (<code>&lt;yol&gt;.&lt;pid&gt;.&lt;iş&gt;.&lt;sayaç&gt;.tmp</code>) + '
    'yeniden adlandırmayla atomiktir; okuma tarafında anahtar doğrulanır, yarım yazılmış dosya asla görünmez; '
    '<code>CacheTrim</code> yarışması kendini onarır (silinen karo yeniden üretilir) ve çökerten kalan 1 saatten eski '
    'geçici dosyaları siler. Paylaşım bilinçli kurulumsa <code>initialize</code> bunu tek satır bilgi olarak yazar.</p>')

add('<h3>c) Tam izolasyon</h3>')
add(pre('''mgrA->setDataDirectory("C:/X-Plane/pluginA/prime-xmap");   // onbellek kendiliginden
mgrB->setDataDirectory("C:/X-Plane/pluginB/prime-xmap");   // &lt;veri&gt;/cache/ olur'''))
add('<p>Ayarlar da karolar da ayrıdır; iki host hiçbir diske dokunmaz. Bellek / doku bütçeleri yönetici '
    'başına ayrı tutulur (paylaşım yoktur).</p>')

add('<h3>d) Hiçbir klasör verilmezse (varsayılan)</h3>')
add(pre('''// ikinci yonetici de veri klasorune dokunmuyorsa:
// &lt;veri&gt; = %LOCALAPPDATA%/prime/prime-xmap , onbellek = &lt;veri&gt;/cache'''))
add('<p>Risk: iki yöneticinin ayarları çakışır — aynı <code>prime_map.json</code> (farklı değer yazan ezer) ve '
    'aynı adlı <code>maps/&lt;ad&gt;/</code> klasörleri. <code>initialize</code> bunu tek satır WARNING ile '
    'bildirir: <i>give each manager its own data directory (setDataDirectory) or use distinct map names</i>. '
    'Önbellek klasörü de aynı olduğundan ikinci yönetici DSF taramadan önbellekten okur.</p>')

add('<h3>e) Ayar kopyası (klasörlerden bağımsız)</h3>')
add(pre('''mapB->importSettings(mapA->exportSettings());               // gorunum, mod, katmanlar,
                                                            // zoom araliklari, renkler,
                                                            // kaynaklar + pencere/view bolumleri
mapB->importTerrainSettings(mapA->exportTerrainSettings()); // zemin
mapB->importDrawings(mapA->exportDrawings());               // cizimler'''))
add('<p><code>exportSettings</code> pencere (başlık, yer, stil) ve görünüm (merkez, zoom, takip) bölümlerini '
    'de taşır: kopya, kaynağıyla aynı yerde ve aynı görünümde açılır. Bunu istemiyorsanız aktarım sonrası '
    '<code>setGeometry</code> / <code>setCenter</code> / <code>setZoom</code> ile ezin.</p>')

add('<h3>f) Canlı senkron (kütüphanede yoktur)</h3>')
add(pre('''// host tarafinda iki yol:
// 1) her setX cagrisini iki haritaya da yapmak
// 2) revizyonlari izleyip fark degistiyse aktarmak
if (mapA->getSettingsRevision() != tLast) { tLast = mapA->getSettingsRevision(); mapB->importSettings(mapA->exportSettings()); }
if (mapA->getDrawingsRevision() != tLastDrw) { tLastDrw = mapA->getDrawingsRevision(); mapB->importDrawings(mapA->exportDrawings()); }'''))
add('<p>Sınırlar: <code>importSettings</code> pencere / görünümü de kopyalar (yukarıdaki e maddesi); '
    '<code>importDrawings</code> çizimleri bütün olarak değiştirir (artımlı değil); geri yansıma '
    '(<code>mapB</code> değişince <code>mapA</code>\'ya yazmak) sonsuz döngüye açıktır — host bunu engellemeli. '
    'Dizin paylaşımı canlı senkron sağlamaz: ayarlar <code>initialize</code>\'da bir kez okunur.</p>')

add('<h2>15. Sınırlar ve bilinen hatalar</h2>')
add('<ul>')
for li in [
    'Katman zoom aralığı: host aralığı yerleşik eşiğin yerine geçer; aşağı çekme katman başına ölçülmüş sert alt sınıra kadar (bölüm 9.2). Renkler modlar arası ortak; 14 katman dışında renk, hiçbir katmanda kalınlık / boy ayarı yok (bölüm 9).',
    'Log öneki süreç genelinde tektir: aynı süreçte birden çok host varsa son <code>setLogPrefix</code> bütün satırlarda görünür.',
    'Aynı süreçte ikinci yönetici aynı veri klasörüne çözülürse <code>initialize</code> tek satır WARNING, yalnızca önbellek klasörü ortaksa tek satır bilgi yazar (bölüm 14b).',
    'Aynı süreçteki bütün host\'lar aynı <code>prime_map.dll</code>\'yi kullanır (Windows aynı adlı DLL\'i bir kez yükler): sürümler aynı olmalıdır. Farklıysa <code>createXMapManager</code> tek satır WARNING yazar (host built with X, loaded DLL is Y at ...); <code>isLibraryVersionMatching()</code> ile host bunu baştan denetleyebilir.',
    'Her yöneticinin bir zemin işçisi vardır; birbirinden uzak yerlere bakan çok sayıda harita aynı işçiyi paylaşır. Bölge ön önbelleği aynı anda tek alan içindir.',
    'Texture: host penceresi pop-out edilmişse (ayrı GL bağlamı) çizilmez. <code>setXPlaneFont(true)</code> ile etiketler dokuda yanlış yere düşebilir.',
    'LEGACY Embedded stilde harita ayrı bir penceredir: host penceresinin önünde tutmak host\'un işidir ve tıklama / sıralama sorunları çıkar. LEGACY stiller ileride kaldırılacaktır.',
    'Genel başlıklar (<code>XMap.h</code>) kütüphanenin iç başlıklarını da içerir; C4251 uyarısı yalnızca bu sınıflar için kapatılmıştır. Host ile kütüphane aynı MSVC ve <code>/MD</code> ile derlenmelidir.',
    'Bina / 3B nesne yok; 180. boylam civarı kısmen; yazılar ASCII; ILS yönü gerçek (true).',
]:
    add(f'<li>{li}</li>')
add('</ul>')

# 16 ------------------------------------------------------------------------------------------------
add('<h2>16. Sürüm ve geçiş</h2>')
add(f'<p><b>{VERSION}</b> ({DATE}). MVP yayını. 1.0.0: varsayılan harita modu <b>Dark</b> (host eskisi gibi '
    '<code>mMode = Vfr</code> verebilir); katman zoom eşikleri tek bir tabloda toplandı ve host aralığı yerleşik '
    'eşiğin <b>yerine geçti</b> — aşağı çekme katman başına ölçülmüş sert alt sınıra kadar serbest, <code>min &gt; max</code> '
    'reddedilir, <code>(0, 99)</code> sınırlamayı kaldırır; yeni <code>getDefaultLayerZoomRange</code> ve '
    '<code>getLayerHardMinZoom</code>; açık harita yokken uçak konumu çevresi ön yüklenir (<code>setPrefetchTiles(0)</code> '
    'kapatır); önbellek geçici dosyaları benzersiz adlı, çökerten kalanlar CacheTrim tarafından silinir; hazırda '
    '<code>Prime Map API 1.0.0 ready (... dll: ...)</code> satırı yüklü DLL\'in yolunu verir; sürüm uyuşmazlığında '
    'fabrika tek satır WARNING yazar, <code>isLibraryVersionMatching()</code> host\'un kendi denetimidir. Ürün 1.0.0 '
    'ile yayındır: sürümler arasında geriye uyumluluk sağlanmaz.</p>')
add('<p>0.26: <code>WindowStyle::Texture</code> (varsayılan) ve <code>inject*</code>, <code>MouseAction</code>, '
    '<code>MouseButton</code>; Embedded / Standalone LEGACY; INI desteği ve eski sürüm dosya göçü kaldırıldı '
    '(sadece JSON); zemin paleti kaynağı <code>"ini"</code> yerine <code>"colors"</code>; <code>Geometry</code> alt kenar '
    'hatası düzeldi; yol sorguları <code>initialize</code> öncesi çalışır; verilmeyen önbellek ayarları artık 0 yazılmaz. '
    '0.26.2: <code>shutdown</code> sonrası harita metotları güvenli no-op (tek seferlik uyarı); '
    '<code>setColor(TerrainBuiltUp)</code> zemin görüntüsüne işler; aynı klasöre çözülen ikinci yönetici '
    '<code>initialize</code>\'da uyarır; önbellek yazımları atomik.</p>')
add(table(['0.23', '0.24 / 0.25 / 0.26'], [
    ['<code>prime::MapLibrary::Get().Init(InitConfig)</code>', '<code>XMapManagerFactory::createXMapManager()</code> + <code>set...</code> + <code>initialize()</code>'],
    ['<code>MapLibrary::CreateMap()</code> / <code>Map::Open()</code> / <code>Close()</code> / <code>unique_ptr</code> bırakma', '<code>addXMap()</code> / <code>XMap::initialize()</code> / <code>close()</code> / <code>removeXMap()</code>'],
    ['<code>MapMode</code>, <code>MapLayer</code>, <code>LibrarySettings</code>, <code>MapObject</code>, <code>OpenConfig</code>', '<code>XMapMode</code>, <code>XMapLayer</code>, <code>XMapManagerSettings</code>, <code>XMapObject</code>, <code>XMapConfig</code>'],
    ['<code>SetLayerVisible</code>, <code>SetUiVisible</code>, <code>SetMouseInput</code>, <code>VersionString</code> (PascalCase)', '<code>setIsLayerVisible</code>, <code>setIsUiVisible</code>, <code>setIsMouseInputEnabled</code>, <code>versionString</code> (camelCase)'],
    ['<code>.ini</code> dosyaları, veri klasörü host eklenti klasörü', '<code>.json</code> (INI okunmaz); veri klasörü <code>setDataDirectory</code> ya da <code>%LOCALAPPDATA%/prime/prime-xmap</code>'],
    ['CMake hedefi <code>prime_map</code>', '<code>prime-map-api</code> (DLL adı yine <code>prime_map.dll</code>)'],
]))
add('</body>\n</html>\n')

with open(OUT, 'w', encoding='utf-8', newline='\n') as f:
    f.write('\n'.join(H))
print('written', OUT, sum(len(x) for x in H), 'chars')
