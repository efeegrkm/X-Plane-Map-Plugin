// AptLine.h
// apt.dat'in tek bir satirini soldan saga okuyan kucuk yardimci.
// X-Plane'e bagimli DEGIL.
//
// Alanlar bir veya daha fazla bosluk/tab ile ayrilir. Sayilar yerel ayardan
// bagimsiz okunur (ondalik ayirici hep nokta): strtod Turkce Windows'ta
// virgul bekleyebilir.
//
//   AptLine tLine("100 29.87 1 0 0.15 ...");
//   int tCode = 0;
//   double tWidth = 0.0;
//   if (tLine.Int(tCode) && tLine.Double(tWidth)) ...
#pragma once

#include <string>
#include <string_view>

class AptLine
{
public:
    explicit AptLine(std::string_view pText);

    // Siradaki alani okur; alan yoksa veya bicim yanlissa false (konum ilerlemez)
    bool Int(int& pOut);
    bool Double(double& pOut);
    bool Token(std::string& pOut);
    bool Token(std::string_view& pOut);

    // Satirin geri kalani, bas/son bosluk atilmis (ad gibi bosluk iceren alanlar)
    std::string Rest();

    bool AtEnd();  // geriye sadece bosluk kaldi mi

private:
    void SkipSpaces();

    const char* mCursor;
    const char* mEnd;
};

// Satirin ilk alanini (satir kodu) okur. Bos satir / yorum icin -1.
int AptRowCode(std::string_view pLine);
