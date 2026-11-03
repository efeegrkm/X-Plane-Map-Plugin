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

class Settings
{
public:
    // Dosyayi okur. Dosya yoksa false doner ve ayarlar bos kalir
    // (bu bir hata degil: ilk acilista dosya henuz yoktur).
    bool Load(const std::string& pPath);

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

private:
    std::string mPath;
    std::string mComment;
    // bolum -> (anahtar -> deger); std::map oldugu icin dosyada alfabetik sirada yazilir
    std::map<std::string, std::map<std::string, std::string>> mValues;
};
