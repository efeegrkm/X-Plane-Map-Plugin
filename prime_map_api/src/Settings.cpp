#include "Settings.h"

#include <cstdio>
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

// JSON dizgi kacisleri: "\\" ve '"' zorunlu, kontrol karakterleri \n \t \r biciminde
std::string jsonEscape(const std::string& pText)
{
    std::string tOut;
    tOut.reserve(pText.size() + 8);
    for (const char tChar : pText)
    {
        switch (tChar)
        {
        case '"':
            tOut += "\\\"";
            break;
        case '\\':
            tOut += "\\\\";
            break;
        case '\n':
            tOut += "\\n";
            break;
        case '\t':
            tOut += "\\t";
            break;
        case '\r':
            tOut += "\\r";
            break;
        default:
            if (static_cast<unsigned char>(tChar) < 0x20)
            {
                char tBuffer[8];
                std::snprintf(tBuffer, sizeof(tBuffer), "\\u%04x", tChar);
                tOut += tBuffer;
            }
            else
            {
                tOut += tChar;
            }
            break;
        }
    }
    return tOut;
}

// JSON string sabitini okur; pIndex tirnagi astiktan sonra cikar. Okuyamazsa false.
bool readJsonString(const std::string& pText, size_t& pPos, std::string* pOut)
{
    if (pPos >= pText.size() || pText[pPos] != '"')
    {
        return false;
    }
    ++pPos;
    std::string tOut;
    while (pPos < pText.size())
    {
        const char tChar = pText[pPos++];
        if (tChar == '"')
        {
            *pOut = tOut;
            return true;
        }
        if (tChar != '\\')
        {
            tOut += tChar;
            continue;
        }
        if (pPos >= pText.size())
        {
            return false;
        }
        const char tEscape = pText[pPos++];
        switch (tEscape)
        {
        case '"':
        case '\\':
        case '/':
            tOut += tEscape;
            break;
        case 'b':
            tOut += '\b';
            break;
        case 'f':
            tOut += '\f';
            break;
        case 'n':
            tOut += '\n';
            break;
        case 'r':
            tOut += '\r';
            break;
        case 't':
            tOut += '\t';
            break;
        case 'u':
        {
            // \uXXXX -> UTF-8 (temel cokdilli duzlem yeterli; surrogat desteklenmez)
            if (pPos + 4 > pText.size())
            {
                return false;
            }
            const int tCode = std::stoi(pText.substr(pPos, 4), nullptr, 16);
            pPos += 4;
            if (tCode < 0x80)
            {
                tOut += static_cast<char>(tCode);
            }
            else if (tCode < 0x800)
            {
                tOut += static_cast<char>(0xC0 | (tCode >> 6));
                tOut += static_cast<char>(0x80 | (tCode & 0x3F));
            }
            else
            {
                tOut += static_cast<char>(0xE0 | (tCode >> 12));
                tOut += static_cast<char>(0x80 | ((tCode >> 6) & 0x3F));
                tOut += static_cast<char>(0x80 | (tCode & 0x3F));
            }
            break;
        }
        default:
            return false;
        }
    }
    return false;
}

// JSON'u iki seviyede okur: { "bolum": { "anahtar": deger } }. Deger metin/sayi/true/false olabilir;
// sayi ve true/false ham metinleri saklanir (getInt/getBool aynen cozer). Bilinmeyen yapi atlanir.
void parseJson(const std::string& pText,
               std::map<std::string, std::map<std::string, std::string>>* pOut)
{
    size_t tPos = pText.find('{');
    if (tPos == std::string::npos)
    {
        return;
    }
    ++tPos;
    while (tPos < pText.size())
    {
        tPos = pText.find_first_not_of(" \t\r\n,", tPos);
        if (tPos == std::string::npos || pText[tPos] == '}')
        {
            return;
        }
        std::string tSection;
        if (!readJsonString(pText, tPos, &tSection))
        {
            return;
        }
        tPos = pText.find_first_not_of(" \t\r\n", tPos);
        if (tPos == std::string::npos || pText[tPos] != ':')
        {
            return;
        }
        ++tPos;
        tPos = pText.find_first_not_of(" \t\r\n", tPos);
        if (tPos == std::string::npos || pText[tPos] != '{')
        {
            return;  // bolum olmayan deger: yok say
        }
        ++tPos;
        while (tPos < pText.size())
        {
            tPos = pText.find_first_not_of(" \t\r\n,", tPos);
            if (tPos == std::string::npos || pText[tPos] == '}')
            {
                break;
            }
            std::string tKey;
            if (!readJsonString(pText, tPos, &tKey))
            {
                break;
            }
            tPos = pText.find_first_not_of(" \t\r\n", tPos);
            if (tPos == std::string::npos || pText[tPos] != ':')
            {
                break;
            }
            ++tPos;
            tPos = pText.find_first_not_of(" \t\r\n", tPos);
            if (tPos == std::string::npos)
            {
                break;
            }
            if (pText[tPos] == '"')
            {
                std::string tValue;
                if (readJsonString(pText, tPos, &tValue) && !tKey.empty())
                {
                    (*pOut)[tSection][tKey] = tValue;
                }
            }
            else  // sayi / true / false / null: kapayiciya kadar ham metin
            {
                const size_t tEnd = pText.find_first_of(",} \t\r\n", tPos);
                const size_t tStop = tEnd == std::string::npos ? pText.size() : tEnd;
                if (!tKey.empty() && tStop > tPos)
                {
                    (*pOut)[tSection][tKey] = pText.substr(tPos, tStop - tPos);
                }
                tPos = tStop;
            }
        }
        tPos = pText.find('}', tPos);
        if (tPos == std::string::npos)
        {
            return;
        }
        ++tPos;
    }
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

// JSON (yeni bicim) ya da eski INI: metin '{' ile basliyorsa JSON say.
void Settings::mergeText(const std::string& pText)
{
    const size_t tFirst = pText.find_first_not_of(" \t\r\n");
    if (tFirst == std::string::npos)
    {
        return;
    }
    if (pText[tFirst] == '{')
    {
        parseJson(pText, &mValues);
        return;
    }
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
    // JSON: { "bolum": { "anahtar": "deger" } }; std::map sayesinde alfabetik sirada
    std::ostringstream tOut;
    tOut << "{\n";
    bool tFirstSection = true;
    for (const auto& tSection : mValues)
    {
        if (!tWanted(tSection.first))
        {
            continue;
        }
        if (!tFirstSection)
        {
            tOut << ",\n";
        }
        tFirstSection = false;
        tOut << "  \"" << jsonEscape(tSection.first) << "\": {\n";
        bool tFirstEntry = true;
        for (const auto& tEntry : tSection.second)
        {
            if (!tFirstEntry)
            {
                tOut << ",\n";
            }
            tFirstEntry = false;
            tOut << "    \"" << jsonEscape(tEntry.first) << "\": \"" << jsonEscape(tEntry.second) << "\"";
        }
        tOut << "\n  }";
    }
    tOut << "\n}\n";
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

void Settings::clearSection(const std::string& pSection)
{
    mValues.erase(pSection);
}

const std::map<std::string, std::string>* Settings::findSection(const std::string& pSection) const
{
    const auto tIt = mValues.find(pSection);
    return tIt == mValues.end() ? nullptr : &tIt->second;
}
