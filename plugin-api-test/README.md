# plugin-api-test

Prime XMap API'yi X-Plane 11 içinde deneyen test eklentisi. Sürüm 1.1.0.

Eklenti taşınabilir ve boyutlandırılabilir bir pencere açar ve haritayı `WindowStyle::Texture` ile bu pencerenin içine basar. Fare, tekerlek ve klavye girdileri haritaya `inject*` fonksiyonlarıyla iletilir. Pencerenin düğmeleri ve menü, API'nin bütün çağrılarını tek tek dener.

| Dosya | İçerik |
|---|---|
| `src/XMapUiTestMain.cpp` | X-Plane giriş noktaları, pencere ve düğmeler |
| `src/XMapApiCalls.h`, `src/XMapApiCalls.cpp` | API'nin her çağrısı ayrı bir metotta |
| `src/MultiXMapSelfTest.h`, `src/MultiXMapSelfTest.cpp` | Çoklu harita, fabrika ve ayar dosyalarının otomatik testi |

Eklenti kütüphaneye kendi klasörünü veri klasörü olarak verir: `<X-Plane>\Resources\plugins\plugin-api-test\`. Ayarlar ve önbellek orada oluşur.

## Otomatik test

Eklenti klasörüne boş bir `selftest.flag` dosyası konur ve X-Plane açılır. `Log.txt`'de her kontrol için `SELFTEST PASS` ya da `SELFTEST FAIL` satırı ve sonunda `SELFTEST DONE: N passed, M failed` satırı görünür. Dosya silindiğinde test çalışmaz.

## Derleme ve kurulum

`prime_map_api` bu projenin kardeş klasöründe olmalıdır; başka bir yerdeyse `-DPRIME_MAP_DIR` ile verilir.

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DMAP_DEPLOY=ON
cmake --build build
```

`MAP_DEPLOY=ON` derleme sonunda `win.xpl` ve `prime_map.dll` dosyalarını `<X-Plane>\Resources\plugins\plugin-api-test\64\` klasörüne kopyalar. Kopyalama öncesinde X-Plane kapatılmalıdır. Resmî eklentiyle birlikte kullanılıyorsa iki eklenti aynı kütüphane sürümüyle derlenmelidir.
