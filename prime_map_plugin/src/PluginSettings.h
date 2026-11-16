// PluginSettings.h
// Eklentinin eski INI okuyucusu (0.22 gocu icin; yeni dosyalar API'de JSON yazar). X-Plane'e bagimli DEGIL.
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

class PluginSettings
{
public:
    // Dosyayi okur. Dosya yoksa false doner ve ayarlar bos kalir
    // (bu bir hata degil: ilk acilista dosya henuz yoktur).
    bool load(const std::string& pPath);

    // Metinden okuma (dosyasiz; API ice aktarma). loadText onceki her seyi siler, mergeText ustune yazar.
    void loadText(const std::string& pText);
    void mergeText(const std::string& pText);
    // Metin olarak (dosya bicimiyle ayni). pSections bos: butun bolumler; "layers*" on ekle eslesir.
    std::string toText(const std::vector<std::string>& pSections) const;

    // Son Load edilen (veya setPath ile verilen) dosyaya yazar
    bool save() const;

    void setPath(const std::string& pPath);
    // Dosyanin basina yazilacak aciklama (her satir '# ' ile baslar); bossa varsayilan
    void setComment(const std::string& pComment);
    const std::string& path() const;

    bool has(const std::string& pSection, const std::string& pKey) const;

    std::string getString(const std::string& pSection, const std::string& pKey,
                          const std::string& pDefault) const;
    double getDouble(const std::string& pSection, const std::string& pKey, double pDefault) const;
    int getInt(const std::string& pSection, const std::string& pKey, int pDefault) const;
    bool getBool(const std::string& pSection, const std::string& pKey, bool pDefault) const;

    void setString(const std::string& pSection, const std::string& pKey, const std::string& pValue);
    void setDouble(const std::string& pSection, const std::string& pKey, double pValue);
    void setInt(const std::string& pSection, const std::string& pKey, int pValue);
    void setBool(const std::string& pSection, const std::string& pKey, bool pValue);

private:
    std::string mPath;
    std::string mComment;
    // bolum -> (anahtar -> deger); std::map oldugu icin dosyada alfabetik sirada yazilir
    std::map<std::string, std::map<std::string, std::string>> mValues;
};
