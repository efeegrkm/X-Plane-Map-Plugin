// apt.dat satirini yerel ayardan bagimsiz okuyan alan okuyucu.
#ifndef PRIME_MAP_SRC_DATA_APTLINE_H
#define PRIME_MAP_SRC_DATA_APTLINE_H

#include <string>
#include <string_view>

class AptLine
{
public:
    explicit AptLine(std::string_view pText);

    bool readInt(int& pOut);
    bool readDouble(double& pOut);
    bool token(std::string& pOut);
    bool token(std::string_view& pOut);

    std::string rest();

    bool atEnd();

private:
    void skipSpaces();

    const char* mCursor;
    const char* mEnd;
};

int aptRowCode(std::string_view pLine);
#endif
