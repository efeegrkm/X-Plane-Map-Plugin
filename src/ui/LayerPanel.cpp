// LayerPanel.cpp
#include "ui/LayerPanel.h"

#include <algorithm>
#include <array>

#include "LayerManager.h"
#include "gfx/Canvas.h"
#include "ui/UiDraw.h"

namespace
{

const int kPadding = 8;       // panel kenari ile icerik arasi
const int kTitleHeight = 20;  // "LAYERS" basligi
const int kRowHeight = 22;    // her satir
const int kBoxSize = 14;      // onay kutusu
const int kArrowWidth = 14;   // grup satirindaki ok
const int kIndent = 18;       // alt katmanlarin iceri kaymasi
const int kMinWidth = 170;

struct LayerGroup
{
    const char* mName;
    std::array<const char*, 6> mLayerIds;  // katman kimlikleri (panel sirasi), nullptr ile biter
};

// Panel gruplari. Cizim sirasi Plugin.cpp'deki CreateLayers'tir; bu tablo
// sadece paneldeki gorunumu belirler.
const auto kGroups = std::to_array<LayerGroup>({
    {"Map", {"terrain", "grid", nullptr, nullptr, nullptr, nullptr}},
    {"Airports", {"airports", "runways", "airport_surfaces", "airport_objects", "taxi_routes", nullptr}},
    {"Navaids", {"vor", "ndb", "ils", "fix", nullptr, nullptr}},
    {"Airways", {"airways_low", "airways_high", nullptr, nullptr, nullptr, nullptr}},
    {"Flight", {"aircraft", "fms", "track", nullptr, nullptr, nullptr}},
});

int FindLayer(const LayerManager& pLayers, const char* pId)
{
    for (int tIndex = 0; tIndex < pLayers.Count(); ++tIndex)
    {
        if (pLayers.At(tIndex)->Id() == pId)
        {
            return tIndex;
        }
    }
    return -1;
}

// Kucuk ucgen ok: saga (grup kapali) veya asagi (grup acik)
void DrawArrow(Gfx::Canvas& pCanvas, const RectI& pRect, bool pExpanded, const Rgba& pColor)
{
    const float tX = static_cast<float>(pRect.mLeft + pRect.mRight) * 0.5f;
    const float tY = static_cast<float>(pRect.mTop + pRect.mBottom) * 0.5f;
    Gfx::Mesh tMesh;
    if (pExpanded)
    {
        tMesh.AddTriangle({tX - 4.0f, tY + 2.5f}, {tX + 4.0f, tY + 2.5f}, {tX, tY - 3.0f}, pColor);
    }
    else
    {
        tMesh.AddTriangle({tX - 2.5f, tY + 4.0f}, {tX - 2.5f, tY - 4.0f}, {tX + 3.0f, tY}, pColor);
    }
    pCanvas.Draw(tMesh);
}

}  // namespace

bool LayerPanel::IsOpen() const
{
    return mOpen;
}

void LayerPanel::SetOpen(bool pOpen)
{
    mOpen = pOpen;
}

void LayerPanel::Toggle()
{
    mOpen = !mOpen;
}

void LayerPanel::ToggleGroup(const std::string& pGroup)
{
    if (!mExpanded.erase(pGroup))
    {
        mExpanded.insert(pGroup);
    }
}

int LayerPanel::VisibleCount(const LayerManager& pLayers, const std::vector<int>& pIndices)
{
    return static_cast<int>(std::count_if(pIndices.begin(), pIndices.end(), [&](int pIndex)
                                          {
                                              return pLayers.At(pIndex)->IsVisible();
                                          }));
}

void LayerPanel::Layout(const RectI& pAnchor, const LayerManager& pLayers)
{
    // Satirlar: gruplar (acik olanin alt katmanlari altinda), sonra grupsuzlar
    mRows.clear();
    std::vector<bool> tGrouped(static_cast<size_t>(pLayers.Count()), false);
    for (const LayerGroup& tGroup : kGroups)
    {
        Row tGroupRow;
        tGroupRow.mIsGroup = true;
        tGroupRow.mGroup = tGroup.mName;
        for (const char* tId : tGroup.mLayerIds)
        {
            if (!tId)
            {
                break;
            }
            const int tIndex = FindLayer(pLayers, tId);
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
        if (mExpanded.count(tGroupRow.mGroup) > 0)
        {
            for (int tIndex : tGroupRow.mLayers)
            {
                mRows.push_back({false, "", {tIndex}, 1});
            }
        }
    }
    for (int tIndex = 0; tIndex < pLayers.Count(); ++tIndex)
    {
        if (!tGrouped[tIndex])
        {
            mRows.push_back({false, "", {tIndex}, 0});
        }
    }

    // Genislik: en uzun satir
    int tWidth = kMinWidth;
    for (const Row& tRow : mRows)
    {
        const std::string& tName = tRow.mIsGroup ? tRow.mGroup : pLayers.At(tRow.mLayers[0])->Name();
        const int tRowWidth = kPadding + tRow.mIndent * kIndent + kBoxSize + 6 + kArrowWidth + 4 +
                              Ui::TextWidth(tName) + kPadding;
        tWidth = std::max(tWidth, tRowWidth);
    }
    mRect.mLeft = pAnchor.mLeft;
    mRect.mRight = pAnchor.mLeft + tWidth;
    mRect.mTop = pAnchor.mBottom - 4;
    mRect.mBottom = mRect.mTop - (kPadding + kTitleHeight + static_cast<int>(mRows.size()) * kRowHeight + kPadding);
}

RectI LayerPanel::RowRect(int pIndex) const
{
    RectI tRow;
    tRow.mLeft = mRect.mLeft + kPadding;
    tRow.mRight = mRect.mRight - kPadding;
    tRow.mTop = mRect.mTop - kPadding - kTitleHeight - pIndex * kRowHeight;
    tRow.mBottom = tRow.mTop - kRowHeight;
    return tRow;
}

RectI LayerPanel::BoxRect(const RectI& pRow, int pIndent)
{
    RectI tBox;
    tBox.mLeft = pRow.mLeft + pIndent * kIndent;
    tBox.mRight = tBox.mLeft + kBoxSize;
    tBox.mTop = (pRow.mTop + pRow.mBottom) / 2 + kBoxSize / 2;
    tBox.mBottom = tBox.mTop - kBoxSize;
    return tBox;
}

void LayerPanel::Draw(Gfx::Canvas& pCanvas, int pMouseX, int pMouseY, const LayerManager& pLayers) const
{
    if (!mOpen)
    {
        return;
    }
    Ui::FillRect(pCanvas, mRect, Ui::kPanelBg);
    Ui::FrameRect(pCanvas, mRect, Ui::kBorder);
    Ui::Text(mRect.mLeft + kPadding, mRect.mTop - kPadding - 12, "LAYERS", Ui::kTextGrey);

    for (int tIndex = 0; tIndex < static_cast<int>(mRows.size()); ++tIndex)
    {
        const Row& tRow = mRows[tIndex];
        const RectI tRect = RowRect(tIndex);
        const bool tHover = tRect.Contains(pMouseX, pMouseY);
        const RectI tBox = BoxRect(tRect, tRow.mIndent);
        const int tOn = VisibleCount(pLayers, tRow.mLayers);
        const int tAll = static_cast<int>(tRow.mLayers.size());
        const int tTextY = (tRect.mTop + tRect.mBottom) / 2 - Ui::FontHeight() / 2 + 1;
        int tTextX = tBox.mRight + 6;

        if (tRow.mIsGroup)
        {
            // Uc durumlu kutu: hepsi acik / karisik / hepsi kapali
            if (tOn > 0 && tOn < tAll)
            {
                Ui::CheckBox(pCanvas, tBox, false, tHover);
                RectI tBar = tBox;
                tBar.mLeft += 3;
                tBar.mRight -= 3;
                tBar.mBottom = (tBox.mTop + tBox.mBottom) / 2 - 1;
                tBar.mTop = tBar.mBottom + 3;
                Ui::FillRect(pCanvas, tBar, Ui::kButtonActive);
            }
            else
            {
                Ui::CheckBox(pCanvas, tBox, tOn == tAll, tHover);
            }
            RectI tArrow = tRect;
            tArrow.mLeft = tTextX;
            tArrow.mRight = tTextX + kArrowWidth;
            DrawArrow(pCanvas, tArrow, mExpanded.count(tRow.mGroup) > 0, Ui::kTextGrey);
            tTextX += kArrowWidth + 4;
            Ui::Text(tTextX, tTextY, tRow.mGroup, tOn > 0 ? Ui::kTextWhite : Ui::kTextGrey);
            continue;
        }
        const Layer* tLayer = pLayers.At(tRow.mLayers[0]);
        Ui::CheckBox(pCanvas, tBox, tLayer->IsVisible(), tHover);
        Ui::Text(tTextX, tTextY, tLayer->Name(), tLayer->IsVisible() ? Ui::kTextWhite : Ui::kTextGrey);
    }
}

LayerPanel::Hit LayerPanel::HitTest(int pX, int pY, const LayerManager& pLayers) const
{
    Hit tHit;
    if (!mOpen || !mRect.Contains(pX, pY))
    {
        return tHit;
    }
    tHit.mInside = true;
    for (int tIndex = 0; tIndex < static_cast<int>(mRows.size()); ++tIndex)
    {
        const Row& tRow = mRows[tIndex];
        const RectI tRect = RowRect(tIndex);
        if (!tRect.Contains(pX, pY))
        {
            continue;
        }
        if (!tRow.mIsGroup)
        {
            tHit.mToggle = tRow.mLayers;
            return tHit;
        }
        // Grup: kutuya tiklandiysa hepsini ac/kapat, ok veya ada tiklandiysa grubu ac/kapa
        const RectI tBox = BoxRect(tRect, tRow.mIndent);
        if (pX <= tBox.mRight + 2)
        {
            tHit.mIsGroupBox = true;
            tHit.mToggle = tRow.mLayers;
            tHit.mGroupOn = VisibleCount(pLayers, tRow.mLayers) < static_cast<int>(tRow.mLayers.size());
        }
        else
        {
            tHit.mExpandGroup = tRow.mGroup;
        }
        return tHit;
    }
    return tHit;
}
