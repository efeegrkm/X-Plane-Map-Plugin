// Gruplu ve uc durumlu katman paneli.
#ifndef PRIME_MAP_SRC_UI_LAYERPANEL_H
#define PRIME_MAP_SRC_UI_LAYERPANEL_H

#include <set>
#include <string>
#include <vector>

#include "ScreenMath.h"

class LayerManager;

namespace Gfx
{
class Canvas;
}

class LayerPanel
{
public:
    struct Hit
    {
        bool mInside = false;
        std::vector<int> mToggle;
        bool mGroupOn = false;
        bool mIsGroupBox = false;
        int mSubLayerOwner = -1;
        int mSubLayer = -1;
        std::string mExpandKey;
        bool mResetDefaults = false;
    };

    bool isOpen() const;
    void setOpen(bool pOpen);
    void toggle();
    void toggleExpanded(const std::string& pKey);

    void layout(const RectI& pAnchor, int pMinBottom, const LayerManager& pLayers);

    void draw(Gfx::Canvas& pCanvas, int pMouseX, int pMouseY, const LayerManager& pLayers) const;

    Hit hitTest(int pX, int pY, const LayerManager& pLayers) const;

    bool scroll(int pX, int pY, int pClicks);

private:
    enum class RowKind
    {
        Group,
        Layer,
        SubLayer,
    };

    struct Row
    {
        RowKind mKind = RowKind::Layer;
        std::string mGroup;
        std::vector<int> mLayers;
        int mSubLayer = -1;
        int mIndent = 0;
    };

    RectI resetButtonRect() const;
    RectI rowRect(int pIndex) const;
    static RectI boxRect(const RectI& pRow, int pIndent);
    static int visibleCount(const LayerManager& pLayers, const std::vector<int>& pIndices);
    static std::string expandKey(const Row& pRow, const LayerManager& pLayers);
    static int rowState(const Row& pRow, const LayerManager& pLayers);
    void addLayerRows(int pLayer, int pIndent, const LayerManager& pLayers);

    bool mOpen = false;
    RectI mRect;
    std::vector<Row> mRows;
    std::set<std::string> mExpanded;
    int mFirstRow = 0;
    int mShownRows = 0;
};
#endif
