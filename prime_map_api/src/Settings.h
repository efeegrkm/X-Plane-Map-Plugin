// Ayar dosyasi (JSON bicimi): { "bolum": { "anahtar": "deger" } }. Degerler tirnak icinde
// saklanir; JSON olmayan metin yok sayilir.
// X-Plane'e bagimli DEGIL. Sayilar her zaman nokta ile yazilir/okunur
// ("C" yerel ayari; Turkce Windows'ta da 41.5).
#pragma once

#include <map>
#include <string>
#include <vector>

class Settings
{
public:
    // Dosyayi okur. Dosya yoksa false doner ve ayarlar bos kalir
    // (bu bir hata degil: ilk acilista dosya henuz yoktur).
    bool load(const std::string& pPath);

    // Metinden okuma (dosyasiz; API ice aktarma). loadText onceki her seyi siler, mergeText ustune yazar.
    void loadText(const std::string& pText);
    void mergeText(const std::string& pText);
    // Metin olarak (JSON). pSections bos: butun bolumler; "layers*" on ekle eslesir.
    std::string toText(const std::vector<std::string>& pSections) const;

    // Son Load edilen (veya setPath ile verilen) dosyaya yazar
    bool save() const;

    void setPath(const std::string& pPath);
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
    // Bolumu butun anahtarlariyla siler (bastan yazilacak bolumler icin: [layer_zoom])
    void clearSection(const std::string& pSection);

    // Bolumun anahtar/deger ciftleri (yoksa nullptr); API [layer_colors] okumasi icin
    const std::map<std::string, std::string>* findSection(const std::string& pSection) const;

private:
    std::string mPath;
    // bolum -> (anahtar -> deger); std::map oldugu icin dosyada alfabetik sirada yazilir
    std::map<std::string, std::map<std::string, std::string>> mValues;
};
