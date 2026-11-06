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
const int kTitleHeight = 24;  // "LAYERS" basligi + ayirici cizgi
const int kRowHeight = 22;    // her satir
const int kBoxSize = 14;      // onay kutusu
const int kArrowWidth = 14;   // acilir satirdaki ok
const int kIndent = 18;       // her seviyenin iceri kaymasi
const int kMinWidth = 190;
const int kMinRows = 4;       // pencere cok kucukse bile en az bu kadar satir

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

// Kucuk ucgen ok: saga (kapali) veya asagi (acik)
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

// Uc durumlu kutu: 0 kapali, 1 acik, 2 karisik (ortada yatay cubuk)
void DrawStateBox(Gfx::Canvas& pCanvas, const RectI& pBox, int pState, bool pHover)
{
    Ui::CheckBox(pCanvas, pBox, pState == 1, pHover);
    if (pState == 2)
    {
        RectI tBar = pBox;
        tBar.mLeft += 3;
        tBar.mRight -= 3;
        tBar.mBottom = (pBox.mTop + pBox.mBottom) / 2 - 1;
        tBar.mTop = tBar.mBottom + 3;
        Ui::FillRect(pCanvas, tBar, Ui::kButtonActive);
    }
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

void LayerPanel::ToggleExpanded(const std::string& pKey)
{
    if (!mExpanded.erase(pKey))
    {
        mExpanded.insert(pKey);
    }
}

int LayerPanel::VisibleCount(const LayerManager& pLayers, const std::vector<int>& pIndices)
{
    return static_cast<int>(std::count_if(pIndices.begin(), pIndices.end(), [&](int pIndex)
                                          {
                                              return pLayers.At(pIndex)->IsVisible();
                                          }));
}

std::string LayerPanel::ExpandKey(const Row& pRow, const LayerManager& pLayers)
{
    if (pRow.mKind == RowKind::Group)
    {
        return "group:" + pRow.mGroup;
    }
    return "layer:" + pLayers.At(pRow.mLayers[0])->Id();
}

int LayerPanel::RowState(const Row& pRow, const LayerManager& pLayers)
{
    if (pRow.mKind == RowKind::Group)
    {
        const int tOn = VisibleCount(pLayers, pRow.mLayers);
        const int tAll = static_cast<int>(pRow.mLayers.size());
        return tOn == 0 ? 0 : (tOn == tAll ? 1 : 2);
    }
    const Layer* tLayer = pLayers.At(pRow.mLayers[0]);
    if (pRow.mKind == RowKind::SubLayer)
    {
        return tLayer->IsVisible() && tLayer->IsSubLayerVisible(pRow.mSubLayer) ? 1 : 0;
    }
    if (!tLayer->IsVisible())
    {
        return 0;
    }
    // Acik katmanin alt katmanlarindan bazilari kapaliysa karisik
    return tLayer->VisibleSubLayerCount() < tLayer->SubLayerCount() ? 2 : 1;
}

void LayerPanel::AddLayerRows(int pLayer, int pIndent, const LayerManager& pLayers)
{
    Row tRow;
    tRow.mKind = RowKind::Layer;
    tRow.mLayers = {pLayer};
    tRow.mIndent = pIndent;
    mRows.push_back(tRow);
    const Layer* tLayer = pLayers.At(pLayer);
    if (tLayer->SubLayerCount() == 0 || mExpanded.count(ExpandKey(tRow, pLayers)) == 0)
    {
        return;
    }
    for (int tSub = 0; tSub < tLayer->SubLayerCount(); ++tSub)
    {
        Row tSubRow;
        tSubRow.mKind = RowKind::SubLayer;
        tSubRow.mLayers = {pLayer};
        tSubRow.mSubLayer = tSub;
        tSubRow.mIndent = pIndent + 1;
        mRows.push_back(tSubRow);
    }
}

void LayerPanel::Layout(const RectI& pAnchor, int pMinBottom, const LayerManager& pLayers)
{
    // Satirlar: gruplar (acik olanin katmanlari, acik katmanin alt katmanlari altinda), sonra grupsuzlar
    mRows.clear();
    std::vector<bool> tGrouped(static_cast<size_t>(pLayers.Count()), false);
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
        if (mExpanded.count(ExpandKey(tGroupRow, pLayers)) > 0)
        {
            for (int tIndex : tGroupRow.mLayers)
            {
                AddLayerRows(tIndex, 1, pLayers);
            }
        }
    }
    for (int tIndex = 0; tIndex < pLayers.Count(); ++tIndex)
    {
        if (!tGrouped[tIndex])
        {
            AddLayerRows(tIndex, 0, pLayers);
        }
    }

    // Genislik: en uzun satir (kaydirma degisince panel ziplamasin diye tum satirlar)
    int tWidth = kMinWidth;
    for (const Row& tRow : mRows)
    {
        const Layer* tLayer = tRow.mKind == RowKind::Group ? nullptr : pLayers.At(tRow.mLayers[0]);
        const std::string& tName = !tLayer ? tRow.mGroup
                                   : (tRow.mKind == RowKind::SubLayer ? tLayer->SubLayerAt(tRow.mSubLayer).mName
                                                                       : tLayer->Name());
        const int tRowWidth = kPadding + tRow.mIndent * kIndent + kBoxSize + 6 + kArrowWidth + 4 +
                              Ui::TextWidth(tName) + kPadding;
        tWidth = std::max(tWidth, tRowWidth);
    }

    // Yukseklik: pencereye sigan kadar satir; fazlasi kaydirilir
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

RectI LayerPanel::RowRect(int pIndex) const
{
    RectI tRow;
    tRow.mLeft = mRect.mLeft + kPadding;
    tRow.mRight = mRect.mRight - kPadding;
    tRow.mTop = mRect.mTop - kPadding - kTitleHeight - (pIndex - mFirstRow) * kRowHeight;
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

bool LayerPanel::Scroll(int pX, int pY, int pClicks)
{
    if (!mOpen || !mRect.Contains(pX, pY))
    {
        return false;
    }
    const int tMaxFirst = std::max(static_cast<int>(mRows.size()) - mShownRows, 0);
    mFirstRow = std::clamp(mFirstRow - pClicks, 0, tMaxFirst);  // tekerlek yukari: listenin basina
    return true;
}

void LayerPanel::Draw(Gfx::Canvas& pCanvas, int pMouseX, int pMouseY, const LayerManager& pLayers) const
{
    if (!mOpen)
    {
        return;
    }
    Ui::FillRect(pCanvas, mRect, Ui::kPanelBg);
    Ui::FrameRect(pCanvas, mRect, Ui::kBorder);
    const int tTitleBase = mRect.mTop - kPadding - 12;
    Ui::Text(mRect.mLeft + kPadding, tTitleBase, "LAYERS", Ui::kTextGrey);
    const int tRowCount = static_cast<int>(mRows.size());
    if (mShownRows < tRowCount)
    {
        // Kaydirma durumu: "5-18 / 31" sag ustte
        const std::string tRange = std::to_string(mFirstRow + 1) + "-" + std::to_string(mFirstRow + mShownRows) +
                                   " / " + std::to_string(tRowCount);
        Ui::Text(mRect.mRight - kPadding - Ui::TextWidth(tRange), tTitleBase, tRange, Ui::kTextDim);
    }
    const float tSeparatorY = static_cast<float>(mRect.mTop - kPadding - kTitleHeight) + 3.5f;
    Ui::Line(pCanvas, {static_cast<float>(mRect.mLeft + kPadding), tSeparatorY},
             {static_cast<float>(mRect.mRight - kPadding), tSeparatorY}, 1.0f, Ui::kBorder);

    for (int tIndex = mFirstRow; tIndex < mFirstRow + mShownRows; ++tIndex)
    {
        const Row& tRow = mRows[tIndex];
        const RectI tRect = RowRect(tIndex);
        const bool tHover = tRect.Contains(pMouseX, pMouseY);
        if (tHover)
        {
            RectI tHighlight = tRect;
            tHighlight.mLeft = tRect.mLeft + tRow.mIndent * kIndent - 3;
            tHighlight.mRight += 3;
            Ui::FillRect(pCanvas, tHighlight, {1.0f, 1.0f, 1.0f, 0.05f});
        }
        const RectI tBox = BoxRect(tRect, tRow.mIndent);
        const int tState = RowState(tRow, pLayers);
        const int tTextY = (tRect.mTop + tRect.mBottom) / 2 - Ui::FontHeight() / 2 + 1;
        int tTextX = tBox.mRight + 6;
        DrawStateBox(pCanvas, tBox, tState, tHover);

        // Agac cizgisi: alt katmanlar ust satirin okunun hizasinda ince bir dikey cizgiye baglanir
        if (tRow.mKind == RowKind::SubLayer)
        {
            const float tGuideX = static_cast<float>(tBox.mLeft - kIndent / 2) + 0.5f;
            Ui::Line(pCanvas, {tGuideX, static_cast<float>(tRect.mTop)}, {tGuideX, static_cast<float>(tRect.mBottom)},
                     1.0f, Ui::kBorder);
        }

        const Layer* tLayer = tRow.mKind == RowKind::Group ? nullptr : pLayers.At(tRow.mLayers[0]);
        const bool tExpandable = tRow.mKind == RowKind::Group ||
                                 (tRow.mKind == RowKind::Layer && tLayer->SubLayerCount() > 0);
        if (tExpandable)
        {
            RectI tArrow = tRect;
            tArrow.mLeft = tTextX;
            tArrow.mRight = tTextX + kArrowWidth;
            DrawArrow(pCanvas, tArrow, mExpanded.count(ExpandKey(tRow, pLayers)) > 0, Ui::kTextGrey);
        }
        if (tRow.mKind != RowKind::SubLayer)
        {
            tTextX += kArrowWidth + 4;  // oksuz katmanlar da ayni hizada baslasin
        }
        const std::string& tName = !tLayer ? tRow.mGroup
                                   : (tRow.mKind == RowKind::SubLayer ? tLayer->SubLayerAt(tRow.mSubLayer).mName
                                                                       : tLayer->Name());
        Ui::Text(tTextX, tTextY, tName, tState != 0 ? Ui::kTextWhite : Ui::kTextGrey);
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
    for (int tIndex = mFirstRow; tIndex < mFirstRow + mShownRows; ++tIndex)
    {
        const Row& tRow = mRows[tIndex];
        const RectI tRect = RowRect(tIndex);
        if (!tRect.Contains(pX, pY))
        {
            continue;
        }
        const RectI tBox = BoxRect(tRect, tRow.mIndent);
        const bool tOnBox = pX <= tBox.mRight + 2;
        switch (tRow.mKind)
        {
            case RowKind::SubLayer:
                tHit.mSubLayerOwner = tRow.mLayers[0];
                tHit.mSubLayer = tRow.mSubLayer;
                break;
            case RowKind::Layer:
                if (!tOnBox && pLayers.At(tRow.mLayers[0])->SubLayerCount() > 0)
                {
                    tHit.mExpandKey = ExpandKey(tRow, pLayers);
                }
                else
                {
                    tHit.mToggle = tRow.mLayers;
                }
                break;
            case RowKind::Group:
                // Kutuya tiklandiysa hepsini ac/kapat, ok veya ada tiklandiysa grubu ac/kapa
                if (tOnBox)
                {
                    tHit.mIsGroupBox = true;
                    tHit.mToggle = tRow.mLayers;
                    tHit.mGroupOn = VisibleCount(pLayers, tRow.mLayers) < static_cast<int>(tRow.mLayers.size());
                }
                else
                {
                    tHit.mExpandKey = ExpandKey(tRow, pLayers);
                }
                break;
        }
        return tHit;
    }
    return tHit;
}
