#include "engine/LayerTable.h"

#include <array>
#include <string>

#include "LayerManager.h"
#include "XMapTheme.h"

namespace
{
using prime::LayerKind;
using prime::XMapLayer;

struct LayerEntry
{
    XMapLayer mLayer;
    LayerKind mKind;
    XMapLayer mParent;
    const char* mKey;
    const char* mName;
    const char* mPanelGroup;
};

const XMapLayer kNone = XMapLayer::Count;

const auto kTable = std::to_array<LayerEntry>({
    {XMapLayer::XMapGroup, LayerKind::Group, kNone, "group.map", "XMap", "XMap"},
    {XMapLayer::Terrain, LayerKind::Layer, XMapLayer::XMapGroup, "terrain", "Terrain (scenery)", nullptr},
    {XMapLayer::TerrainForests, LayerKind::SubLayer, XMapLayer::Terrain, "terrain.forests", "Forests", nullptr},
    {XMapLayer::TerrainBuiltUp, LayerKind::SubLayer, XMapLayer::Terrain, "terrain.built_up", "Built-up areas",
     nullptr},
    {XMapLayer::Roads, LayerKind::Layer, XMapLayer::XMapGroup, "roads", "Roads and railways", nullptr},
    {XMapLayer::RoadsMotorways, LayerKind::SubLayer, XMapLayer::Roads, "roads.motorways", "Motorways", nullptr},
    {XMapLayer::RoadsPrimary, LayerKind::SubLayer, XMapLayer::Roads, "roads.primary", "Primary roads", nullptr},
    {XMapLayer::RoadsSecondary, LayerKind::SubLayer, XMapLayer::Roads, "roads.secondary", "Secondary roads", nullptr},
    {XMapLayer::RoadsLocal, LayerKind::SubLayer, XMapLayer::Roads, "roads.local", "Local roads", nullptr},
    {XMapLayer::RoadsRailways, LayerKind::SubLayer, XMapLayer::Roads, "roads.railways", "Railways", nullptr},
    {XMapLayer::RoadsPowerLines, LayerKind::SubLayer, XMapLayer::Roads, "roads.power_lines", "Power lines", nullptr},
    {XMapLayer::Grid, LayerKind::Layer, XMapLayer::XMapGroup, "grid", "Lat / lon grid", nullptr},
    {XMapLayer::AirportsGroup, LayerKind::Group, kNone, "group.airports", "Airports", "Airports"},
    {XMapLayer::AirportSymbols, LayerKind::Layer, XMapLayer::AirportsGroup, "airports", "Airport symbols", nullptr},
    {XMapLayer::Runways, LayerKind::Layer, XMapLayer::AirportsGroup, "runways", "Runways", nullptr},
    {XMapLayer::RunwaysPaved, LayerKind::SubLayer, XMapLayer::Runways, "runways.paved", "Paved (land)", nullptr},
    {XMapLayer::RunwaysUnpaved, LayerKind::SubLayer, XMapLayer::Runways, "runways.unpaved", "Unpaved (land)",
     nullptr},
    {XMapLayer::RunwaysWater, LayerKind::SubLayer, XMapLayer::Runways, "runways.water", "Water runways", nullptr},
    {XMapLayer::RunwaysOverruns, LayerKind::SubLayer, XMapLayer::Runways, "runways.overruns", "Blast pads / overruns",
     nullptr},
    {XMapLayer::RunwaysMarkings, LayerKind::SubLayer, XMapLayer::Runways, "runways.markings", "Markings", nullptr},
    {XMapLayer::RunwaysNumbers, LayerKind::SubLayer, XMapLayer::Runways, "runways.numbers", "Runway numbers", nullptr},
    {XMapLayer::Surfaces, LayerKind::Layer, XMapLayer::AirportsGroup, "airport_surfaces", "Airport surfaces", nullptr},
    {XMapLayer::SurfacesPavement, LayerKind::SubLayer, XMapLayer::Surfaces, "airport_surfaces.pavement", "Pavement",
     nullptr},
    {XMapLayer::SurfacesPavementEdges, LayerKind::SubLayer, XMapLayer::Surfaces, "airport_surfaces.pavement_edges",
     "Pavement edges", nullptr},
    {XMapLayer::SurfacesBoundary, LayerKind::SubLayer, XMapLayer::Surfaces, "airport_surfaces.boundary",
     "Airport boundary", nullptr},
    {XMapLayer::SurfacesMarkings, LayerKind::SubLayer, XMapLayer::Surfaces, "airport_surfaces.markings",
     "Painted lines", nullptr},
    {XMapLayer::Objects, LayerKind::Layer, XMapLayer::AirportsGroup, "airport_objects", "Airport objects", nullptr},
    {XMapLayer::ObjectsLighting, LayerKind::SubLayer, XMapLayer::Objects, "airport_objects.lighting",
     "Edge / taxi lights", nullptr},
    {XMapLayer::ObjectsHelipads, LayerKind::SubLayer, XMapLayer::Objects, "airport_objects.helipads", "Helipads",
     nullptr},
    {XMapLayer::ObjectsBeacons, LayerKind::SubLayer, XMapLayer::Objects, "airport_objects.beacons", "Beacons",
     nullptr},
    {XMapLayer::ObjectsWindsocks, LayerKind::SubLayer, XMapLayer::Objects, "airport_objects.windsocks", "Windsocks",
     nullptr},
    {XMapLayer::ObjectsGates, LayerKind::SubLayer, XMapLayer::Objects, "airport_objects.gates", "Gates", nullptr},
    {XMapLayer::ObjectsTieDowns, LayerKind::SubLayer, XMapLayer::Objects, "airport_objects.tie_downs", "Tie-downs",
     nullptr},
    {XMapLayer::ObjectsHangars, LayerKind::SubLayer, XMapLayer::Objects, "airport_objects.hangars", "Hangars",
     nullptr},
    {XMapLayer::ObjectsOtherStarts, LayerKind::SubLayer, XMapLayer::Objects, "airport_objects.other_starts",
     "Other start locations", nullptr},
    {XMapLayer::ObjectsStartNames, LayerKind::SubLayer, XMapLayer::Objects, "airport_objects.start_names",
     "Parking names", nullptr},
    {XMapLayer::ObjectsApproachLights, LayerKind::SubLayer, XMapLayer::Objects, "airport_objects.approach_lights",
     "PAPI / VASI", nullptr},
    {XMapLayer::ObjectsSigns, LayerKind::SubLayer, XMapLayer::Objects, "airport_objects.signs", "Taxi signs",
     nullptr},
    {XMapLayer::TaxiRoutes, LayerKind::Layer, XMapLayer::AirportsGroup, "taxi_routes", "Taxi routes", nullptr},
    {XMapLayer::NavaidsGroup, LayerKind::Group, kNone, "group.navaids", "Navaids", "Navaids"},
    {XMapLayer::Vor, LayerKind::Layer, XMapLayer::NavaidsGroup, "vor", "VOR / DME", nullptr},
    {XMapLayer::Ndb, LayerKind::Layer, XMapLayer::NavaidsGroup, "ndb", "NDB", nullptr},
    {XMapLayer::Ils, LayerKind::Layer, XMapLayer::NavaidsGroup, "ils", "ILS / LOC", nullptr},
    {XMapLayer::Fixes, LayerKind::Layer, XMapLayer::NavaidsGroup, "fix", "Fixes", nullptr},
    {XMapLayer::AirwaysGroup, LayerKind::Group, kNone, "group.airways", "Airways", "Airways"},
    {XMapLayer::AirwaysLow, LayerKind::Layer, XMapLayer::AirwaysGroup, "airways_low", "Airways (low)", nullptr},
    {XMapLayer::AirwaysHigh, LayerKind::Layer, XMapLayer::AirwaysGroup, "airways_high", "Airways (high)", nullptr},
    {XMapLayer::FlightGroup, LayerKind::Group, kNone, "group.flight", "Flight", "Flight"},
    {XMapLayer::Aircraft, LayerKind::Layer, XMapLayer::FlightGroup, "aircraft", "Aircraft", nullptr},
    {XMapLayer::FmsRoute, LayerKind::Layer, XMapLayer::FlightGroup, "fms", "FMS route", nullptr},
    {XMapLayer::FlightTrack, LayerKind::Layer, XMapLayer::FlightGroup, "track", "Flight track", nullptr},
    {XMapLayer::Drawings, LayerKind::Layer, kNone, "drawings", "User drawings", nullptr},
    {XMapLayer::DrawingsPoints, LayerKind::SubLayer, XMapLayer::Drawings, "drawings.points", "Points", nullptr},
    {XMapLayer::DrawingsRoutes, LayerKind::SubLayer, XMapLayer::Drawings, "drawings.routes", "Routes", nullptr},
    {XMapLayer::DrawingsRegions, LayerKind::SubLayer, XMapLayer::Drawings, "drawings.regions", "Regions", nullptr},
    {XMapLayer::DrawingsLabels, LayerKind::SubLayer, XMapLayer::Drawings, "drawings.labels", "Names and distances",
     nullptr},
});

static_assert(kTable.size() == static_cast<size_t>(XMapLayer::Count), "XMapLayer table must cover the enum");

const LayerEntry* entryOf(XMapLayer pLayer)
{
    const size_t tIndex = static_cast<size_t>(pLayer);
    if (tIndex >= kTable.size())
    {
        return nullptr;
    }
    return &kTable[tIndex];
}

bool builtInDefault(std::string_view pKey)
{
    return pKey != "taxi_routes" && pKey != "fix" && pKey != "roads.power_lines";
}

bool themeDefaultOf(const XMapTheme& pTheme, std::string_view pKey)
{
    for (const LayerDefault& tDefault : pTheme.mLayerDefaults)
    {
        if (pKey == tDefault.mId)
        {
            return tDefault.mVisible;
        }
    }
    return builtInDefault(pKey);
}
}

namespace LayerRules
{
using prime::LayerKind;
using prime::XMapLayer;

std::optional<XMapLayer> parentOf(XMapLayer pLayer)
{
    const LayerEntry* tEntry = entryOf(pLayer);
    if (!tEntry || tEntry->mParent == kNone)
    {
        return std::nullopt;
    }
    return tEntry->mParent;
}

std::vector<XMapLayer> childrenOf(XMapLayer pLayer)
{
    std::vector<XMapLayer> tChildren;
    for (const LayerEntry& tEntry : kTable)
    {
        if (tEntry.mParent == pLayer && pLayer != kNone)
        {
            tChildren.push_back(tEntry.mLayer);
        }
    }
    return tChildren;
}

std::vector<XMapLayer> topLevelLayers()
{
    std::vector<XMapLayer> tTop;
    for (const LayerEntry& tEntry : kTable)
    {
        if (tEntry.mParent == kNone)
        {
            tTop.push_back(tEntry.mLayer);
        }
    }
    return tTop;
}

LayerKind kindOf(XMapLayer pLayer)
{
    const LayerEntry* tEntry = entryOf(pLayer);
    return tEntry ? tEntry->mKind : LayerKind::Layer;
}

const char* layerName(XMapLayer pLayer)
{
    const LayerEntry* tEntry = entryOf(pLayer);
    return tEntry ? tEntry->mName : "";
}

const char* layerKey(XMapLayer pLayer)
{
    const LayerEntry* tEntry = entryOf(pLayer);
    return tEntry ? tEntry->mKey : "";
}

std::optional<XMapLayer> layerFromKey(std::string_view pKey)
{
    for (const LayerEntry& tEntry : kTable)
    {
        if (pKey == tEntry.mKey)
        {
            return tEntry.mLayer;
        }
    }
    return std::nullopt;
}

bool isLayerDefaultVisible(XMapLayer pLayer, prime::XMapMode pMode)
{
    const XMapTheme& tTheme = themeFor(static_cast<::XMapMode>(pMode));
    const LayerEntry* tEntry = entryOf(pLayer);
    if (!tEntry)
    {
        return false;
    }
    if (tEntry->mKind == LayerKind::Group)
    {
        for (XMapLayer tChild : childrenOf(pLayer))
        {
            if (themeDefaultOf(tTheme, layerKey(tChild)))
            {
                return true;
            }
        }
        return false;
    }
    return themeDefaultOf(tTheme, tEntry->mKey);
}
}

namespace LayerRules
{
Layer* findLayer(const LayerManager& pLayers, XMapLayer pLayer)
{
    const LayerEntry* tEntry = entryOf(pLayer);
    if (!tEntry || tEntry->mKind == LayerKind::Group)
    {
        return nullptr;
    }
    const XMapLayer tOwner = tEntry->mKind == LayerKind::SubLayer ? tEntry->mParent : pLayer;
    const std::string_view tId = LayerRules::layerKey(tOwner);
    for (int tIndex = 0; tIndex < pLayers.count(); ++tIndex)
    {
        if (pLayers.at(tIndex)->id() == tId)
        {
            return pLayers.at(tIndex);
        }
    }
    return nullptr;
}

int subLayerIndex(const LayerManager& pLayers, XMapLayer pLayer)
{
    const LayerEntry* tEntry = entryOf(pLayer);
    const Layer* tLayer = findLayer(pLayers, pLayer);
    if (!tEntry || !tLayer || tEntry->mKind != LayerKind::SubLayer)
    {
        return -1;
    }
    const std::string_view tKey = tEntry->mKey;
    const std::string_view tSubId = tKey.substr(tKey.find('.') + 1);
    for (int tSub = 0; tSub < tLayer->subLayerCount(); ++tSub)
    {
        if (tLayer->subLayerAt(tSub).mId == tSubId)
        {
            return tSub;
        }
    }
    return -1;
}

bool isVisible(const LayerManager& pLayers, XMapLayer pLayer)
{
    const LayerEntry* tEntry = entryOf(pLayer);
    if (!tEntry)
    {
        return false;
    }
    if (tEntry->mKind == LayerKind::Group)
    {
        return state(pLayers, pLayer) != prime::LayerState::Off;
    }
    const Layer* tLayer = findLayer(pLayers, pLayer);
    if (!tLayer)
    {
        return false;
    }
    if (tEntry->mKind == LayerKind::SubLayer)
    {
        return tLayer->isVisible() && tLayer->isSubLayerVisible(subLayerIndex(pLayers, pLayer));
    }
    return tLayer->isVisible();
}

prime::LayerState state(const LayerManager& pLayers, XMapLayer pLayer)
{
    using prime::LayerState;
    const LayerEntry* tEntry = entryOf(pLayer);
    if (!tEntry)
    {
        return LayerState::Off;
    }
    if (tEntry->mKind == LayerKind::Group)
    {
        int tOn = 0;
        int tAll = 0;
        for (XMapLayer tChild : LayerRules::childrenOf(pLayer))
        {
            const Layer* tLayer = findLayer(pLayers, tChild);
            if (tLayer)
            {
                ++tAll;
                tOn += tLayer->isVisible() ? 1 : 0;
            }
        }
        return tOn == 0 ? LayerState::Off : (tOn == tAll ? LayerState::On : LayerState::Mixed);
    }
    const Layer* tLayer = findLayer(pLayers, pLayer);
    if (!tLayer)
    {
        return LayerState::Off;
    }
    if (tEntry->mKind == LayerKind::SubLayer)
    {
        return isVisible(pLayers, pLayer) ? LayerState::On : LayerState::Off;
    }
    if (!tLayer->isVisible())
    {
        return LayerState::Off;
    }
    return tLayer->visibleSubLayerCount() < tLayer->subLayerCount() ? LayerState::Mixed : LayerState::On;
}

namespace
{
bool setLayerOnly(Layer& pLayer, bool pVisible)
{
    bool tChanged = pLayer.isVisible() != pVisible;
    pLayer.setVisible(pVisible);
    if (pVisible && pLayer.subLayerCount() > 0 && pLayer.visibleSubLayerCount() == 0)
    {
        for (int tSub = 0; tSub < pLayer.subLayerCount(); ++tSub)
        {
            pLayer.setSubLayerVisible(tSub, true);
        }
        tChanged = true;
    }
    return tChanged;
}

bool setSubLayer(Layer& pLayer, int pSub, bool pVisible)
{
    if (pSub < 0)
    {
        return false;
    }
    const bool tWasVisible = pLayer.isVisible() && pLayer.isSubLayerVisible(pSub);
    if (tWasVisible == pVisible)
    {
        return false;
    }
    if (pVisible && !pLayer.isVisible())
    {
        for (int tSub = 0; tSub < pLayer.subLayerCount(); ++tSub)
        {
            pLayer.setSubLayerVisible(tSub, false);
        }
    }
    pLayer.setSubLayerVisible(pSub, pVisible);
    if (pVisible)
    {
        pLayer.setVisible(true);
    }
    else if (pLayer.visibleSubLayerCount() == 0)
    {
        pLayer.setVisible(false);
    }
    return true;
}

std::vector<Layer*> groupDefaultLayers(const LayerManager& pLayers, const XMapTheme& pTheme, XMapLayer pGroup)
{
    std::vector<Layer*> tAll;
    std::vector<Layer*> tDefaultOn;
    for (XMapLayer tChild : LayerRules::childrenOf(pGroup))
    {
        Layer* tLayer = findLayer(pLayers, tChild);
        if (!tLayer)
        {
            continue;
        }
        tAll.push_back(tLayer);
        if (LayerManager::themeDefault(*tLayer, pTheme))
        {
            tDefaultOn.push_back(tLayer);
        }
    }
    return tDefaultOn.empty() ? tAll : tDefaultOn;
}
}

bool setVisible(LayerManager& pLayers, const XMapTheme& pTheme, XMapLayer pLayer, bool pVisible)
{
    const LayerEntry* tEntry = entryOf(pLayer);
    if (!tEntry)
    {
        return false;
    }
    if (tEntry->mKind == LayerKind::Group)
    {
        bool tChanged = false;
        if (pVisible)
        {
            for (Layer* tLayer : groupDefaultLayers(pLayers, pTheme, pLayer))
            {
                tChanged = setLayerOnly(*tLayer, true) || tChanged;
            }
        }
        else
        {
            for (XMapLayer tChild : LayerRules::childrenOf(pLayer))
            {
                Layer* tLayer = findLayer(pLayers, tChild);
                if (tLayer)
                {
                    tChanged = setLayerOnly(*tLayer, false) || tChanged;
                }
            }
        }
        return tChanged;
    }
    Layer* tLayer = findLayer(pLayers, pLayer);
    if (!tLayer)
    {
        return false;
    }
    if (tEntry->mKind == LayerKind::SubLayer)
    {
        return setSubLayer(*tLayer, subLayerIndex(pLayers, pLayer), pVisible);
    }
    return setLayerOnly(*tLayer, pVisible);
}

bool toggle(LayerManager& pLayers, const XMapTheme& pTheme, XMapLayer pLayer)
{
    const LayerEntry* tEntry = entryOf(pLayer);
    if (!tEntry)
    {
        return false;
    }
    if (tEntry->mKind == LayerKind::Group)
    {
        bool tTurnOn = false;
        for (Layer* tLayer : groupDefaultLayers(pLayers, pTheme, pLayer))
        {
            tTurnOn = tTurnOn || !tLayer->isVisible();
        }
        return setVisible(pLayers, pTheme, pLayer, tTurnOn);
    }
    return setVisible(pLayers, pTheme, pLayer, !isVisible(pLayers, pLayer));
}

void showOnly(LayerManager& pLayers, const XMapTheme& pTheme, const XMapLayer* pBegin, const XMapLayer* pEnd)
{
    for (int tIndex = 0; tIndex < pLayers.count(); ++tIndex)
    {
        pLayers.at(tIndex)->setVisible(false);
    }
    for (const XMapLayer* tItem = pBegin; tItem != pEnd; ++tItem)
    {
        if (LayerRules::kindOf(*tItem) == LayerKind::Layer)
        {
            Layer* tLayer = findLayer(pLayers, *tItem);
            if (tLayer)
            {
                tLayer->setVisible(true);
                for (int tSub = 0; tSub < tLayer->subLayerCount(); ++tSub)
                {
                    tLayer->setSubLayerVisible(tSub, true);
                }
            }
            continue;
        }
        setVisible(pLayers, pTheme, *tItem, true);
    }
}

std::optional<XMapLayer> groupFromPanelName(std::string_view pName)
{
    for (const LayerEntry& tEntry : kTable)
    {
        if (tEntry.mPanelGroup && pName == tEntry.mPanelGroup)
        {
            return tEntry.mLayer;
        }
    }
    return std::nullopt;
}

struct ZoomEntry
{
    XMapLayer mLayer;
    double mMinZoom;
    double mMaxZoom;
    double mHardMinZoom;
};

const auto kZoomTable = std::to_array<ZoomEntry>({
    {XMapLayer::AirportSymbols, 4.0, 99.0, 3.0},
    {XMapLayer::Vor, 5.0, 99.0, 3.0},
    {XMapLayer::Ndb, 7.0, 99.0, 3.0},
    {XMapLayer::Ils, 8.5, 99.0, 4.0},
    {XMapLayer::Fixes, 9.0, 99.0, 7.0},
    {XMapLayer::Terrain, 5.0, 99.0, 4.0},
    {XMapLayer::AirwaysLow, 6.5, 99.0, 5.0},
    {XMapLayer::AirwaysHigh, 6.5, 99.0, 6.0},
    {XMapLayer::Runways, 11.0, 99.0, 4.0},
    {XMapLayer::Surfaces, 10.5, 99.0, 6.0},
    {XMapLayer::TaxiRoutes, 13.5, 99.0, 9.5},
    {XMapLayer::Objects, 12.5, 99.0, 12.0},
    {XMapLayer::RoadsMotorways, 7.5, 99.0, 6.5},
    {XMapLayer::RoadsPrimary, 9.0, 99.0, 8.0},
    {XMapLayer::RoadsSecondary, 10.5, 99.0, 9.5},
    {XMapLayer::RoadsLocal, 12.0, 99.0, 11.0},
    {XMapLayer::RoadsRailways, 9.5, 99.0, 8.5},
    {XMapLayer::RoadsPowerLines, 11.0, 99.0, 10.0},
    {XMapLayer::SurfacesPavement, 11.0, 99.0, 6.0},
    {XMapLayer::SurfacesPavementEdges, 11.0, 99.0, 6.0},
    {XMapLayer::SurfacesBoundary, 10.5, 99.0, 6.0},
    {XMapLayer::SurfacesMarkings, 13.5, 99.0, 6.0},
    {XMapLayer::ObjectsLighting, 14.5, 99.0, 12.0},
    {XMapLayer::ObjectsHelipads, 12.5, 99.0, 12.0},
    {XMapLayer::ObjectsBeacons, 12.5, 99.0, 12.0},
    {XMapLayer::ObjectsWindsocks, 13.5, 99.0, 12.0},
    {XMapLayer::ObjectsGates, 14.0, 99.0, 12.0},
    {XMapLayer::ObjectsTieDowns, 14.0, 99.0, 12.0},
    {XMapLayer::ObjectsHangars, 14.0, 99.0, 12.0},
    {XMapLayer::ObjectsOtherStarts, 14.0, 99.0, 12.0},
    {XMapLayer::ObjectsStartNames, 15.5, 99.0, 12.0},
    {XMapLayer::ObjectsApproachLights, 14.5, 99.0, 12.0},
    {XMapLayer::ObjectsSigns, 15.5, 99.0, 12.0},
});

LayerZoomDefaults zoomDefaultsOf(XMapLayer pLayer)
{
    for (const ZoomEntry& tEntry : kZoomTable)
    {
        if (tEntry.mLayer == pLayer)
        {
            return {tEntry.mMinZoom, tEntry.mMaxZoom, tEntry.mHardMinZoom};
        }
    }
    return {0.0, 99.0, 0.0};
}

void applyBuiltInZoomRanges(LayerManager& pLayers)
{
    for (int tIndex = 0; tIndex < pLayers.count(); ++tIndex)
    {
        Layer& tLayer = *pLayers.at(tIndex);
        const std::optional<XMapLayer> tItem = layerFromKey(tLayer.id());
        if (tItem)
        {
            const LayerZoomDefaults tDefaults = zoomDefaultsOf(*tItem);
            tLayer.setBuiltInZoomRange(tDefaults.mMinZoom, tDefaults.mMaxZoom, tDefaults.mHardMinZoom);
        }
        for (int tSub = 0; tSub < tLayer.subLayerCount(); ++tSub)
        {
            const std::optional<XMapLayer> tSubItem = layerFromKey(LayerManager::subLayerKey(tLayer, tSub));
            if (tSubItem)
            {
                const LayerZoomDefaults tDefaults = zoomDefaultsOf(*tSubItem);
                tLayer.setSubLayerBuiltInZoomRange(tSub, tDefaults.mMinZoom, tDefaults.mMaxZoom,
                                                   tDefaults.mHardMinZoom);
            }
        }
    }
}
}
