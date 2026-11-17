// apt.dat'in tek bir satirini soldan saga okuyan kucuk yardimci.
// X-Plane'e bagimli DEGIL.
//
// Alanlar bir veya daha fazla bosluk/tab ile ayrilir. Sayilar yerel ayardan
// bagimsiz okunur (ondalik ayirici hep nokta): strtod Turkce Windows'ta
// virgul bekleyebilir.
//
//   AptLine tLine("100 29.87 1 0 0.15 ...");
//   if (tLine.readInt(tCode) && tLine.readDouble(tWidth)) ...
#ifndef PRIME_MAP_SRC_DATA_APTLINE_H
#define PRIME_MAP_SRC_DATA_APTLINE_H

#include <string>
#include <string_view>

class AptLine
{
public:
    explicit AptLine(std::string_view pText);

    // Siradaki alani okur; alan yoksa veya bicim yanlissa false (konum ilerlemez)
    bool readInt(int& pOut);
    bool readDouble(double& pOut);
    bool token(std::string& pOut);
    bool token(std::string_view& pOut);

    // Satirin geri kalani, bas/son bosluk atilmis (ad gibi bosluk iceren alanlar)
    std::string rest();

    bool atEnd();  // geriye sadece bosluk kaldi mi

private:
    void skipSpaces();

    const char* mCursor;
    const char* mEnd;
};

// Satirin ilk alanini (satir kodu) okur. Bos satir / yorum icin -1.
int aptRowCode(std::string_view pLine);
#endif  // PRIME_MAP_SRC_DATA_APTLINE_H
