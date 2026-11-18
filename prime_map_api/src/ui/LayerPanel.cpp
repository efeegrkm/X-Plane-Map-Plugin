#include "ui/LayerPanel.h"

#include <algorithm>
#include <array>

#include "LayerManager.h"
#include "gfx/Canvas.h"
#include "ui/UiDraw.h"

namespace
{
const int kPadding = 8;
const int kTitleHeight = 24;
const int kRowHeight = 22;
const int kBoxSize = 14;
const int kArrowWidth = 14;
const int kIndent = 18;
const int kMinWidth = 190;
const int kMinRows = 4;
const char* kResetLabel = "Defaults";
const int kResetHeight = 18;

struct LayerGroup
{
    const char* mName;
    std::array<const char*, 6> mLayerIds;
};

const auto kGroups = std::to_array<LayerGroup>({
    {"XMap", {"terrain", "roads", "grid", nullptr, nullptr, nullptr}},
    {"Airports", {"airports", "runways", "airport_surfaces", "airport_objects", "taxi_routes", nullptr}},
    {"Navaids", {"vor", "ndb", "ils", "fix", nullptr, nullptr}},
    {"Airways", {"airways_low", "airways_high", nullptr, nullptr, nullptr, nullptr}},
    {"Flight", {"aircraft", "fms", "track", nullptr, nullptr, nullptr}},
});

int findLayer(const LayerManager& pLayers, const char* pId)
{
    for (int tIndex = 0; tIndex < pLayers.count(); ++tIndex)
    {
        if (pLayers.at(tIndex)->id() == pId)
        {
            return tIndex;
        }
    }
    return -1;
}

void drawArrow(Gfx::Canvas& pCanvas, const RectI& pRect, bool pExpanded, const Rgba& pColor)
{
    const float tX = static_cast<float>(pRect.mLeft + pRect.mRight) * 0.5f;
    const float tY = static_cast<float>(pRect.mTop + pRect.mBottom) * 0.5f;
    Gfx::Mesh tMesh;
    if (pExpanded)
    {
        tMesh.addTriangle({tX - 4.0f, tY + 2.5f}, {tX + 4.0f, tY + 2.5f}, {tX, tY - 3.0f}, pColor);
    }
    else
    {
        tMesh.addTriangle({tX - 2.5f, tY + 4.0f}, {tX - 2.5f, tY - 4.0f}, {tX + 3.0f, tY}, pColor);
    }
    pCanvas.draw(tMesh);
}

void drawStateBox(Gfx::Canvas& pCanvas, const RectI& pBox, int pState, bool pHover)
{
    Ui::checkBox(pCanvas, pBox, pState == 1, pHover);
    if (pState == 2)
    {
        RectI tBar = pBox;
        tBar.mLeft += 3;
        tBar.mRight -= 3;
        tBar.mBottom = (pBox.mTop + pBox.mBottom) / 2 - 1;
        tBar.mTop = tBar.mBottom + 3;
        Ui::fillRect(pCanvas, tBar, Ui::kButtonActive);
    }
}
}

bool LayerPanel::isOpen() const
{
    return mOpen;
}

void LayerPanel::setOpen(bool pOpen)
{
    mOpen = pOpen;
}

void LayerPanel::toggle()
{
    mOpen = !mOpen;
}

void LayerPanel::toggleExpanded(const std::string& pKey)
{
    if (!mExpanded.erase(pKey))
    {
        mExpanded.insert(pKey);
    }
}

int LayerPanel::visibleCount(const LayerManager& pLayers, const std::vector<int>& pIndices)
{
    return static_cast<int>(std::count_if(pIndices.begin(), pIndices.end(), [&](int pIndex)
                                          {
                                              return pLayers.at(pIndex)->isVisible();
                                          }));
}

std::string LayerPanel::expandKey(const Row& pRow, const LayerManager& pLayers)
{
    if (pRow.mKind == RowKind::Group)
    {
        return "group:" + pRow.mGroup;
    }
    return "layer:" + pLayers.at(pRow.mLayers[0])->id();
}

int LayerPanel::rowState(const Row& pRow, const LayerManager& pLayers)
{
    if (pRow.mKind == RowKind::Group)
    {
        const int tOn = visibleCount(pLayers, pRow.mLayers);
        const int tAll = static_cast<int>(pRow.mLayers.size());
        return tOn == 0 ? 0 : (tOn == tAll ? 1 : 2);
    }
    const Layer* tLayer = pLayers.at(pRow.mLayers[0]);
    if (pRow.mKind == RowKind::SubLayer)
    {
        return tLayer->isVisible() && tLayer->isSubLayerVisible(pRow.mSubLayer) ? 1 : 0;
    }
    if (!tLayer->isVisible())
    {
        return 0;
    }
    return tLayer->visibleSubLayerCount() < tLayer->subLayerCount() ? 2 : 1;
}

void LayerPanel::addLayerRows(int pLayer, int pIndent, const LayerManager& pLayers)
{
    Row tRow;
    tRow.mKind = RowKind::Layer;
    tRow.mLayers = {pLayer};
    tRow.mIndent = pIndent;
    mRows.push_back(tRow);
    const Layer* tLayer = pLayers.at(pLayer);
    if (tLayer->subLayerCount() == 0 || mExpanded.count(expandKey(tRow, pLayers)) == 0)
    {
        return;
    }
    for (int tSub = 0; tSub < tLayer->subLayerCount(); ++tSub)
    {
        Row tSubRow;
        tSubRow.mKind = RowKind::SubLayer;
        tSubRow.mLayers = {pLayer};
        tSubRow.mSubLayer = tSub;
        tSubRow.mIndent = pIndent + 1;
        mRows.push_back(tSubRow);
    }
}

void LayerPanel::layout(const RectI& pAnchor, int pMinBottom, const LayerManager& pLayers)
{
    mRows.clear();
    std::vector<bool> tGrouped(static_cast<size_t>(pLayers.count()), false);
    for (const LayerGroup& tGroup : kGroups)
    {
        Row tGroupRow;
        tGroupRow.mKind = RowKind::Group;
        tGroupRow.mGroup = tGroup.mName;
        for (const char* tId : tGroup.mLayerIds)
        {
            if (!tId)
            {
                break;
            }
            const int tIndex = findLayer(pLayers, tId);
            if (tIndex >= 0)
            {
                tGroupRow.mLayers.push_back(tIndex);
                tGrouped[tIndex] = true;
            }
        }
        if (tGroupRow.mLayers.empty())
        {
            continue;
        }
        mRows.push_back(tGroupRow);
        if (mExpanded.count(expandKey(tGroupRow, pLayers)) > 0)
        {
            for (int tIndex : tGroupRow.mLayers)
            {
                addLayerRows(tIndex, 1, pLayers);
            }
        }
    }
    for (int tIndex = 0; tIndex < pLayers.count(); ++tIndex)
    {
        if (!tGrouped[tIndex])
        {
            addLayerRows(tIndex, 0, pLayers);
        }
    }

    int tWidth = std::max(kMinWidth, kPadding + Ui::textWidth("LAYERS") + Ui::textWidth("00-00 / 00") + 24 +
                                         Ui::buttonWidth(kResetLabel) + kPadding);
    for (const Row& tRow : mRows)
    {
        const Layer* tLayer = tRow.mKind == RowKind::Group ? nullptr : pLayers.at(tRow.mLayers[0]);
        const std::string& tName = !tLayer ? tRow.mGroup
                                   : (tRow.mKind == RowKind::SubLayer ? tLayer->subLayerAt(tRow.mSubLayer).mName
                                                                       : tLayer->name());
        const int tRowWidth = kPadding + tRow.mIndent * kIndent + kBoxSize + 6 + kArrowWidth + 4 +
                              Ui::textWidth(tName) + kPadding;
        tWidth = std::max(tWidth, tRowWidth);
    }

    const int tTop = pAnchor.mBottom - 4;
    const int tFixed = kPadding + kTitleHeight + kPadding;
    const int tCapacity = std::max((tTop - pMinBottom - tFixed) / kRowHeight, kMinRows);
    const int tRowCount = static_cast<int>(mRows.size());
    mShownRows = std::min(tRowCount, tCapacity);
    mFirstRow = std::clamp(mFirstRow, 0, tRowCount - mShownRows);

    mRect.mLeft = pAnchor.mLeft;
    mRect.mRight = pAnchor.mLeft + tWidth;
    mRect.mTop = tTop;
    mRect.mBottom = mRect.mTop - (tFixed + mShownRows * kRowHeight);
}

RectI LayerPanel::resetButtonRect() const
{
    RectI tButton;
    tButton.mRight = mRect.mRight - kPadding;
    tButton.mLeft = tButton.mRight - Ui::buttonWidth(kResetLabel) + 8;
    tButton.mTop = mRect.mTop - kPadding + 2;
    tButton.mBottom = tButton.mTop - kResetHeight;
    return tButton;
}

RectI LayerPanel::rowRect(int pIndex) const
{
    RectI tRow;
    tRow.mLeft = mRect.mLeft + kPadding;
    tRow.mRight = mRect.mRight - kPadding;
    tRow.mTop = mRect.mTop - kPadding - kTitleHeight - (pIndex - mFirstRow) * kRowHeight;
    tRow.mBottom = tRow.mTop - kRowHeight;
    return tRow;
}

RectI LayerPanel::boxRect(const RectI& pRow, int pIndent)
{
    RectI tBox;
    tBox.mLeft = pRow.mLeft + pIndent * kIndent;
    tBox.mRight = tBox.mLeft + kBoxSize;
    tBox.mTop = (pRow.mTop + pRow.mBottom) / 2 + kBoxSize / 2;
    tBox.mBottom = tBox.mTop - kBoxSize;
    return tBox;
}

bool LayerPanel::scroll(int pX, int pY, int pClicks)
{
    if (!mOpen || !mRect.contains(pX, pY))
    {
        return false;
    }
    const int tMaxFirst = std::max(static_cast<int>(mRows.size()) - mShownRows, 0);
    mFirstRow = std::clamp(mFirstRow - pClicks, 0, tMaxFirst);
    return true;
}

void LayerPanel::draw(Gfx::Canvas& pCanvas, int pMouseX, int pMouseY, const LayerManager& pLayers) const
{
    if (!mOpen)
    {
        return;
    }
    Ui::fillRect(pCanvas, mRect, Ui::kPanelBg);
    Ui::frameRect(pCanvas, mRect, Ui::kBorder);
    const int tTitleBase = mRect.mTop - kPadding - 12;
    Ui::text(mRect.mLeft + kPadding, tTitleBase, "LAYERS", Ui::kTextGrey);
    const int tRowCount = static_cast<int>(mRows.size());
    if (mShownRows < tRowCount)
    {
        const std::string tRange = std::to_string(mFirstRow + 1) + "-" + std::to_string(mFirstRow + mShownRows) +
                                   " / " + std::to_string(tRowCount);
        Ui::text(resetButtonRect().mLeft - 8 - Ui::textWidth(tRange), tTitleBase, tRange, Ui::kTextDim);
    }
    const RectI tReset = resetButtonRect();
    Ui::Button(pCanvas, tReset, kResetLabel, false, true, tReset.contains(pMouseX, pMouseY));
    const float tSeparatorY = static_cast<float>(mRect.mTop - kPadding - kTitleHeight) + 3.5f;
    Ui::line(pCanvas, {static_cast<float>(mRect.mLeft + kPadding), tSeparatorY},
             {static_cast<float>(mRect.mRight - kPadding), tSeparatorY}, 1.0f, Ui::kBorder);

    for (int tIndex = mFirstRow; tIndex < mFirstRow + mShownRows; ++tIndex)
    {
        const Row& tRow = mRows[tIndex];
        const RectI tRect = rowRect(tIndex);
        const bool tHover = tRect.contains(pMouseX, pMouseY);
        if (tHover)
        {
            RectI tHighlight = tRect;
            tHighlight.mLeft = tRect.mLeft + tRow.mIndent * kIndent - 3;
            tHighlight.mRight += 3;
            Ui::fillRect(pCanvas, tHighlight, {1.0f, 1.0f, 1.0f, 0.05f});
        }
        const RectI tBox = boxRect(tRect, tRow.mIndent);
        const int tState = rowState(tRow, pLayers);
        const int tTextY = (tRect.mTop + tRect.mBottom) / 2 - Ui::fontHeight() / 2 + 1;
        int tTextX = tBox.mRight + 6;
        drawStateBox(pCanvas, tBox, tState, tHover);

        if (tRow.mKind == RowKind::SubLayer)
        {
            const float tGuideX = static_cast<float>(tBox.mLeft - kIndent / 2) + 0.5f;
            Ui::line(pCanvas, {tGuideX, static_cast<float>(tRect.mTop)}, {tGuideX, static_cast<float>(tRect.mBottom)},
                     1.0f, Ui::kBorder);
        }

        const Layer* tLayer = tRow.mKind == RowKind::Group ? nullptr : pLayers.at(tRow.mLayers[0]);
        const bool tExpandable = tRow.mKind == RowKind::Group ||
                                 (tRow.mKind == RowKind::Layer && tLayer->subLayerCount() > 0);
        if (tExpandable)
        {
            RectI tArrow = tRect;
            tArrow.mLeft = tTextX;
            tArrow.mRight = tTextX + kArrowWidth;
            drawArrow(pCanvas, tArrow, mExpanded.count(expandKey(tRow, pLayers)) > 0, Ui::kTextGrey);
        }
        if (tRow.mKind != RowKind::SubLayer)
        {
            tTextX += kArrowWidth + 4;
        }
        const std::string& tName = !tLayer ? tRow.mGroup
                                   : (tRow.mKind == RowKind::SubLayer ? tLayer->subLayerAt(tRow.mSubLayer).mName
                                                                       : tLayer->name());
        Ui::text(tTextX, tTextY, tName, tState != 0 ? Ui::kTextWhite : Ui::kTextGrey);
    }
}

LayerPanel::Hit LayerPanel::hitTest(int pX, int pY, const LayerManager& pLayers) const
{
    Hit tHit;
    if (!mOpen || !mRect.contains(pX, pY))
    {
        return tHit;
    }
    tHit.mInside = true;
    if (resetButtonRect().contains(pX, pY))
    {
        tHit.mResetDefaults = true;
        return tHit;
    }
    for (int tIndex = mFirstRow; tIndex < mFirstRow + mShownRows; ++tIndex)
    {
        const Row& tRow = mRows[tIndex];
        const RectI tRect = rowRect(tIndex);
        if (!tRect.contains(pX, pY))
        {
            continue;
        }
        const RectI tBox = boxRect(tRect, tRow.mIndent);
        const bool tOnBox = pX <= tBox.mRight + 2;
        switch (tRow.mKind)
        {
            case RowKind::SubLayer:
                tHit.mSubLayerOwner = tRow.mLayers[0];
                tHit.mSubLayer = tRow.mSubLayer;
                break;
            case RowKind::Layer:
                if (!tOnBox && pLayers.at(tRow.mLayers[0])->subLayerCount() > 0)
                {
                    tHit.mExpandKey = expandKey(tRow, pLayers);
                }
                else
                {
                    tHit.mToggle = tRow.mLayers;
                }
                break;
            case RowKind::Group:
                if (tOnBox)
                {
                    tHit.mIsGroupBox = true;
                    tHit.mToggle = tRow.mLayers;
                    tHit.mGroupOn = visibleCount(pLayers, tRow.mLayers) < static_cast<int>(tRow.mLayers.size());
                }
                else
                {
                    tHit.mExpandKey = expandKey(tRow, pLayers);
                }
                break;
        }
        return tHit;
    }
    return tHit;
}
