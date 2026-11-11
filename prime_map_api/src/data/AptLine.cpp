// AptLine.cpp
#include "data/AptLine.h"

namespace
{

bool IsSpace(char pChar)
{
    return pChar == ' ' || pChar == '\t' || pChar == '\r' || pChar == '\n';
}

bool IsDigit(char pChar)
{
    return pChar >= '0' && pChar <= '9';
}

}  // namespace

AptLine::AptLine(std::string_view pText)
    : mCursor(pText.data())
    , mEnd(pText.data() + pText.size())
{
}

void AptLine::SkipSpaces()
{
    while (mCursor < mEnd && IsSpace(*mCursor))
    {
        ++mCursor;
    }
}

bool AptLine::Int(int& pOut)
{
    SkipSpaces();
    const char* tAt = mCursor;
    bool tNegative = false;
    if (tAt < mEnd && (*tAt == '-' || *tAt == '+'))
    {
        tNegative = (*tAt == '-');
        ++tAt;
    }
    if (tAt >= mEnd || !IsDigit(*tAt))
    {
        return false;
    }
    int tValue = 0;
    while (tAt < mEnd && IsDigit(*tAt))
    {
        tValue = tValue * 10 + (*tAt - '0');
        ++tAt;
    }
    // "12.5" gibi ondalik bir alan tamsayi olarak okunmasin
    if (tAt < mEnd && !IsSpace(*tAt))
    {
        return false;
    }
    mCursor = tAt;
    pOut = tNegative ? -tValue : tValue;
    return true;
}

bool AptLine::Double(double& pOut)
{
    SkipSpaces();
    const char* tAt = mCursor;
    bool tNegative = false;
    if (tAt < mEnd && (*tAt == '-' || *tAt == '+'))
    {
        tNegative = (*tAt == '-');
        ++tAt;
    }
    double tValue = 0.0;
    bool tHasDigit = false;
    while (tAt < mEnd && IsDigit(*tAt))
    {
        tValue = tValue * 10.0 + (*tAt - '0');
        ++tAt;
        tHasDigit = true;
    }
    if (tAt < mEnd && *tAt == '.')
    {
        ++tAt;
        double tScale = 0.1;
        while (tAt < mEnd && IsDigit(*tAt))
        {
            tValue += (*tAt - '0') * tScale;
            tScale *= 0.1;
            ++tAt;
            tHasDigit = true;
        }
    }
    if (!tHasDigit || (tAt < mEnd && !IsSpace(*tAt)))
    {
        return false;
    }
    mCursor = tAt;
    pOut = tNegative ? -tValue : tValue;
    return true;
}

bool AptLine::Token(std::string_view& pOut)
{
    SkipSpaces();
    const char* tStart = mCursor;
    while (mCursor < mEnd && !IsSpace(*mCursor))
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

bool AptLine::Token(std::string& pOut)
{
    std::string_view tToken;
    if (!Token(tToken))
    {
        return false;
    }
    pOut.assign(tToken);
    return true;
}

std::string AptLine::Rest()
{
    SkipSpaces();
    const char* tStop = mEnd;
    while (tStop > mCursor && IsSpace(tStop[-1]))
    {
        --tStop;
    }
    std::string tResult(mCursor, tStop);
    mCursor = mEnd;
    return tResult;
}

bool AptLine::AtEnd()
{
    SkipSpaces();
    return mCursor >= mEnd;
}

int AptRowCode(std::string_view pLine)
{
    AptLine tLine(pLine);
    int tCode = -1;
    return tLine.Int(tCode) ? tCode : -1;
}
