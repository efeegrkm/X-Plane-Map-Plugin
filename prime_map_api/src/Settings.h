// Settings.h
// Basit ayar dosyasi (INI bicimi). X-Plane'e bagimli DEGIL.
//
//   # yorum satiri
//   [bolum]
//   anahtar=deger
//
// Sayilar her zaman nokta ile yazilir/okunur (Turkce Windows'ta da 41.5).
#pragma once

#include <map>
#include <string>
#include <vector>

class Settings
{
public:
    // Dosyayi okur. Dosya yoksa false doner ve ayarlar bos kalir
    // (bu bir hata degil: ilk acilista dosya henuz yoktur).
    bool Load(const std::string& pPath);

    // Metinden okuma (dosyasiz; API ice aktarma). LoadText onceki her seyi siler, MergeText ustune yazar.
    void LoadText(const std::string& pText);
    void MergeText(const std::string& pText);
    // Metin olarak (dosya bicimiyle ayni). pSections bos: butun bolumler; "layers*" on ekle eslesir.
    std::string ToText(const std::vector<std::string>& pSections) const;

    // Son Load edilen (veya SetPath ile verilen) dosyaya yazar
    bool Save() const;

    void SetPath(const std::string& pPath);
    // Dosyanin basina yazilacak aciklama (her satir '# ' ile baslar); bossa varsayilan
    void SetComment(const std::string& pComment);
    const std::string& Path() const;

    bool Has(const std::string& pSection, const std::string& pKey) const;

    std::string GetString(const std::string& pSection, const std::string& pKey,
                          const std::string& pDefault) const;
    double GetDouble(const std::string& pSection, const std::string& pKey, double pDefault) const;
    int GetInt(const std::string& pSection, const std::string& pKey, int pDefault) const;
    bool GetBool(const std::string& pSection, const std::string& pKey, bool pDefault) const;

    void SetString(const std::string& pSection, const std::string& pKey, const std::string& pValue);
    void SetDouble(const std::string& pSection, const std::string& pKey, double pValue);
    void SetInt(const std::string& pSection, const std::string& pKey, int pValue);
    void SetBool(const std::string& pSection, const std::string& pKey, bool pValue);
    // Bolumu butun anahtarlariyla siler (bastan yazilacak bolumler icin: [layer_zoom])
    void ClearSection(const std::string& pSection);

private:
    std::string mPath;
    std::string mComment;
    // bolum -> (anahtar -> deger); std::map oldugu icin dosyada alfabetik sirada yazilir
    std::map<std::string, std::map<std::string, std::string>> mValues;
};
