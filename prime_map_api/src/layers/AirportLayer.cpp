#include "layers/AirportLayer.h"

#include <array>
#include <cmath>
#include <string>

#include "XMapView.h"
#include "data/AirportGeometryStore.h"
#include "data/NavDatabase.h"
#include "gfx/Canvas.h"
#include "layers/XMapSymbols.h"

namespace
{

struct CategoryStyle
{
    double mMinZoom;       // bunun altinda cizilmez
    double mLabelMinZoom;  // kimlik etiketi
    double mNameMinZoom;   // kimlik + ad
    float mRadius;         // sembol yaricapi (boxel)
};

// Sira AirportLayer::Category ile ayni olmali. Liste ayni zamanda oncelik
// sirasidir: seyreltmede once buyukler yer alir.
const auto kCategoryStyles = std::to_array<CategoryStyle>({
    {4.0, 6.0, 11.0, 6.0f},    // buyuk      (renkler XMapTheme::mAirport)
    {8.0, 9.0, 12.0, 4.0f},    // kucuk
    {9.0, 10.5, 13.0, 4.0f},   // deniz ussu
    {11.0, 12.5, 14.0, 4.5f},  // heliport
    {11.0, 13.0, 99.0, 3.5f},  // kapali
});

const double kHelipadDetailZoom = 12.5;  // AirportObjectsLayer kHelipadMinZoom ile ayni
const size_t kMaxSymbols = 3000;  // FPS korumasi: bir karede en fazla bu kadar sembol
const double kEdgeMargin = 20.0;  // kenardan tasan semboller de cizilsin (boxel)

const CategoryStyle& styleOf(AirportLayer::Category pCategory)
{
    return kCategoryStyles[static_cast<int>(pCategory)];
}

}

AirportLayer::AirportLayer(const NavDatabase& pDatabase, const AirportGeometryStore* pGeometry)
    : Layer("airports", "Airport symbols", true)
    , mDatabase(pDatabase)
    , mGeometry(pGeometry)
{
}

AirportLayer::Category AirportLayer::categoryOf(AirportKind pAirportKind, bool pMajor)
{
    switch (pAirportKind)
    {
        case AirportKind::Seaplane:
            return Category::Seaplane;
        case AirportKind::Heliport:
            return Category::Heliport;
        case AirportKind::Closed:
            return Category::Closed;
        default:
            return pMajor ? Category::Major : Category::Minor;
    }
}

void AirportLayer::collectVisible(const drawContext& pContext)
{
    mVisible.clear();
    const XMapView& tView = *pContext.mView;
    if (!mDatabase.isReady() || tView.zoom() < styleOf(Category::Major).mMinZoom)
    {
        return;
    }

    RectD tArea = pContext.mArea;
    tArea.mLeft -= kEdgeMargin;
    tArea.mRight += kEdgeMargin;
    tArea.mBottom -= kEdgeMargin;
    tArea.mTop += kEdgeMargin;

    mQuery.clear();
    mDatabase.query(NavKind::Airport, tView.boundsOf(tArea), mQuery);
    mSymbolPlacer.reset(tArea);

    // Kategori kategori, onemliden onemsize: yer kalirsa sonrakiler eklenir
    const XMapTheme& tTheme = *pContext.mTheme;
    const bool tByTower = tTheme.mAirportByTower && mGeometry && mGeometry->isReady();
    for (int tCategoryIndex = 0; tCategoryIndex < static_cast<int>(kCategoryStyles.size()); ++tCategoryIndex)
    {
        const Category tCategory = static_cast<Category>(tCategoryIndex);
        const CategoryStyle& tStyle = styleOf(tCategory);
        if (tView.zoom() < tStyle.mMinZoom || (tTheme.mMajorAirportsOnly && tCategory != Category::Major))
        {
            continue;
        }
        for (int tIndex : mQuery)
        {
            const NavRecord& tRecord = mDatabase.at(tIndex);
            if (categoryOf(tRecord.mAirportKind, tRecord.mMajor) != tCategory)
            {
                continue;
            }
            double tX = 0.0;
            double tY = 0.0;
            tView.latLonToBoxel({tRecord.mLat, tRecord.mLon}, &tX, &tY);
            if (!tArea.contains(tX, tY))
            {
                continue;
            }
            const double tRadius = tStyle.mRadius + 1.0;
            RectD tBox;
            tBox.mLeft = tX - tRadius;
            tBox.mRight = tX + tRadius;
            tBox.mBottom = tY - tRadius;
            tBox.mTop = tY + tRadius;
            if (!mSymbolPlacer.tryPlace(tBox))
            {
                continue;  // baska bir havalimaninin sembolunun ustune biniyor
            }
            if (pContext.mLabels)
            {
                pContext.mLabels->reserve(tBox);  // hicbir katmanin etiketi bu sembolu ortmesin
            }
            Rgba tColor = tTheme.mAirport[tCategoryIndex];
            Rgba tLabelColor = tTheme.mAirportLabel[tCategoryIndex];
            if (tByTower && (tCategory == Category::Major || tCategory == Category::Minor))
            {
                // Sectional kurali: kuleli mavi, kulesiz macenta. Kule = apt.dat 54/1054 satiri.
                const AirportGeometry* tSummary = mGeometry->find(tRecord.mId);
                const bool tTowered = tSummary && tSummary->mHasTower;
                tColor = tTowered ? tTheme.mAirportTowered : tTheme.mAirportUntowered;
                tLabelColor = tTowered ? tTheme.mAirportLabel[0] : tTheme.mAirportLabel[1];
            }
            mVisible.push_back({tIndex, static_cast<float>(tX), static_cast<float>(tY), tCategory, tColor,
                                tLabelColor});
            if (mVisible.size() >= kMaxSymbols)
            {
                return;
            }
        }
    }
}

void AirportLayer::draw(const drawContext& pContext)
{
    collectVisible(pContext);
    if (mVisible.empty())
    {
        return;
    }

    // Semboller: halka (kapalilarda carpi), heliportta "H"; koyu konturlu
    Gfx::OutlinedLines tSymbols(XMapSymbols::kOutlineExtra);
    tSymbols.setOutlineColor(pContext.mTheme->mHalo);
    Gfx::Mesh tDots;
    const float tWidth = XMapSymbols::kLineWidth;
    for (const VisibleAirport& tAirport : mVisible)
    {
        const CategoryStyle& tStyle = styleOf(tAirport.mCategory);
        const float tRadius = tStyle.mRadius;
        const float tX = tAirport.mX;
        const float tY = tAirport.mY;
        switch (tAirport.mCategory)
        {
            case Category::Closed:
                tSymbols.addLine({tX - tRadius, tY - tRadius}, {tX + tRadius, tY + tRadius}, tWidth, tAirport.mColor);
                tSymbols.addLine({tX - tRadius, tY + tRadius}, {tX + tRadius, tY - tRadius}, tWidth, tAirport.mColor);
                break;
            case Category::Heliport:
            {
                if (pContext.mView->zoom() >= kHelipadDetailZoom)
                {
                    break;  // nesne katmani gercek helipadi (yone donuk H) ciziyor; ustune binmesin
                }
                tSymbols.addRing({tX, tY}, tRadius, tWidth, tAirport.mColor);
                const float tH = tRadius * 0.55f;
                tSymbols.addLine({tX - tH, tY - tH}, {tX - tH, tY + tH}, tWidth, tAirport.mColor);
                tSymbols.addLine({tX + tH, tY - tH}, {tX + tH, tY + tH}, tWidth, tAirport.mColor);
                tSymbols.addLine({tX - tH, tY}, {tX + tH, tY}, tWidth, tAirport.mColor);
                break;
            }
            case Category::Major:
                tSymbols.addRing({tX, tY}, tRadius, tWidth, tAirport.mColor);
                tDots.addDisc({tX, tY}, 2.0f, tAirport.mColor);  // buyuklerin ortasinda dolu nokta
                break;
            default:
                tSymbols.addRing({tX, tY}, tRadius, tWidth, tAirport.mColor);
                break;
        }
    }
    pContext.mCanvas->draw(tSymbols);
    pContext.mCanvas->draw(tDots);
}

void AirportLayer::drawLabels(const drawContext& pContext)
{
    if (!pContext.mLabels)
    {
        return;
    }
    const double tZoom = pContext.mView->zoom();
    for (const VisibleAirport& tAirport : mVisible)
    {
        const CategoryStyle& tStyle = styleOf(tAirport.mCategory);
        if (tZoom < tStyle.mLabelMinZoom)
        {
            continue;
        }
        const NavRecord& tRecord = mDatabase.at(tAirport.mIndex);
        std::string tText = tRecord.mId;
        if (tZoom >= tStyle.mNameMinZoom && !tRecord.mName.empty())
        {
            tText += "  " + tRecord.mName;
        }
        XMapSymbols::drawLabelNear(*pContext.mLabels, tAirport.mX, tAirport.mY, tStyle.mRadius + 4.0f, tText,
                                  tAirport.mLabelColor, pContext.labelShadow());
    }
}

std::string AirportLayer::describe(const drawContext& /*pContext*/, double pX, double pY) const
{
    for (const VisibleAirport& tAirport : mVisible)
    {
        if (std::hypot(tAirport.mX - pX, tAirport.mY - pY) <= styleOf(tAirport.mCategory).mRadius + 3.0)
        {
            const NavRecord& tRecord = mDatabase.at(tAirport.mIndex);
            return tRecord.mName.empty() ? tRecord.mId : tRecord.mId + "  " + tRecord.mName;
        }
    }
    return "";
}
