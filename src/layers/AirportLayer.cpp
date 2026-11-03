// AirportLayer.cpp
#include "layers/AirportLayer.h"

#include <cstdio>

#include "GLInclude.h"
#include "MapView.h"
#include "data/NavDatabase.h"
#include "layers/MapSymbols.h"

namespace
{

struct CategoryStyle
{
    double mMinZoom;       // bunun altinda cizilmez
    double mLabelMinZoom;  // kimlik etiketi
    double mNameMinZoom;   // kimlik + ad
    float mRadius;         // sembol yaricapi (boxel)
    Rgba mColor;
    Rgba mLabelColor;
};

// Sira AirportLayer::Category ile ayni olmali. Liste ayni zamanda oncelik
// sirasidir: seyreltmede once buyukler yer alir.
const CategoryStyle kCategoryStyles[] = {
    {4.0, 6.0, 11.0, 6.0f, {0.40f, 0.72f, 1.00f, 1.0f}, {0.80f, 0.90f, 1.00f, 1.0f}},   // buyuk
    {8.0, 9.0, 12.0, 4.0f, {0.55f, 0.65f, 0.78f, 1.0f}, {0.66f, 0.73f, 0.82f, 1.0f}},   // kucuk
    {9.0, 10.5, 13.0, 4.0f, {0.35f, 0.62f, 0.95f, 1.0f}, {0.60f, 0.75f, 0.95f, 1.0f}},  // deniz ussu
    {11.0, 12.5, 14.0, 4.5f, {0.72f, 0.62f, 0.92f, 1.0f}, {0.78f, 0.72f, 0.92f, 1.0f}}, // heliport
    {11.0, 13.0, 99.0, 3.5f, {0.50f, 0.52f, 0.56f, 1.0f}, {0.58f, 0.60f, 0.64f, 1.0f}}, // kapali
};
const int kCategoryCount = 5;

const size_t kMaxSymbols = 3000;  // FPS korumasi: bir karede en fazla bu kadar sembol
const double kEdgeMargin = 20.0;  // kenardan tasan semboller de cizilsin (boxel)

const CategoryStyle& StyleOf(AirportLayer::Category pCategory)
{
    return kCategoryStyles[static_cast<int>(pCategory)];
}

}  // namespace

AirportLayer::AirportLayer(const NavDatabase& pDatabase)
    : Layer("airports", "Airports", true)
    , mDatabase(pDatabase)
{
}

AirportLayer::Category AirportLayer::CategoryOf(AirportKind pAirportKind, bool pMajor)
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

void AirportLayer::CollectVisible(const DrawContext& pContext)
{
    mVisible.clear();
    const MapView& tView = *pContext.mView;
    if (!mDatabase.IsReady() || tView.Zoom() < StyleOf(Category::Major).mMinZoom)
    {
        return;
    }

    RectD tArea = pContext.mArea;
    tArea.mLeft -= kEdgeMargin;
    tArea.mRight += kEdgeMargin;
    tArea.mBottom -= kEdgeMargin;
    tArea.mTop += kEdgeMargin;

    mQuery.clear();
    mDatabase.Query(NavKind::Airport, tView.BoundsOf(tArea), mQuery);
    mSymbolPlacer.Reset(tArea);

    // Kategori kategori, onemliden onemsize: yer kalirsa sonrakiler eklenir
    for (int tCategoryIndex = 0; tCategoryIndex < kCategoryCount; ++tCategoryIndex)
    {
        const Category tCategory = static_cast<Category>(tCategoryIndex);
        const CategoryStyle& tStyle = StyleOf(tCategory);
        if (tView.Zoom() < tStyle.mMinZoom)
        {
            continue;
        }
        for (int tIndex : mQuery)
        {
            const NavRecord& tRecord = mDatabase.At(tIndex);
            if (CategoryOf(tRecord.mAirportKind, tRecord.mMajor) != tCategory)
            {
                continue;
            }
            double tX = 0.0;
            double tY = 0.0;
            tView.LatLonToBoxel({tRecord.mLat, tRecord.mLon}, &tX, &tY);
            if (!tArea.Contains(tX, tY))
            {
                continue;
            }
            const double tRadius = tStyle.mRadius + 1.0;
            RectD tBox;
            tBox.mLeft = tX - tRadius;
            tBox.mRight = tX + tRadius;
            tBox.mBottom = tY - tRadius;
            tBox.mTop = tY + tRadius;
            if (!mSymbolPlacer.TryPlace(tBox))
            {
                continue;  // baska bir havalimaninin sembolunun ustune biniyor
            }
            if (pContext.mLabels)
            {
                pContext.mLabels->Reserve(tBox);  // hicbir katmanin etiketi bu sembolu ortmesin
            }
            mVisible.push_back({tIndex, static_cast<float>(tX), static_cast<float>(tY), tCategory});
            if (mVisible.size() >= kMaxSymbols)
            {
                return;
            }
        }
    }
}

void AirportLayer::Draw(const DrawContext& pContext)
{
    CollectVisible(pContext);
    if (mVisible.empty())
    {
        return;
    }
    Ui::UseFlatState();

    // Koyu kontur, sonra renkli sembol: halka (kapalilarda carpi), heliportta "H"
    for (int tPass = 0; tPass < 2; ++tPass)
    {
        const bool tOutline = (tPass == 0);
        glLineWidth(tOutline ? MapSymbols::kOutlineWidth : MapSymbols::kLineWidth);
        MapSymbols::SetColor(MapSymbols::kOutline);
        glBegin(GL_LINES);
        for (const VisibleAirport& tAirport : mVisible)
        {
            const CategoryStyle& tStyle = StyleOf(tAirport.mCategory);
            if (!tOutline)
            {
                MapSymbols::SetColor(tStyle.mColor);
            }
            const float tRadius = tStyle.mRadius;
            if (tAirport.mCategory == Category::Closed)
            {
                glVertex2f(tAirport.mX - tRadius, tAirport.mY - tRadius);
                glVertex2f(tAirport.mX + tRadius, tAirport.mY + tRadius);
                glVertex2f(tAirport.mX - tRadius, tAirport.mY + tRadius);
                glVertex2f(tAirport.mX + tRadius, tAirport.mY - tRadius);
                continue;
            }
            MapSymbols::EmitPolygonEdges(tAirport.mX, tAirport.mY, tRadius, 16, 0.0f);
            if (tAirport.mCategory == Category::Heliport)
            {
                const float tH = tRadius * 0.55f;
                glVertex2f(tAirport.mX - tH, tAirport.mY - tH);
                glVertex2f(tAirport.mX - tH, tAirport.mY + tH);
                glVertex2f(tAirport.mX + tH, tAirport.mY - tH);
                glVertex2f(tAirport.mX + tH, tAirport.mY + tH);
                glVertex2f(tAirport.mX - tH, tAirport.mY);
                glVertex2f(tAirport.mX + tH, tAirport.mY);
            }
        }
        glEnd();
    }

    // Buyuklerin ortasinda dolu nokta
    MapSymbols::SetColor(StyleOf(Category::Major).mColor);
    glBegin(GL_TRIANGLES);
    for (const VisibleAirport& tAirport : mVisible)
    {
        if (tAirport.mCategory == Category::Major)
        {
            MapSymbols::EmitDisc(tAirport.mX, tAirport.mY, 2.0f, 8);
        }
    }
    glEnd();
    glLineWidth(1.0f);
}

void AirportLayer::DrawLabels(const DrawContext& pContext)
{
    if (!pContext.mLabels)
    {
        return;
    }
    const double tZoom = pContext.mView->Zoom();
    char tText[128];
    for (const VisibleAirport& tAirport : mVisible)
    {
        const CategoryStyle& tStyle = StyleOf(tAirport.mCategory);
        if (tZoom < tStyle.mLabelMinZoom)
        {
            continue;
        }
        const NavRecord& tRecord = mDatabase.At(tAirport.mIndex);
        if (tZoom >= tStyle.mNameMinZoom && !tRecord.mName.empty())
        {
            std::snprintf(tText, sizeof(tText), "%s  %s", tRecord.mId.c_str(), tRecord.mName.c_str());
        }
        else
        {
            std::snprintf(tText, sizeof(tText), "%s", tRecord.mId.c_str());
        }
        MapSymbols::DrawLabelNear(*pContext.mLabels, tAirport.mX, tAirport.mY, tStyle.mRadius + 4.0f, tText,
                                  tStyle.mLabelColor);
    }
}
