// Overlay.h
// Tiklama penceresi + buton paneli ile X-Plane haritasini senkron tutar.
//
// X-Plane haritanin ekrandaki yerini bize SOYLEMIYOR. Bu yuzden her karede:
//   1. Katmanimizin draw callback'i haritanin OpenGL matrisini ve gorunen
//      alanini kaydeder -> haritanin ekrandaki dikdortgeni (piksel) bulunur.
//   2. Piksel dikdortgeni boxel'e cevrilir (UI olcekleme dahil).
//   3. Tiklama penceresi tam bu dikdortgene, panel haritanin yanina konur.
//   4. Harita kapandiysa (birkac karedir cizilmediyse) ikisi de gizlenir.
//   5. Isaretleme modunda pencereler haritanin onunde tutulur.
#pragma once

namespace Overlay {

void Init();
void Shutdown();

// Her karede flight loop'tan cagrilir: gorunurluk, konum, on-arka sirasi
void Update();

// Harita her cizildiginde cagrilir. Flight loop calismiyorsa (ornegin pause)
// sadece konumlari gunceller.
void OnMapDrawn();

// Tiklama penceresinden gelen tiklama (boxel)
void HandleMapClick(int x, int y);

void SetDiagnostics(bool on);
bool Diagnostics();

// Panelde gosterilecek uyari (bos olabilir)
const char* StatusText();

}  // namespace Overlay
