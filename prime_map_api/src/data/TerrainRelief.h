// TerrainRelief.h
// Zemin goruntusunun piksel formulleri (0.16, Asama 1). Saf fonksiyonlar:
// girdi -> sayi; X-Plane'e ve OpenGL'e bagimli DEGIL, testli.
//
//   Kabartma  : egim -> parlaklik carpani. Cok yonlu isik (225/270/315/360
//               derece, agirlik 315'te); tek yonlu isikta bazi yamaclar
//               dumduz gorunur, cok yonlu isik bunu azaltir. Agirlik
//               kuzeybatida kalir: isik guneyden gelirse goz sirtlari vadi
//               sanir (ters kabartma yanilsamasi).
//   VFR       : rakim -> sectional bandi. Bant siniri piksel basina bikubik
//               rakimdan hesaplanir; sinira yarim pikselden yakin piksel iki
//               bandin karisimidir (kenar yumusatma).
//   Su        : sea_level derinligi -> koyulasma; kiyiya uzaklik -> acik serit.
//   Kiyi      : kara orani alanindan (0..1) suyun tarafinda tek piksel sinir cizgisi.
//
//      kara orani   1  1  1  1 | 0  0  0       cizgi   0  0  0  0 | 1  0  0
//                   (kara)     | (su)                  sadece kiyiya degen su pikseli
#pragma once

#include <cstdint>
#include <vector>

#include "data/TerrainClasses.h"

// Bir seviyenin egim abartmasi: mExaggeration * (pCellMeters / pFinestCellMeters) ^ mCoarseBoost.
// Kaba seviyede egimler ortalamayla yumusar; hafif artis uzak gorunumu duz birakmaz.
double ReliefExaggeration(const TerrainReliefParams& pParams, double pCellMeters, double pFinestCellMeters);

// Kabartma parlaklik carpani (0.45 .. 1.55). pSlopeEast / pSlopeNorth: yuzeyin dogu ve
// kuzey yonundeki egimi (metre / metre, abartmasiz). pStrength: [shading] strength.
// mFlatSlope'un altindaki egimler bastirilir (DEM'in 1 m basamaklari dalga gibi gorunmesin).
double ReliefFactor(double pSlopeEast, double pSlopeNorth, double pStrength, double pExaggeration,
                    const TerrainReliefParams& pParams);

// VFR bandi rengi (0..255, pRgb[3]). pFeetPerPixel: rakimin bir pikselde ne kadar degistigi
// (feet); 0 ise kenar yumusatma ve kontur yok.
void VfrBandColor(double pMeters, double pFeetPerPixel, const TerrainVfrParams& pParams, double pRgb[3]);

// Derinlik koyulasma orani 0..1. pSeaLevel: sea_level (metre). -10 m ve ustu (veri yok,
// gol, kiyi) 0; derinlestikce mDepthFullMeters'a kadar artar.
double DepthDarkening(double pSeaLevel, const TerrainWaterParams& pParams);

// Kiyi seridi aciklik orani 0..1: kiyi cizgisinin (1 piksel) arkasinda en guclu, pBandPixels sonra 0
// (karesel azalir)
double CoastBandLightening(double pDistancePixels, double pBandPixels, const TerrainWaterParams& pParams);

// Her pikselin en yakin kara pikseline uzakligi (piksel; 3-4 chamfer, ~%8 hata).
// pLand: pSize x pSize, 1 = kara. pMaxDistance'tan uzak olanlar pMaxDistance.
void DistanceToLand(const std::vector<uint8_t>& pLand, int pSize, float pMaxDistance, std::vector<float>* pOut);

// Kiyi cizgisi gucu 0..1. pLandFraction: pSize x pSize kara orani (0..1; < 0 = veri yok).
// Suyun tarafinda tek piksel: 4 komsusundan biri kara olan su pikseli; karisik piksel orana gore.
void CoastlineStrength(const std::vector<float>& pLandFraction, int pSize, std::vector<float>* pOut);
