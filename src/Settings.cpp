// Settings.cpp
#include "Settings.h"

#include <filesystem>
#include <fstream>
#include <locale>
#include <sstream>

namespace
{

// Bastaki ve sondaki bosluklari at
std::string Trim(const std::string& pText)
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

bool Settings::Load(const std::string& pPath)
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
    MergeText(tText.str());
    return true;
}

void Settings::LoadText(const std::string& pText)
{
    mValues.clear();
    MergeText(pText);
}

void Settings::MergeText(const std::string& pText)
{
    std::istringstream tLines(pText);
    std::string tSection;
    std::string tLine;
    while (std::getline(tLines, tLine))
    {
        tLine = Trim(tLine);
        if (tLine.empty() || tLine[0] == '#' || tLine[0] == ';')
        {
            continue;
        }
        if (tLine.front() == '[' && tLine.back() == ']')
        {
            tSection = Trim(tLine.substr(1, tLine.size() - 2));
            continue;
        }
        const size_t tEquals = tLine.find('=');
        if (tEquals == std::string::npos)
        {
            continue;  // taninmayan satir: atla
        }
        const std::string tKey = Trim(tLine.substr(0, tEquals));
        const std::string tValue = Trim(tLine.substr(tEquals + 1));
        if (!tKey.empty())
        {
            mValues[tSection][tKey] = tValue;
        }
    }
}

std::string Settings::ToText(const std::vector<std::string>& pSections) const
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
        tOut << "# Prime Map Plugin settings. Can be edited by hand while X-Plane is closed.\n";
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

bool Settings::Save() const
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
        tFile << ToText({});
        if (!tFile)
        {
            return false;
        }
    }
    std::error_code tError;
    std::filesystem::rename(tTemporary, mPath, tError);
    return !tError;
}

void Settings::SetPath(const std::string& pPath)
{
    mPath = pPath;
}

void Settings::SetComment(const std::string& pComment)
{
    mComment = pComment;
}

const std::string& Settings::Path() const
{
    return mPath;
}

bool Settings::Has(const std::string& pSection, const std::string& pKey) const
{
    const auto tSection = mValues.find(pSection);
    return tSection != mValues.end() && tSection->second.count(pKey) > 0;
}

std::string Settings::GetString(const std::string& pSection, const std::string& pKey,
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

double Settings::GetDouble(const std::string& pSection, const std::string& pKey, double pDefault) const
{
    if (!Has(pSection, pKey))
    {
        return pDefault;
    }
    // "C" yerel ayariyla oku: ondalik ayirici her zaman nokta
    std::istringstream tStream(GetString(pSection, pKey, ""));
    tStream.imbue(std::locale::classic());
    double tValue = pDefault;
    tStream >> tValue;
    return tStream.fail() ? pDefault : tValue;
}

int Settings::GetInt(const std::string& pSection, const std::string& pKey, int pDefault) const
{
    if (!Has(pSection, pKey))
    {
        return pDefault;
    }
    std::istringstream tStream(GetString(pSection, pKey, ""));
    tStream.imbue(std::locale::classic());
    int tValue = pDefault;
    tStream >> tValue;
    return tStream.fail() ? pDefault : tValue;
}

bool Settings::GetBool(const std::string& pSection, const std::string& pKey, bool pDefault) const
{
    if (!Has(pSection, pKey))
    {
        return pDefault;
    }
    const std::string tValue = GetString(pSection, pKey, "");
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

void Settings::SetString(const std::string& pSection, const std::string& pKey, const std::string& pValue)
{
    mValues[pSection][pKey] = pValue;
}

void Settings::SetDouble(const std::string& pSection, const std::string& pKey, double pValue)
{
    std::ostringstream tStream;
    tStream.imbue(std::locale::classic());
    tStream.precision(10);
    tStream << pValue;
    SetString(pSection, pKey, tStream.str());
}

void Settings::SetInt(const std::string& pSection, const std::string& pKey, int pValue)
{
    SetString(pSection, pKey, std::to_string(pValue));
}

void Settings::SetBool(const std::string& pSection, const std::string& pKey, bool pValue)
{
    SetString(pSection, pKey, pValue ? "1" : "0");
}
