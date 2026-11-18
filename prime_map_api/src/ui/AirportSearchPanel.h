// Haritanin kendi arayuzundeki havalimani arama paneli.
#ifndef PRIME_MAP_SRC_UI_AIRPORTSEARCHPANEL_H
#define PRIME_MAP_SRC_UI_AIRPORTSEARCHPANEL_H

#include <string>
#include <vector>

#include "ScreenMath.h"

class AirportGeometryStore;

namespace Gfx
{
class Canvas;
}

class AirportSearchPanel
{
public:
    struct Hit
    {
        bool mInside = false;
        bool mInput = false;
        int mAirport = -1;
    };

    explicit AirportSearchPanel(const AirportGeometryStore& pAirports);

    bool isOpen() const;
    void setOpen(bool pOpen);
    void toggle();

    void layout(const RectI& pAnchor);
    void draw(Gfx::Canvas& pCanvas, int pMouseX, int pMouseY, bool pFocused, float pNow) const;
    Hit hitTest(int pX, int pY) const;

    void typeCharacter(char pChar);
    void backspace();
    void moveSelection(int pDelta);
    int selectedAirport() const;

    void refresh();

private:
    void search();
    RectI inputRect() const;
    RectI rowRect(int pIndex) const;

    const AirportGeometryStore& mAirports;
    bool mOpen = false;
    std::string mQuery;
    std::vector<int> mResults;
    int mSelected = 0;
    bool mSearchedWhenReady = false;
    RectI mRect;
};
#endif
