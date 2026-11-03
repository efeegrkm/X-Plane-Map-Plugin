# XPLMMap prototipi (sürüm 0.3) — arşiv

Bu klasör, X-Plane'in kendi haritasına `XPLMMap` katmanı ekleyen ilk
prototipin olduğu gibi saklanmış kopyasıdır. **Derlemeye dahil değildir.**

Neden bırakıldı (ayrıntı: proje kökündeki `GOREV.md`, Bölüm 2):
- X-Plane harita penceresinin yan bölmesi SDK'dan kaldırılamıyor.
- X-Plane'in kendi katmanları (navaid, hava yolu...) SDK'dan açılıp kapatılamıyor.
- Harita katmanları fare olayı almıyor; tıklama için görünmez pencere gerekiyordu.

Yeni yaklaşım: kendi penceremiz (`XPLMCreateWindowEx`) + kendi OpenGL çizimimiz.

İçerik:
- `src/`, `tests/`, `CMakeLists.txt`, `README.md`: 0.3 kaynakları (değiştirilmedi)
- `prebuilt/`: 0.3'ün hazır derlemesi
