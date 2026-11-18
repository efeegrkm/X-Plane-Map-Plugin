// Bolumlu anahtar-deger ayar deposu; JSON olarak okunur ve yazilir.
#ifndef PRIME_MAP_SRC_SETTINGS_H
#define PRIME_MAP_SRC_SETTINGS_H

#include <map>
#include <string>
#include <vector>

class Settings
{
public:
    bool load(const std::string& pPath);

    void loadText(const std::string& pText);
    void mergeText(const std::string& pText);
    std::string toText(const std::vector<std::string>& pSections) const;

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
    void clearSection(const std::string& pSection);

    const std::map<std::string, std::string>* findSection(const std::string& pSection) const;

private:
    std::string mPath;
    std::map<std::string, std::map<std::string, std::string>> mValues;
};
#endif
