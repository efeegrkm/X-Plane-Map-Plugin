stb_truetype.h - TrueType yazi tipi okuma ve harf rasterlestirme (dis kutuphane, YAMASIZ)

Kaynak    : https://github.com/nothings/stb (stb_truetype.h)
Surum     : v1.26, commit f58f558c120e9b32c217290b80bad1a0729fbb2c
Lisans    : public domain (Unlicense) veya MIT, dosyanin sonunda
Indirildi : 2026-11-07

Ne icin kullaniliyor:
  Kendi yazi cizicimiz (src/gfx/FontAtlas.cpp, 0.18+). X-Plane'in kendi
  Resources/fonts/DejaVuSans.ttf dosyasi calisma aninda okunur, harfler bir
  doku atlasina basilir; etiketler bu atlastan tek cizim cagrisiyla cizilir.
  Yazi tipi dosyasini biz dagitmiyoruz (X-Plane kurulumunda zaten var).

Kullanim: sadece FontAtlas.cpp icinde STB_TRUETYPE_IMPLEMENTATION ile derlenir.
