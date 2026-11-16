// Settings.cpp
#include "Settings.h"

#include <filesystem>
#include <fstream>
#include <locale>
#include <sstream>

namespace
{

// Bastaki ve sondaki bosluklari at
std::string trim(const std::string& pText)
{
    const char* tSpaces = " \t\r\n";
    const size_t tBegin = pText.find_first_not_of(tSpaces);
    if (tBegin == std::string::npos)
    {
        return "";
    }
    const size_t tEnd = pText.find_last_not_of(tSpaces);
    return pText.substr(tBegin, tEnd - tBegin + 1);
}

}  // namespace

bool Settings::load(const std::string& pPath)
{
    mPath = pPath;
    mValues.clear();

    std::ifstream tFile(pPath);
    if (!tFile)
    {
        return false;
    }
    std::ostringstream tText;
    tText << tFile.rdbuf();
    mergeText(tText.str());
    return true;
}

void Settings::loadText(const std::string& pText)
{
    mValues.clear();
    mergeText(pText);
}

void Settings::mergeText(const std::string& pText)
{
    std::istringstream tLines(pText);
    std::string tSection;
    std::string tLine;
    while (std::getline(tLines, tLine))
    {
        tLine = trim(tLine);
        if (tLine.empty() || tLine[0] == '#' || tLine[0] == ';')
        {
            continue;
        }
        if (tLine.front() == '[' && tLine.back() == ']')
        {
            tSection = trim(tLine.substr(1, tLine.size() - 2));
            continue;
        }
        const size_t tEquals = tLine.find('=');
        if (tEquals == std::string::npos)
        {
            continue;  // taninmayan satir: atla
        }
        const std::string tKey = trim(tLine.substr(0, tEquals));
        const std::string tValue = trim(tLine.substr(tEquals + 1));
        if (!tKey.empty())
        {
            mValues[tSection][tKey] = tValue;
        }
    }
}

std::string Settings::toText(const std::vector<std::string>& pSections) const
{
    // pSections bos: hepsi. "layers*" gibi sonu '*' olan oge on ek olarak eslesir.
    auto tWanted = [&pSections](const std::string& pName)
    {
        if (pSections.empty())
        {
            return true;
        }
        for (const std::string& tPattern : pSections)
        {
            if (!tPattern.empty() && tPattern.back() == '*')
            {
                if (pName.compare(0, tPattern.size() - 1, tPattern, 0, tPattern.size() - 1) == 0)
                {
                    return true;
                }
            }
            else if (pName == tPattern)
            {
                return true;
            }
        }
        return false;
    };
    std::ostringstream tOut;
    if (mComment.empty())
    {
        tOut << "# Prime XMap Plugin settings. Can be edited by hand while X-Plane is closed.\n";
    }
    else
    {
        std::istringstream tLines(mComment);
        std::string tLine;
        while (std::getline(tLines, tLine))
        {
            tOut << "# " << tLine << "\n";
        }
    }
    for (const auto& tSection : mValues)
    {
        if (!tWanted(tSection.first))
        {
            continue;
        }
        tOut << "\n[" << tSection.first << "]\n";
        for (const auto& tEntry : tSection.second)
        {
            tOut << tEntry.first << "=" << tEntry.second << "\n";
        }
    }
    return tOut.str();
}

bool Settings::save() const
{
    if (mPath.empty())
    {
        return false;
    }
    // Once gecici dosyaya, sonra yeniden adlandir: yarim yazilmis dosya kalmasin
    const std::string tTemporary = mPath + ".tmp";
    {
        std::ofstream tFile(tTemporary, std::ios::trunc);
        if (!tFile)
        {
            return false;
        }
        tFile << toText({});
        if (!tFile)
        {
            return false;
        }
    }
    std::error_code tError;
    std::filesystem::rename(tTemporary, mPath, tError);
    return !tError;
}

void Settings::setPath(const std::string& pPath)
{
    mPath = pPath;
}

void Settings::setComment(const std::string& pComment)
{
    mComment = pComment;
}

const std::string& Settings::path() const
{
    return mPath;
}

bool Settings::has(const std::string& pSection, const std::string& pKey) const
{
    const auto tSection = mValues.find(pSection);
    return tSection != mValues.end() && tSection->second.count(pKey) > 0;
}

std::string Settings::getString(const std::string& pSection, const std::string& pKey,
                                const std::string& pDefault) const
{
    const auto tSection = mValues.find(pSection);
    if (tSection == mValues.end())
    {
        return pDefault;
    }
    const auto tEntry = tSection->second.find(pKey);
    if (tEntry == tSection->second.end())
    {
        return pDefault;
    }
    return tEntry->second;
}

double Settings::getDouble(const std::string& pSection, const std::string& pKey, double pDefault) const
{
    if (!has(pSection, pKey))
    {
        return pDefault;
    }
    // "C" yerel ayariyla oku: ondalik ayirici her zaman nokta
    std::istringstream tStream(getString(pSection, pKey, ""));
    tStream.imbue(std::locale::classic());
    double tValue = pDefault;
    tStream >> tValue;
    return tStream.fail() ? pDefault : tValue;
}

int Settings::getInt(const std::string& pSection, const std::string& pKey, int pDefault) const
{
    if (!has(pSection, pKey))
    {
        return pDefault;
    }
    std::istringstream tStream(getString(pSection, pKey, ""));
    tStream.imbue(std::locale::classic());
    int tValue = pDefault;
    tStream >> tValue;
    return tStream.fail() ? pDefault : tValue;
}

bool Settings::getBool(const std::string& pSection, const std::string& pKey, bool pDefault) const
{
    if (!has(pSection, pKey))
    {
        return pDefault;
    }
    const std::string tValue = getString(pSection, pKey, "");
    if (tValue == "1" || tValue == "yes" || tValue == "true")
    {
        return true;
    }
    if (tValue == "0" || tValue == "no" || tValue == "false")
    {
        return false;
    }
    return pDefault;
}

void Settings::setString(const std::string& pSection, const std::string& pKey, const std::string& pValue)
{
    mValues[pSection][pKey] = pValue;
}

void Settings::setDouble(const std::string& pSection, const std::string& pKey, double pValue)
{
    std::ostringstream tStream;
    tStream.imbue(std::locale::classic());
    tStream.precision(10);
    tStream << pValue;
    setString(pSection, pKey, tStream.str());
}

void Settings::setInt(const std::string& pSection, const std::string& pKey, int pValue)
{
    setString(pSection, pKey, std::to_string(pValue));
}

void Settings::setBool(const std::string& pSection, const std::string& pKey, bool pValue)
{
    setString(pSection, pKey, pValue ? "1" : "0");
}
