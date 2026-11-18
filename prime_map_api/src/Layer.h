// Tum harita katmanlarinin taban sinifi: gorunurluk, alt katmanlar ve zoom araligi.
#ifndef PRIME_MAP_SRC_LAYER_H
#define PRIME_MAP_SRC_LAYER_H

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "XMapTheme.h"
#include "ScreenMath.h"

class LabelPlacer;
class XMapView;

namespace Gfx
{
class Canvas;
}

struct drawContext
{
    const XMapView* mView = nullptr;
    Gfx::Canvas* mCanvas = nullptr;
    RectD mArea;
    RectD mClearArea;
    double mPixelsPerBoxel = 1.0;
    float mNow = 0.0f;

    bool mHasCursor = false;
    double mCursorX = 0.0;
    double mCursorY = 0.0;

    LabelPlacer* mLabels = nullptr;

    const XMapTheme* mTheme = &themeFor(XMapMode::Dark);

    Rgba labelShadow() const
    {
        return withAlpha(mTheme->mHalo, 1.0f);
    }
};

class Layer
{
public:
    Layer(std::string_view pId, std::string_view pName, bool pDefaultVisible)
        : mId(pId)
        , mName(pName)
        , mVisible(pDefaultVisible)
        , mDefaultVisible(pDefaultVisible)
    {
    }

    virtual ~Layer() = default;

    Layer(const Layer&) = delete;
    Layer& operator=(const Layer&) = delete;

    const std::string& id() const
    {
        return mId;
    }

    const std::string& name() const
    {
        return mName;
    }

    bool isVisible() const
    {
        return mVisible;
    }

    void setVisible(bool pVisible)
    {
        mVisible = pVisible;
    }

    bool defaultVisible() const
    {
        return mDefaultVisible;
    }

    virtual void update(float )
    {
    }

    virtual bool prepareFrame()
    {
        return false;
    }

    virtual bool isOverlay() const
    {
        return false;
    }

    virtual void draw(const drawContext& pContext) = 0;

    virtual void drawLabels(const drawContext& )
    {
    }

    virtual void drawOverlay(const drawContext& )
    {
    }

    virtual std::string describe(const drawContext& , double , double ) const
    {
        return "";
    }

    virtual int labelPriority() const
    {
        return 50;
    }

    struct SubLayer
    {
        std::string mId;
        std::string mName;
        bool mVisible = true;
        bool mDefaultVisible = true;
        double mMinZoom = kNoMinZoom;
        double mMaxZoom = kNoMaxZoom;
        double mBuiltInMin = kNoMinZoom;
        double mBuiltInMax = kNoMaxZoom;
        double mHardMin = kNoMinZoom;
        bool mHostRange = false;
    };

    int subLayerCount() const
    {
        return static_cast<int>(mSubLayers.size());
    }

    const SubLayer& subLayerAt(int pIndex) const
    {
        return mSubLayers[static_cast<size_t>(pIndex)];
    }

    bool isSubLayerVisible(int pIndex) const
    {
        if (pIndex < 0 || pIndex >= subLayerCount())
        {
            return true;
        }
        return mSubLayers[static_cast<size_t>(pIndex)].mVisible;
    }

    void setSubLayerVisible(int pIndex, bool pVisible)
    {
        if (pIndex >= 0 && pIndex < subLayerCount())
        {
            mSubLayers[static_cast<size_t>(pIndex)].mVisible = pVisible;
        }
    }

    int visibleSubLayerCount() const
    {
        int tCount = 0;
        for (const SubLayer& tSub : mSubLayers)
        {
            tCount += tSub.mVisible ? 1 : 0;
        }
        return tCount;
    }

    static constexpr double kNoMinZoom = 0.0;
    static constexpr double kNoMaxZoom = 99.0;

    void setBuiltInZoomRange(double pMinZoom, double pMaxZoom, double pHardMinZoom)
    {
        mBuiltInMin = pMinZoom;
        mBuiltInMax = pMaxZoom;
        mHardMin = pHardMinZoom;
    }

    void setSubLayerBuiltInZoomRange(int pIndex, double pMinZoom, double pMaxZoom, double pHardMinZoom)
    {
        if (pIndex >= 0 && pIndex < subLayerCount())
        {
            SubLayer& tSub = mSubLayers[static_cast<size_t>(pIndex)];
            tSub.mBuiltInMin = pMinZoom;
            tSub.mBuiltInMax = pMaxZoom;
            tSub.mHardMin = pHardMinZoom;
        }
    }

    double minZoomEff() const
    {
        return mHostRange ? (std::max)(mMinZoom, mHardMin) : (std::max)(mBuiltInMin, mHardMin);
    }

    double maxZoomEff() const
    {
        return mHostRange ? mMaxZoom : mBuiltInMax;
    }

    double hardMinZoom() const
    {
        return mHardMin;
    }

    bool hasHostZoomRange() const
    {
        return mHostRange;
    }

    void setZoomRange(double pMinZoom, double pMaxZoom)
    {
        mMinZoom = pMinZoom;
        mMaxZoom = pMaxZoom;
        mHostRange = true;
    }

    void clearZoomRange()
    {
        mMinZoom = kNoMinZoom;
        mMaxZoom = kNoMaxZoom;
        mHostRange = false;
    }

    void setSubLayerZoomRange(int pIndex, double pMinZoom, double pMaxZoom)
    {
        if (pIndex >= 0 && pIndex < subLayerCount())
        {
            SubLayer& tSub = mSubLayers[static_cast<size_t>(pIndex)];
            tSub.mMinZoom = pMinZoom;
            tSub.mMaxZoom = pMaxZoom;
            tSub.mHostRange = true;
        }
    }

    void clearSubLayerZoomRange(int pIndex)
    {
        if (pIndex >= 0 && pIndex < subLayerCount())
        {
            SubLayer& tSub = mSubLayers[static_cast<size_t>(pIndex)];
            tSub.mMinZoom = kNoMinZoom;
            tSub.mMaxZoom = kNoMaxZoom;
            tSub.mHostRange = false;
        }
    }

    double subMinZoomEff(int pIndex) const
    {
        if (pIndex < 0 || pIndex >= subLayerCount())
        {
            return kNoMinZoom;
        }
        const SubLayer& tSub = mSubLayers[static_cast<size_t>(pIndex)];
        return tSub.mHostRange ? (std::max)(tSub.mMinZoom, tSub.mHardMin)
                               : (std::max)(tSub.mBuiltInMin, tSub.mHardMin);
    }

    double subMaxZoomEff(int pIndex) const
    {
        if (pIndex < 0 || pIndex >= subLayerCount())
        {
            return kNoMaxZoom;
        }
        const SubLayer& tSub = mSubLayers[static_cast<size_t>(pIndex)];
        return tSub.mHostRange ? tSub.mMaxZoom : tSub.mBuiltInMax;
    }

    double subHardMinZoom(int pIndex) const
    {
        return (pIndex < 0 || pIndex >= subLayerCount()) ? kNoMinZoom
                                                         : mSubLayers[static_cast<size_t>(pIndex)].mHardMin;
    }

    bool subHasHostRange(int pIndex) const
    {
        return pIndex >= 0 && pIndex < subLayerCount() && mSubLayers[static_cast<size_t>(pIndex)].mHostRange;
    }

    void setCurrentZoom(double pZoom)
    {
        mCurrentZoom = pZoom;
    }

    bool isInZoomRange() const
    {
        return mCurrentZoom >= minZoomEff() && mCurrentZoom <= maxZoomEff();
    }

    bool isAnySubLayerInZoomRange() const
    {
        for (int tSub = 0; tSub < subLayerCount(); ++tSub)
        {
            if (isSubLayerDrawn(tSub))
            {
                return true;
            }
        }
        return false;
    }

    bool isSubLayerDrawn(int pIndex) const
    {
        if (pIndex < 0 || pIndex >= subLayerCount())
        {
            return true;
        }
        const SubLayer& tSub = mSubLayers[static_cast<size_t>(pIndex)];
        return tSub.mVisible && mCurrentZoom >= subMinZoomEff(pIndex) && mCurrentZoom <= subMaxZoomEff(pIndex);
    }

protected:
    int addSubLayer(std::string_view pId, std::string_view pName, bool pDefaultVisible = true)
    {
        SubLayer tSub;
        tSub.mId = pId;
        tSub.mName = pName;
        tSub.mVisible = pDefaultVisible;
        tSub.mDefaultVisible = pDefaultVisible;
        mSubLayers.push_back(tSub);
        return subLayerCount() - 1;
    }

private:
    std::string mId;
    std::string mName;
    bool mVisible;
    bool mDefaultVisible;
    double mMinZoom = kNoMinZoom;
    double mMaxZoom = kNoMaxZoom;
    double mBuiltInMin = kNoMinZoom;
    double mBuiltInMax = kNoMaxZoom;
    double mHardMin = kNoMinZoom;
    bool mHostRange = false;
    double mCurrentZoom = 10.0;
    std::vector<SubLayer> mSubLayers;
};
#endif
