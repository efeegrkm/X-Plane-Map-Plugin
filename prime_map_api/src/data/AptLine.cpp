#include "data/AptLine.h"

namespace
{

bool isSpace(char pChar)
{
    return pChar == ' ' || pChar == '\t' || pChar == '\r' || pChar == '\n';
}

bool isDigit(char pChar)
{
    return pChar >= '0' && pChar <= '9';
}

}  // namespace

AptLine::AptLine(std::string_view pText)
    : mCursor(pText.data())
    , mEnd(pText.data() + pText.size())
{
}

void AptLine::skipSpaces()
{
    while (mCursor < mEnd && isSpace(*mCursor))
    {
        ++mCursor;
    }
}

bool AptLine::readInt(int& pOut)
{
    skipSpaces();
    const char* tAt = mCursor;
    bool tNegative = false;
    if (tAt < mEnd && (*tAt == '-' || *tAt == '+'))
    {
        tNegative = (*tAt == '-');
        ++tAt;
    }
    if (tAt >= mEnd || !isDigit(*tAt))
    {
        return false;
    }
    int tValue = 0;
    while (tAt < mEnd && isDigit(*tAt))
    {
        tValue = tValue * 10 + (*tAt - '0');
        ++tAt;
    }
    // "12.5" gibi ondalik bir alan tamsayi olarak okunmasin
    if (tAt < mEnd && !isSpace(*tAt))
    {
        return false;
    }
    mCursor = tAt;
    pOut = tNegative ? -tValue : tValue;
    return true;
}

bool AptLine::readDouble(double& pOut)
{
    skipSpaces();
    const char* tAt = mCursor;
    bool tNegative = false;
    if (tAt < mEnd && (*tAt == '-' || *tAt == '+'))
    {
        tNegative = (*tAt == '-');
        ++tAt;
    }
    double tValue = 0.0;
    bool tHasDigit = false;
    while (tAt < mEnd && isDigit(*tAt))
    {
        tValue = tValue * 10.0 + (*tAt - '0');
        ++tAt;
        tHasDigit = true;
    }
    if (tAt < mEnd && *tAt == '.')
    {
        ++tAt;
        double tScale = 0.1;
        while (tAt < mEnd && isDigit(*tAt))
        {
            tValue += (*tAt - '0') * tScale;
            tScale *= 0.1;
            ++tAt;
            tHasDigit = true;
        }
    }
    if (!tHasDigit || (tAt < mEnd && !isSpace(*tAt)))
    {
        return false;
    }
    mCursor = tAt;
    pOut = tNegative ? -tValue : tValue;
    return true;
}

bool AptLine::token(std::string_view& pOut)
{
    skipSpaces();
    const char* tStart = mCursor;
    while (mCursor < mEnd && !isSpace(*mCursor))
    {
        ++mCursor;
    }
    if (mCursor == tStart)
    {
        return false;
    }
    pOut = std::string_view(tStart, static_cast<size_t>(mCursor - tStart));
    return true;
}

bool AptLine::token(std::string& pOut)
{
    std::string_view tToken;
    if (!token(tToken))
    {
        return false;
    }
    pOut.assign(tToken);
    return true;
}

std::string AptLine::rest()
{
    skipSpaces();
    const char* tStop = mEnd;
    while (tStop > mCursor && isSpace(tStop[-1]))
    {
        --tStop;
    }
    std::string tResult(mCursor, tStop);
    mCursor = mEnd;
    return tResult;
}

bool AptLine::atEnd()
{
    skipSpaces();
    return mCursor >= mEnd;
}

int aptRowCode(std::string_view pLine)
{
    AptLine tLine(pLine);
    int tCode = -1;
    return tLine.readInt(tCode) ? tCode : -1;
}
