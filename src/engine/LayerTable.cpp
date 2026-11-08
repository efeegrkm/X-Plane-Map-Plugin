// LayerTable.cpp
#include "engine/LayerTable.h"

#include <array>
#include <string>

#include "LayerManager.h"
#include "MapTheme.h"

namespace
{

using PrimeMap::LayerKind;
using PrimeMap::MapLayer;

struct LayerEntry
{
    MapLayer mLayer;
    LayerKind mKind;
    MapLayer mParent;   // ust oge yoksa MapLayer::Count
    const char* mKey;   // ayar anahtari
    const char* mName;  // panelde gorunen ad
    const char* mPanelGroup;  // grup ogesi icin LayerPanel kGroups adi
};

const MapLayer kNone = MapLayer::Count;

// Sira enum ile AYNI olmali (testte kontrol edilir)
const auto kTable = std::to_array<LayerEntry>({
    {MapLayer::MapGroup, LayerKind::Group, kNone, "group.map", "Map", "Map"},
    {MapLayer::Terrain, LayerKind::Layer, MapLayer::MapGroup, "terrain", "Terrain (scenery)", nullptr},
    {MapLayer::TerrainForests, LayerKind::SubLayer, MapLayer::Terrain, "terrain.forests", "Forests", nullptr},
    {MapLayer::TerrainBuiltUp, LayerKind::SubLayer, MapLayer::Terrain, "terrain.built_up", "Built-up areas",
     nullptr},
    {MapLayer::Roads, LayerKind::Layer, MapLayer::MapGroup, "roads", "Roads and railways", nullptr},
    {MapLayer::RoadsMotorways, LayerKind::SubLayer, MapLayer::Roads, "roads.motorways", "Motorways", nullptr},
    {MapLayer::RoadsPrimary, LayerKind::SubLayer, MapLayer::Roads, "roads.primary", "Primary roads", nullptr},
    {MapLayer::RoadsSecondary, LayerKind::SubLayer, MapLayer::Roads, "roads.secondary", "Secondary roads", nullptr},
    {MapLayer::RoadsLocal, LayerKind::SubLayer, MapLayer::Roads, "roads.local", "Local roads", nullptr},
    {MapLayer::RoadsRailways, LayerKind::SubLayer, MapLayer::Roads, "roads.railways", "Railways", nullptr},
    {MapLayer::RoadsPowerLines, LayerKind::SubLayer, MapLayer::Roads, "roads.power_lines", "Power lines", nullptr},
    {MapLayer::Grid, LayerKind::Layer, MapLayer::MapGroup, "grid", "Lat / lon grid", nullptr},
    {MapLayer::AirportsGroup, LayerKind::Group, kNone, "group.airports", "Airports", "Airports"},
    {MapLayer::AirportSymbols, LayerKind::Layer, MapLayer::AirportsGroup, "airports", "Airport symbols", nullptr},
    {MapLayer::Runways, LayerKind::Layer, MapLayer::AirportsGroup, "runways", "Runways", nullptr},
    {MapLayer::RunwaysPaved, LayerKind::SubLayer, MapLayer::Runways, "runways.paved", "Paved (land)", nullptr},
    {MapLayer::RunwaysUnpaved, LayerKind::SubLayer, MapLayer::Runways, "runways.unpaved", "Unpaved (land)",
     nullptr},
    {MapLayer::RunwaysWater, LayerKind::SubLayer, MapLayer::Runways, "runways.water", "Water runways", nullptr},
    {MapLayer::RunwaysOverruns, LayerKind::SubLayer, MapLayer::Runways, "runways.overruns", "Blast pads / overruns",
     nullptr},
    {MapLayer::RunwaysMarkings, LayerKind::SubLayer, MapLayer::Runways, "runways.markings", "Markings", nullptr},
    {MapLayer::RunwaysNumbers, LayerKind::SubLayer, MapLayer::Runways, "runways.numbers", "Runway numbers", nullptr},
    {MapLayer::Surfaces, LayerKind::Layer, MapLayer::AirportsGroup, "airport_surfaces", "Airport surfaces", nullptr},
    {MapLayer::SurfacesPavement, LayerKind::SubLayer, MapLayer::Surfaces, "airport_surfaces.pavement", "Pavement",
     nullptr},
    {MapLayer::SurfacesPavementEdges, LayerKind::SubLayer, MapLayer::Surfaces, "airport_surfaces.pavement_edges",
     "Pavement edges", nullptr},
    {MapLayer::SurfacesBoundary, LayerKind::SubLayer, MapLayer::Surfaces, "airport_surfaces.boundary",
     "Airport boundary", nullptr},
    {MapLayer::SurfacesMarkings, LayerKind::SubLayer, MapLayer::Surfaces, "airport_surfaces.markings",
     "Painted lines", nullptr},
    {MapLayer::Objects, LayerKind::Layer, MapLayer::AirportsGroup, "airport_objects", "Airport objects", nullptr},
    {MapLayer::ObjectsLighting, LayerKind::SubLayer, MapLayer::Objects, "airport_objects.lighting",
     "Edge / taxi lights", nullptr},
    {MapLayer::ObjectsHelipads, LayerKind::SubLayer, MapLayer::Objects, "airport_objects.helipads", "Helipads",
     nullptr},
    {MapLayer::ObjectsBeacons, LayerKind::SubLayer, MapLayer::Objects, "airport_objects.beacons", "Beacons",
     nullptr},
    {MapLayer::ObjectsWindsocks, LayerKind::SubLayer, MapLayer::Objects, "airport_objects.windsocks", "Windsocks",
     nullptr},
    {MapLayer::ObjectsGates, LayerKind::SubLayer, MapLayer::Objects, "airport_objects.gates", "Gates", nullptr},
    {MapLayer::ObjectsTieDowns, LayerKind::SubLayer, MapLayer::Objects, "airport_objects.tie_downs", "Tie-downs",
     nullptr},
    {MapLayer::ObjectsHangars, LayerKind::SubLayer, MapLayer::Objects, "airport_objects.hangars", "Hangars",
     nullptr},
    {MapLayer::ObjectsOtherStarts, LayerKind::SubLayer, MapLayer::Objects, "airport_objects.other_starts",
     "Other start locations", nullptr},
    {MapLayer::ObjectsStartNames, LayerKind::SubLayer, MapLayer::Objects, "airport_objects.start_names",
     "Parking names", nullptr},
    {MapLayer::ObjectsApproachLights, LayerKind::SubLayer, MapLayer::Objects, "airport_objects.approach_lights",
     "PAPI / VASI", nullptr},
    {MapLayer::ObjectsSigns, LayerKind::SubLayer, MapLayer::Objects, "airport_objects.signs", "Taxi signs",
     nullptr},
    {MapLayer::TaxiRoutes, LayerKind::Layer, MapLayer::AirportsGroup, "taxi_routes", "Taxi routes", nullptr},
    {MapLayer::NavaidsGroup, LayerKind::Group, kNone, "group.navaids", "Navaids", "Navaids"},
    {MapLayer::Vor, LayerKind::Layer, MapLayer::NavaidsGroup, "vor", "VOR / DME", nullptr},
    {MapLayer::Ndb, LayerKind::Layer, MapLayer::NavaidsGroup, "ndb", "NDB", nullptr},
    {MapLayer::Ils, LayerKind::Layer, MapLayer::NavaidsGroup, "ils", "ILS / LOC", nullptr},
    {MapLayer::Fixes, LayerKind::Layer, MapLayer::NavaidsGroup, "fix", "Fixes", nullptr},
    {MapLayer::AirwaysGroup, LayerKind::Group, kNone, "group.airways", "Airways", "Airways"},
    {MapLayer::AirwaysLow, LayerKind::Layer, MapLayer::AirwaysGroup, "airways_low", "Airways (low)", nullptr},
    {MapLayer::AirwaysHigh, LayerKind::Layer, MapLayer::AirwaysGroup, "airways_high", "Airways (high)", nullptr},
    {MapLayer::FlightGroup, LayerKind::Group, kNone, "group.flight", "Flight", "Flight"},
    {MapLayer::Aircraft, LayerKind::Layer, MapLayer::FlightGroup, "aircraft", "Aircraft", nullptr},
    {MapLayer::FmsRoute, LayerKind::Layer, MapLayer::FlightGroup, "fms", "FMS route", nullptr},
    {MapLayer::FlightTrack, LayerKind::Layer, MapLayer::FlightGroup, "track", "Flight track", nullptr},
    {MapLayer::Drawings, LayerKind::Layer, kNone, "drawings", "User drawings", nullptr},
    {MapLayer::DrawingsPoints, LayerKind::SubLayer, MapLayer::Drawings, "drawings.points", "Points", nullptr},
    {MapLayer::DrawingsRoutes, LayerKind::SubLayer, MapLayer::Drawings, "drawings.routes", "Routes", nullptr},
    {MapLayer::DrawingsRegions, LayerKind::SubLayer, MapLayer::Drawings, "drawings.regions", "Regions", nullptr},
    {MapLayer::DrawingsLabels, LayerKind::SubLayer, MapLayer::Drawings, "drawings.labels", "Names and distances",
     nullptr},
});

static_assert(kTable.size() == static_cast<size_t>(MapLayer::Count), "MapLayer table must cover the enum");

const LayerEntry* EntryOf(MapLayer pLayer)
{
    const size_t tIndex = static_cast<size_t>(pLayer);
    if (tIndex >= kTable.size())
    {
        return nullptr;
    }
    return &kTable[tIndex];
}

// Temanin ayari yoksa katmanin kendi varsayilani (Layer yapicilari): sadece bunlar kapali
bool BuiltInDefault(std::string_view pKey)
{
    return pKey != "taxi_routes" && pKey != "fix" && pKey != "roads.power_lines";
}

bool ThemeDefaultOf(const MapTheme& pTheme, std::string_view pKey)
{
    for (const LayerDefault& tDefault : pTheme.mLayerDefaults)
    {
        if (pKey == tDefault.mId)
        {
            return tDefault.mVisible;
        }
    }
    return BuiltInDefault(pKey);
}

}  // namespace

// ============================================================================
// Genel API'nin serbest fonksiyonlari (PrimeMap.h)
// ============================================================================

namespace PrimeMap
{

std::optional<MapLayer> ParentOf(MapLayer pLayer)
{
    const LayerEntry* tEntry = EntryOf(pLayer);
    if (!tEntry || tEntry->mParent == kNone)
    {
        return std::nullopt;
    }
    return tEntry->mParent;
}

std::vector<MapLayer> ChildrenOf(MapLayer pLayer)
{
    std::vector<MapLayer> tChildren;
    for (const LayerEntry& tEntry : kTable)
    {
        if (tEntry.mParent == pLayer && pLayer != kNone)
        {
            tChildren.push_back(tEntry.mLayer);
        }
    }
    return tChildren;
}

std::vector<MapLayer> TopLevelLayers()
{
    std::vector<MapLayer> tTop;
    for (const LayerEntry& tEntry : kTable)
    {
        if (tEntry.mParent == kNone)
        {
            tTop.push_back(tEntry.mLayer);
        }
    }
    return tTop;
}

LayerKind KindOf(MapLayer pLayer)
{
    const LayerEntry* tEntry = EntryOf(pLayer);
    return tEntry ? tEntry->mKind : LayerKind::Layer;
}

const char* LayerName(MapLayer pLayer)
{
    const LayerEntry* tEntry = EntryOf(pLayer);
    return tEntry ? tEntry->mName : "";
}

const char* LayerKey(MapLayer pLayer)
{
    const LayerEntry* tEntry = EntryOf(pLayer);
    return tEntry ? tEntry->mKey : "";
}

std::optional<MapLayer> LayerFromKey(std::string_view pKey)
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

bool IsLayerDefaultVisible(MapLayer pLayer, MapMode pMode)
{
    const MapTheme& tTheme = ThemeFor(static_cast<::MapMode>(pMode));
    const LayerEntry* tEntry = EntryOf(pLayer);
    if (!tEntry)
    {
        return false;
    }
    if (tEntry->mKind == LayerKind::Group)
    {
        for (MapLayer tChild : ChildrenOf(pLayer))
        {
            if (ThemeDefaultOf(tTheme, LayerKey(tChild)))
            {
                return true;
            }
        }
        return false;
    }
    return ThemeDefaultOf(tTheme, tEntry->mKey);
}

}  // namespace PrimeMap

// ============================================================================
// Kurallar
// ============================================================================

namespace LayerRules
{

Layer* FindLayer(const LayerManager& pLayers, MapLayer pLayer)
{
    const LayerEntry* tEntry = EntryOf(pLayer);
    if (!tEntry || tEntry->mKind == LayerKind::Group)
    {
        return nullptr;
    }
    const MapLayer tOwner = tEntry->mKind == LayerKind::SubLayer ? tEntry->mParent : pLayer;
    const std::string_view tId = PrimeMap::LayerKey(tOwner);
    for (int tIndex = 0; tIndex < pLayers.Count(); ++tIndex)
    {
        if (pLayers.At(tIndex)->Id() == tId)
        {
            return pLayers.At(tIndex);
        }
    }
    return nullptr;
}

int SubLayerIndex(const LayerManager& pLayers, MapLayer pLayer)
{
    const LayerEntry* tEntry = EntryOf(pLayer);
    const Layer* tLayer = FindLayer(pLayers, pLayer);
    if (!tEntry || !tLayer || tEntry->mKind != LayerKind::SubLayer)
    {
        return -1;
    }
    const std::string_view tKey = tEntry->mKey;
    const std::string_view tSubId = tKey.substr(tKey.find('.') + 1);
    for (int tSub = 0; tSub < tLayer->SubLayerCount(); ++tSub)
    {
        if (tLayer->SubLayerAt(tSub).mId == tSubId)
        {
            return tSub;
        }
    }
    return -1;
}

bool IsVisible(const LayerManager& pLayers, MapLayer pLayer)
{
    const LayerEntry* tEntry = EntryOf(pLayer);
    if (!tEntry)
    {
        return false;
    }
    if (tEntry->mKind == LayerKind::Group)
    {
        return State(pLayers, pLayer) != PrimeMap::LayerState::Off;
    }
    const Layer* tLayer = FindLayer(pLayers, pLayer);
    if (!tLayer)
    {
        return false;
    }
    if (tEntry->mKind == LayerKind::SubLayer)
    {
        return tLayer->IsVisible() && tLayer->IsSubLayerVisible(SubLayerIndex(pLayers, pLayer));
    }
    return tLayer->IsVisible();
}

PrimeMap::LayerState State(const LayerManager& pLayers, MapLayer pLayer)
{
    using PrimeMap::LayerState;
    const LayerEntry* tEntry = EntryOf(pLayer);
    if (!tEntry)
    {
        return LayerState::Off;
    }
    if (tEntry->mKind == LayerKind::Group)
    {
        // Panelle ayni: gruptaki katmanlarin kac tanesi acik (alt katmanlara bakilmaz)
        int tOn = 0;
        int tAll = 0;
        for (MapLayer tChild : PrimeMap::ChildrenOf(pLayer))
        {
            const Layer* tLayer = FindLayer(pLayers, tChild);
            if (tLayer)
            {
                ++tAll;
                tOn += tLayer->IsVisible() ? 1 : 0;
            }
        }
        return tOn == 0 ? LayerState::Off : (tOn == tAll ? LayerState::On : LayerState::Mixed);
    }
    const Layer* tLayer = FindLayer(pLayers, pLayer);
    if (!tLayer)
    {
        return LayerState::Off;
    }
    if (tEntry->mKind == LayerKind::SubLayer)
    {
        return IsVisible(pLayers, pLayer) ? LayerState::On : LayerState::Off;
    }
    if (!tLayer->IsVisible())
    {
        return LayerState::Off;
    }
    return tLayer->VisibleSubLayerCount() < tLayer->SubLayerCount() ? LayerState::Mixed : LayerState::On;
}

namespace
{

bool SetLayerOnly(Layer& pLayer, bool pVisible)
{
    bool tChanged = pLayer.IsVisible() != pVisible;
    pLayer.SetVisible(pVisible);
    // Butun alt katmanlari kapali bir katman acilirsa hepsini ac (bos katman gostermesin)
    if (pVisible && pLayer.SubLayerCount() > 0 && pLayer.VisibleSubLayerCount() == 0)
    {
        for (int tSub = 0; tSub < pLayer.SubLayerCount(); ++tSub)
        {
            pLayer.SetSubLayerVisible(tSub, true);
        }
        tChanged = true;
    }
    return tChanged;
}

bool SetSubLayer(Layer& pLayer, int pSub, bool pVisible)
{
    if (pSub < 0)
    {
        return false;
    }
    const bool tWasVisible = pLayer.IsVisible() && pLayer.IsSubLayerVisible(pSub);
    if (tWasVisible == pVisible)
    {
        return false;
    }
    if (pVisible && !pLayer.IsVisible())
    {
        // Kapali katmanin bir alt katmani acildi: sadece o acilsin, eski alt katman ayarlari geri gelmesin
        for (int tSub = 0; tSub < pLayer.SubLayerCount(); ++tSub)
        {
            pLayer.SetSubLayerVisible(tSub, false);
        }
    }
    pLayer.SetSubLayerVisible(pSub, pVisible);
    if (pVisible)
    {
        pLayer.SetVisible(true);
    }
    else if (pLayer.VisibleSubLayerCount() == 0)
    {
        pLayer.SetVisible(false);  // son alt katman kapandi: kutu "bos" gorunsun
    }
    return true;
}

// Grubun bu moddaki varsayilan acik katmanlari (hic yoksa hepsi)
std::vector<Layer*> GroupDefaultLayers(const LayerManager& pLayers, const MapTheme& pTheme, MapLayer pGroup)
{
    std::vector<Layer*> tAll;
    std::vector<Layer*> tDefaultOn;
    for (MapLayer tChild : PrimeMap::ChildrenOf(pGroup))
    {
        Layer* tLayer = FindLayer(pLayers, tChild);
        if (!tLayer)
        {
            continue;
        }
        tAll.push_back(tLayer);
        if (LayerManager::ThemeDefault(*tLayer, pTheme))
        {
            tDefaultOn.push_back(tLayer);
        }
    }
    return tDefaultOn.empty() ? tAll : tDefaultOn;
}

}  // namespace

bool SetVisible(LayerManager& pLayers, const MapTheme& pTheme, MapLayer pLayer, bool pVisible)
{
    const LayerEntry* tEntry = EntryOf(pLayer);
    if (!tEntry)
    {
        return false;
    }
    if (tEntry->mKind == LayerKind::Group)
    {
        bool tChanged = false;
        if (pVisible)
        {
            for (Layer* tLayer : GroupDefaultLayers(pLayers, pTheme, pLayer))
            {
                tChanged = SetLayerOnly(*tLayer, true) || tChanged;
            }
        }
        else
        {
            for (MapLayer tChild : PrimeMap::ChildrenOf(pLayer))
            {
                Layer* tLayer = FindLayer(pLayers, tChild);
                if (tLayer)
                {
                    tChanged = SetLayerOnly(*tLayer, false) || tChanged;
                }
            }
        }
        return tChanged;
    }
    Layer* tLayer = FindLayer(pLayers, pLayer);
    if (!tLayer)
    {
        return false;
    }
    if (tEntry->mKind == LayerKind::SubLayer)
    {
        return SetSubLayer(*tLayer, SubLayerIndex(pLayers, pLayer), pVisible);
    }
    return SetLayerOnly(*tLayer, pVisible);
}

bool Toggle(LayerManager& pLayers, const MapTheme& pTheme, MapLayer pLayer)
{
    const LayerEntry* tEntry = EntryOf(pLayer);
    if (!tEntry)
    {
        return false;
    }
    if (tEntry->mKind == LayerKind::Group)
    {
        // Varsayilan acik katmanlardan biri kapaliysa onlari ac; hepsi aciksa gruptaki her seyi kapat
        bool tTurnOn = false;
        for (Layer* tLayer : GroupDefaultLayers(pLayers, pTheme, pLayer))
        {
            tTurnOn = tTurnOn || !tLayer->IsVisible();
        }
        return SetVisible(pLayers, pTheme, pLayer, tTurnOn);
    }
    return SetVisible(pLayers, pTheme, pLayer, !IsVisible(pLayers, pLayer));
}

void ShowOnly(LayerManager& pLayers, const MapTheme& pTheme, const MapLayer* pBegin, const MapLayer* pEnd)
{
    for (int tIndex = 0; tIndex < pLayers.Count(); ++tIndex)
    {
        pLayers.At(tIndex)->SetVisible(false);
    }
    for (const MapLayer* tItem = pBegin; tItem != pEnd; ++tItem)
    {
        if (PrimeMap::KindOf(*tItem) == LayerKind::Layer)
        {
            // Acikca istenen katman: butun alt katmanlariyla
            Layer* tLayer = FindLayer(pLayers, *tItem);
            if (tLayer)
            {
                tLayer->SetVisible(true);
                for (int tSub = 0; tSub < tLayer->SubLayerCount(); ++tSub)
                {
                    tLayer->SetSubLayerVisible(tSub, true);
                }
            }
            continue;
        }
        SetVisible(pLayers, pTheme, *tItem, true);
    }
}

std::optional<MapLayer> GroupFromPanelName(std::string_view pName)
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

}  // namespace LayerRules
