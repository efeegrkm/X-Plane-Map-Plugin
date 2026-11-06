// LandColorField.h
// Zemin dokusunda arazi siniflari arasi yumusak gecis icin kara renklerinin
// yumusatilmis alani (normalize evrisim). X-Plane'e bagimli DEGIL.
//
// Neden: DSF'te arazi turu ucgen basina atanmis; duz renkle boyaninca ucgenler
// keskin mozaik olarak gorunur. X-Plane 3D sahnede bunu yuklemede urettigi kenar
// dokulariyla yumusatir (DSF'te yok). Burada sadece KARA renkleri sabit bir yer
// mesafesinde karistirilir: su ve kiyi keskin kalir, golge sonradan eklenir.
//
// Izgara: dokunun (payli) piksellerinin pFactor x pFactor bloklari tek hucre.
// Buyuk erimde (seviye 5) hucre basina birden cok piksel dusurulur; sonuc zaten
// yumusak oldugu icin cift dogrusal okuma farki gorunmez.
#pragma once

#include <vector>

struct TerrainColor;

// Bir doku icin yumusatma plani. Bos plan (mPad == 0) = yumusatma yok.
struct LandBlendPlan
{
    int mPad = 0;      // dokunun her kenarina eklenen pay (cikti pikseli)
    int mFactor = 1;   // izgara hucresi = mFactor x mFactor piksel
    int mHalfX = 0;    // kutu yari genisligi (hucre), dogu-bati
    int mHalfY = 0;    // kutu yari genisligi (hucre), kuzey-guney
};

// pMeters: [blend] meters. pMetersPerPixelX/Y: dokunun bir pikselinin yerdeki boyu.
// Erim 1,5 pikselden kucukse bos plan (uzak seviyelerde filtre kendiliginden kapanir).
LandBlendPlan PlanLandBlend(double pMeters, double pMetersPerPixelX, double pMetersPerPixelY,
                            double pMaxReachPixels);

class LandColorField
{
public:
    // pPixels: payli dokunun kenari (piksel). Hucre sayisi = ceil(pPixels / pFactor).
    void Reset(int pPixels, int pFactor);

    // (pPixelX, pPixelY) payli dokunun pikseli; pWeight: o pikseldeki kara ornek sayisi
    void Add(int pPixelX, int pPixelY, const TerrainColor& pColor, float pWeight);

    // 3 x (yatay + dikey) kutu bulanikligi (~Gaussian). Kenar disi = veri yok.
    void Blur(int pHalfX, int pHalfY);

    // Payli dokunun piksel MERKEZINDEKI yumusatilmis kara rengi (cift dogrusal).
    // Cevrede hic kara yoksa false.
    bool ColorAt(int pPixelX, int pPixelY, double pRgb[3]) const;

private:
    static void BoxPass(std::vector<float>& pData, int pCells, int pHalf, bool pHorizontal);

    int mCells = 0;
    int mFactor = 1;
    std::vector<float> mData;  // hucre basina 4 float: R*w, G*w, B*w toplami ve w toplami
};
