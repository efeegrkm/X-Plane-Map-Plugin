#include "engine/MapInstance.h"

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <sstream>
#include <system_error>

#include "XPLMDisplay.h"

#include "Log.h"
#include "MapTheme.h"
#include "Settings.h"
#include "engine/LayerTable.h"
#include "engine/MapData.h"
#include "layers/AircraftLayer.h"
#include "layers/AirportLayer.h"
#include "layers/AirportObjectsLayer.h"
#include "layers/AirportSurfaceLayer.h"
#include "layers/AirwayLayer.h"
#include "layers/DrawingLayer.h"
#include "layers/FmsLayer.h"
#include "layers/GridLayer.h"
#include "layers/NavaidLayer.h"
#include "layers/RoadLayer.h"
#include "layers/RunwayLayer.h"
#include "layers/TaxiRouteLayer.h"
#include "layers/TerrainLayer.h"
#include "layers/TrackLayer.h"

namespace
{

const char* kSettingsFileName = "map_settings.ini";
const char* kDrawingsFileName = "map_drawings.txt";
const char* kTerrainFileName = "terrain_colors.ini";
const char* kWindowSection = "window";
const char* kSourcesSection = "sources";
const char* kLayerColorsSection = "layer_colors";
const int kMinStandaloneWidth = 400;   // Standalone pencerenin kullanilabilir en kucuk boyutu (boxel)
const int kMinStandaloneHeight = 260;

const char* kSettingsComment =
    "Prime XMap settings of ONE map (written by the library; safe to edit while X-Plane is closed).\n"
    "[window] left/top/right/bottom/visible: restored only for Standalone maps (the host places Embedded maps).\n"
    "[view] mode (scenery|vfr|ifr_low|ifr_high|dark), center_lat, center_lon, zoom, follow_aircraft,\n"
    "show_controls, mouse_input, min_zoom, max_zoom, zoom_step, pan_step.\n"
    "[render] cache (1: map image cache / FBO), max_fps (map image redraw rate). [text] renderer = atlas | xplane.\n"
    "[tools]: drawing palette. [layers.<mode>]: <layer or layer.sublayer> = 1/0, per map mode.\n"
    "[layer_zoom]: <layer or layer.sublayer> = <min zoom> <max zoom> (only limited layers are listed).\n"
    "[sources] aircraft / route = xplane | host.\n"
    "[layer_colors] <layer key> = <r g b> (0..255): API setColor overrides.";

bool readTextFile(const std::string& pPath, std::string* pText)
{
    std::ifstream tFile(pPath);
    if (!tFile)
    {
        return false;
    }
    std::ostringstream tText;
    tText << tFile.rdbuf();
    *pText = tText.str();
    return true;
}

bool writeTextFile(const std::string& pPath, const std::string& pText)
{
    // Once gecici dosyaya, sonra yeniden adlandir: yarim yazilmis dosya kalmasin
    const std::string tTemporary = pPath + ".tmp";
    {
        std::ofstream tFile(tTemporary, std::ios::trunc);
        if (!tFile)
        {
            return false;
        }
        tFile << pText;
        if (!tFile)
        {
            return false;
        }
    }
    std::error_code tError;
    std::filesystem::rename(tTemporary, pPath, tError);
    return !tError;
}

void screenBounds(int* pLeft, int* pTop, int* pRight, int* pBottom)
{
    XPLMGetScreenBoundsGlobal(pLeft, pTop, pRight, pBottom);
}

// Ana X-Plane penceresinin ortasinda, ekranin %70'i (en fazla 1000x700)
RectI defaultGeometry()
{
    int tLeft = 0;
    int tTop = 0;
    int tRight = 0;
    int tBottom = 0;
    screenBounds(&tLeft, &tTop, &tRight, &tBottom);
    const int tWidth = std::clamp((tRight - tLeft) * 7 / 10, kMinStandaloneWidth, 1000);
    const int tHeight = std::clamp((tTop - tBottom) * 7 / 10, kMinStandaloneHeight, 700);
    RectI tGeometry;
    tGeometry.mLeft = (tLeft + tRight) / 2 - tWidth / 2;
    tGeometry.mTop = (tTop + tBottom) / 2 + tHeight / 2;
    tGeometry.mRight = tGeometry.mLeft + tWidth;
    tGeometry.mBottom = tGeometry.mTop - tHeight;
    return tGeometry;
}

// Standalone pencere: boyut yeterli ve baslik cubugu ekranda mi (monitor sokulmus /
// cozunurluk degismis olabilir)
bool isStandaloneGeometryUsable(const RectI& pGeometry)
{
    if (pGeometry.width() < kMinStandaloneWidth || pGeometry.height() < kMinStandaloneHeight)
    {
        return false;
    }
    int tLeft = 0;
    int tTop = 0;
    int tRight = 0;
    int tBottom = 0;
    screenBounds(&tLeft, &tTop, &tRight, &tBottom);
    const int tTitleBar = 30;
    const bool tTitleVisible = pGeometry.mTop + tTitleBar <= tTop && pGeometry.mTop >= tBottom + 20;
    const bool tHorizontalVisible = pGeometry.mRight >= tLeft + 100 && pGeometry.mLeft <= tRight - 100;
    return tTitleVisible && tHorizontalVisible;
}

}  // namespace

XMapInstance::XMapInstance(XMapData& pData, int pId)
    : mData(pData)
    , mId(pId)
    , mWindow(mLayers, mAircraft, mDrawings, pData.airports())
{
    mAircraft.init();
    mPaletteId = mData.registerPalette(&mPalette);  // varsayilan zemin ayarlari (cogu zaman palet 0)
    createLayers();
    // XMapWindow, katmanlar eklenmeden kuruldu: modun varsayilan katman durumunu simdi uygula
    mLayers.resetToDefaults(themeFor(mWindow.getMapMode()));
    mData.addInstance(this);
}

XMapInstance::~XMapInstance()
{
    close();
    mData.removeInstance(this);
}

void XMapInstance::createLayers()
{
    // Alttan uste cizim sirasi (ayrinti: MIMARI.pdf bolum 7). Etiketler de bu sirayla
    // yer bulur: ustteki satirdaki katmanin etiketi onceliklidir (FMS ve
    // kullanici cizimleri etiketlerinin yerini onceden ayirir).
    // Veri depolari ortak (XMapData); katman nesneleri ve durumlari bu haritanin.
    AirportGeometryStore& tAirports = mData.airportGeometry();
    AirportDetailStore& tDetails = mData.airportDetails();
    std::unique_ptr<TerrainLayer> tTerrain = std::make_unique<TerrainLayer>(mData.hub(), mPalette, mPaletteId);
    mTerrainLayer = tTerrain.get();
    mLayers.add(std::move(tTerrain));  // en altta: zemin
    mLayers.add(std::make_unique<RoadLayer>(mData.hub()));  // DSF yollari: zeminin ustu, yuzeylerin alti
    mLayers.add(std::make_unique<AirportSurfaceLayer>(tAirports, tDetails));
    mLayers.add(std::make_unique<GridLayer>());
    // ILS tuyleri pistlerin ALTINDA: buyuk yari saydam alanlar pistleri ortmesin
    mLayers.add(std::make_unique<NavaidLayer>(NavaidType::Ils, mData.navs(), &tAirports));
    mLayers.add(std::make_unique<RunwayLayer>(tAirports));
    mLayers.add(std::make_unique<TaxiRouteLayer>(tAirports, tDetails));  // varsayilan kapali
    mLayers.add(std::make_unique<AirportObjectsLayer>(tAirports, tDetails));  // pistlerin ustunde
    // Hava yollari navaid sembollerinin altinda (cizgiler sembolleri ortmesin)
    mLayers.add(std::make_unique<AirwayLayer>(mData.airways(), false));
    mLayers.add(std::make_unique<AirwayLayer>(mData.airways(), true));
    mLayers.add(std::make_unique<AirportLayer>(mData.navs(), &tAirports));
    mLayers.add(std::make_unique<NavaidLayer>(NavaidType::Vor, mData.navs()));
    mLayers.add(std::make_unique<NavaidLayer>(NavaidType::Ndb, mData.navs()));
    mLayers.add(std::make_unique<NavaidLayer>(NavaidType::Fix, mData.navs()));
    std::unique_ptr<FmsLayer> tFms = std::make_unique<FmsLayer>();
    mFmsLayer = tFms.get();
    mLayers.add(std::move(tFms));
    std::unique_ptr<TrackLayer> tTrack = std::make_unique<TrackLayer>(mAircraft);
    mTrackLayer = tTrack.get();
    mLayers.add(std::move(tTrack));
    mLayers.add(std::make_unique<AircraftLayer>(mAircraft));
    mLayers.add(std::make_unique<DrawingLayer>(mDrawings));
}

int XMapInstance::id() const
{
    return mId;
}

const std::string& XMapInstance::name() const
{
    return mName;
}

void XMapInstance::setName(const std::string& pName)
{
    mName = pName;
}

std::string XMapInstance::safeFolderName(const std::string& pName)
{
    std::string tSafe;
    for (const char tChar : pName)
    {
        const bool tAllowed = (tChar >= 'a' && tChar <= 'z') || (tChar >= 'A' && tChar <= 'Z') ||
                              (tChar >= '0' && tChar <= '9') || tChar == '-' || tChar == '_' || tChar == '.';
        tSafe += tAllowed ? tChar : '_';
    }
    // "." / ".." ust klasore cikmasin; Windows sondaki noktayi siler
    while (!tSafe.empty() && tSafe.back() == '.')
    {
        tSafe.back() = '_';
    }
    return tSafe;
}

std::string XMapInstance::folderName() const
{
    const std::string tSafe = safeFolderName(mName);
    return tSafe.empty() ? std::format("map_{}", mId) : tSafe;
}

std::string XMapInstance::directory() const
{
    return mData.mapsDirectory() + folderName() + "/";
}

void XMapInstance::setPersistence(MapPersistence pPersistence)
{
    mPersistence = pPersistence;
}

MapPersistence XMapInstance::persistence() const
{
    return mPersistence;
}

bool XMapInstance::open()
{
    if (mWindow.isCreated())
    {
        return true;
    }
    if (mPersistence == MapPersistence::LoadAndSave)
    {
        loadFiles();
    }
    // Yer verilmemisse (ya da Standalone pencere ekran disinda kalacaksa) varsayilan yer
    const RectI tGeometry = mWindow.getGeometry();
    const bool tUsable = mWindow.isEmbedded() ? (tGeometry.width() > 0 && tGeometry.height() > 0)
                                              : isStandaloneGeometryUsable(tGeometry);
    if (!tUsable)
    {
        mWindow.setGeometry(defaultGeometry());
    }
    mFloatingGeometry = mWindow.getGeometry();
    mWindow.create();
    if (!mWindow.isCreated())
    {
        return false;
    }
    mData.openMapsChanged();  // ortak onbellek sinirlari acik harita sayisina gore buyur
    if (mPersistence != MapPersistence::Off)
    {
        // Dosyalar ilk acilista olussun (kullanici acip duzenleyebilsin)
        std::error_code tError;
        std::filesystem::create_directories(directory(), tError);
        const std::string tTerrainPath = directory() + kTerrainFileName;
        if (!std::filesystem::exists(tTerrainPath, tError) || !mPalette.loadedCurrentVersion())
        {
            writeTerrainFile();
        }
        writeSettingsFile();
    }
    mSavedSettings = mWindow.settingsRevision();
    mSavedDrawings = mWindow.drawingsRevision();
    mSavedState = stateSignature();
    mSavedExtra = mExtraRevision;
    log("map {} '{}' opened ({})", mId, folderName(),
        mPersistence == MapPersistence::Off ? std::string("no settings files") : "settings: " + directory());
    return true;
}

void XMapInstance::close()
{
    if (!mWindow.isCreated())
    {
        return;
    }
    if (!mWindow.isPoppedOut())
    {
        mFloatingGeometry = mWindow.getGeometry();
    }
    saveNow();
    mWindow.destroy();
    mData.openMapsChanged();
    log("map {} '{}' closed", mId, folderName());
}

bool XMapInstance::isOpen() const
{
    return mWindow.isCreated();
}

void XMapInstance::update(float pNow)
{
    mAircraft.refresh();
    mLayers.updateAll(pNow);

    // Alt seritteki yazi: ortak durum + BU haritanin zemin durumu. Ekranda istenen kalitede olmayan
    // (kaba yedek / bos) doku oldugu surece yaz; isci bosta ise bekleyen bir sey yoktur (uretilemeyen
    // parca): yazma, takili kalmasin.
    std::string tText = mData.sharedStatusText();
    std::string tTerrain = mData.terrainWorkerStatus();
    const int tPending = pendingTerrainImages();
    if (tPending > 0 && !mData.isTerrainIdle())
    {
        const std::string tLoading = std::format("Loading terrain: {} image{}", tPending, tPending == 1 ? "" : "s");
        tTerrain = tTerrain.empty() ? tLoading : tLoading + " (" + tTerrain + ")";
    }
    if (!tTerrain.empty())
    {
        tText += tText.empty() ? "" : "   ";
        tText += tTerrain;
    }
    mStatusText = tText;
    mWindow.setStatusMessage(mStatusText);
    mWindow.setDataBusy(mData.isBusy());

    // isDataLoaded: ortak veri hazir ve bu haritada bekleyen doku yok
    if (mDataLoadedFlag)
    {
        const MapDataStatus& tData = mData.status();
        *mDataLoadedFlag = tData.mNavReady && tData.mAirportsReady && tData.mAirwaysReady &&
                           tData.mTerrainIndexReady && tPending == 0;
    }

    if (!mWindow.isCreated())
    {
        return;
    }
    mWindow.update(pNow);
    if (!mWindow.isPoppedOut())
    {
        mFloatingGeometry = mWindow.getGeometry();
    }

    // Otomatik kayit: bir sey degistiyse, en erken autosave_seconds arayla
    const prime::MapManagerSettings& tLibrary = mData.managerSettings();
    if (mPersistence == MapPersistence::Off || !tLibrary.mAutoSave ||
        pNow - mLastSaveTime < static_cast<float>(tLibrary.mAutoSaveSeconds))
    {
        return;
    }
    if (mWindow.settingsRevision() != mSavedSettings || stateSignature() != mSavedState ||
        mExtraRevision != mSavedExtra)
    {
        mLastSaveTime = pNow;
        if (!writeSettingsFile())
        {
            log("WARNING: could not write {}{}", directory(), kSettingsFileName);
        }
    }
    if (mWindow.drawingsRevision() != mSavedDrawings)
    {
        mLastSaveTime = pNow;
        if (!writeDrawingsFile())
        {
            log("WARNING: could not write {}{}", directory(), kDrawingsFileName);
        }
    }
}

XMapWindow& XMapInstance::window()
{
    return mWindow;
}

const XMapWindow& XMapInstance::window() const
{
    return mWindow;
}

LayerManager& XMapInstance::layers()
{
    return mLayers;
}

Drawings& XMapInstance::userDrawings()
{
    return mDrawings;
}

AircraftData& XMapInstance::aircraft()
{
    return mAircraft;
}

FmsLayer* XMapInstance::fms()
{
    return mFmsLayer;
}

TrackLayer* XMapInstance::track()
{
    return mTrackLayer;
}

XMapData& XMapInstance::data()
{
    return mData;
}

RectI XMapInstance::savedGeometry() const
{
    return mWindow.isCreated() && !mWindow.isPoppedOut() ? mWindow.getGeometry() : mFloatingGeometry;
}

uint64_t XMapInstance::stateSignature() const
{
    // Kullanicinin fareyle yaptigi, revizyon sayacini artirmayan degisiklik: pencerenin yeri / boyutu.
    // gorunum(merkez, zoom) bilerek YOK: ucak takibinde her karede degisir, dosya surekli yazilirdi.
    // Gorunum, baska bir ayar degistiginde ve harita kapanirken yazilir.
    uint64_t tHash = 1469598103934665603ull;
    auto tMix = [&tHash](uint64_t pValue)
    {
        tHash = (tHash ^ pValue) * 1099511628211ull;
    };
    const RectI tGeometry = savedGeometry();
    tMix(static_cast<uint64_t>(static_cast<int64_t>(tGeometry.mLeft)));
    tMix(static_cast<uint64_t>(static_cast<int64_t>(tGeometry.mTop)));
    tMix(static_cast<uint64_t>(static_cast<int64_t>(tGeometry.mRight)));
    tMix(static_cast<uint64_t>(static_cast<int64_t>(tGeometry.mBottom)));
    return tHash;
}

void XMapInstance::storeExtraSections()
{
    Settings& tStore = mWindow.settingsStore();
    const RectI tGeometry = savedGeometry();
    tStore.setInt(kWindowSection, "left", tGeometry.mLeft);
    tStore.setInt(kWindowSection, "top", tGeometry.mTop);
    tStore.setInt(kWindowSection, "right", tGeometry.mRight);
    tStore.setInt(kWindowSection, "bottom", tGeometry.mBottom);
    tStore.setBool(kWindowSection, "visible", mWindow.isCreated() ? mWindow.isVisible() : mWindow.wantsVisible());
    tStore.setString(kSourcesSection, "aircraft", mAircraft.isFromHost() ? "host" : "xplane");
    tStore.setString(kSourcesSection, "route", mFmsLayer && mFmsLayer->isFromHost() ? "host" : "xplane");

    // [layer_colors]: API setColor ile degistirilen katman renkleri (tema uzerine yazmalar)
    tStore.clearSection(kLayerColorsSection);
    for (const auto& [tLayer, tColor] : mWindow.layerColors())
    {
        const auto tChannel = [](float pValue)
        {
            return std::clamp(static_cast<int>(pValue * 255.0f + 0.5f), 0, 255);
        };
        tStore.setString(kLayerColorsSection, LayerRules::layerKey(tLayer),
                         std::format("{} {} {}", tChannel(tColor.mR), tChannel(tColor.mG), tChannel(tColor.mB)));
    }
}

void XMapInstance::applyWindowSection()
{
    // Sadece Standalone: kullanici pencereyi tasir / kapatir, bir sonraki acilista ayni yerde bulur.
    // Embedded haritanin yerini ve gorunurlugunu host belirler (kendi arayuzunun bir parcasi).
    if (mWindow.isEmbedded())
    {
        return;
    }
    const Settings& tStore = mWindow.settingsStore();
    if (tStore.has(kWindowSection, "left") && tStore.has(kWindowSection, "right"))
    {
        RectI tGeometry;
        tGeometry.mLeft = tStore.getInt(kWindowSection, "left", 0);
        tGeometry.mTop = tStore.getInt(kWindowSection, "top", 0);
        tGeometry.mRight = tStore.getInt(kWindowSection, "right", 0);
        tGeometry.mBottom = tStore.getInt(kWindowSection, "bottom", 0);
        if (isStandaloneGeometryUsable(tGeometry))
        {
            mWindow.setGeometry(tGeometry);
            mFloatingGeometry = tGeometry;
        }
    }
    if (tStore.has(kWindowSection, "visible"))
    {
        const bool tVisible = tStore.getBool(kWindowSection, "visible", true);
        if (tVisible != (mWindow.isCreated() ? mWindow.isVisible() : mWindow.wantsVisible()))
        {
            mWindow.setVisible(tVisible);
        }
    }
}

void XMapInstance::applySourcesSection()
{
    const Settings& tStore = mWindow.settingsStore();
    if (tStore.has(kSourcesSection, "aircraft"))
    {
        mAircraft.setFromHost(tStore.getString(kSourcesSection, "aircraft", "xplane") == "host");
    }
    if (tStore.has(kSourcesSection, "route") && mFmsLayer)
    {
        mFmsLayer->setFromHost(tStore.getString(kSourcesSection, "route", "xplane") == "host");
    }
}

std::string XMapInstance::exportSettings()
{
    storeExtraSections();
    mWindow.saveSettings();
    return mWindow.settingsStore().toText({kWindowSection, "view", "render", "text", "tools", "layers*",
                                           "layer_zoom", kSourcesSection, kLayerColorsSection});
}

void XMapInstance::importSettings(const std::string& pText)
{
    mWindow.importSettings(pText);
    // [window] / [sources] sadece metinde varsa uygulanir (importSettings bellekteki ayarlara ekledi)
    Settings tGiven;
    tGiven.loadText(pText);
    if (tGiven.has(kWindowSection, "left") || tGiven.has(kWindowSection, "visible"))
    {
        applyWindowSection();
    }
    applySourcesSection();
    applyLayerColorsSection(tGiven);
}

void XMapInstance::applyLayerColorsSection(const Settings& pStore)
{
    const auto* tSection = pStore.findSection(kLayerColorsSection);
    if (!tSection)
    {
        return;
    }
    std::map<prime::MapLayer, Rgba> tColors;
    for (const auto& [tKey, tValue] : *tSection)
    {
        const std::optional<prime::MapLayer> tLayer = LayerRules::layerFromKey(tKey);
        std::istringstream tStream(tValue);
        int tR = 0;
        int tG = 0;
        int tB = 0;
        if (!(tStream >> tR >> tG >> tB))
        {
            log("WARNING: map {} '{}': bad [layer_colors] entry '{}'", mId, folderName(), tKey);
            continue;
        }
        if (!tLayer)
        {
            log("WARNING: map {} '{}': unknown [layer_colors] key '{}'", mId, folderName(), tKey);
            continue;
        }
        const auto tNorm = [](int pChannel)
        {
            return std::clamp(pChannel, 0, 255) / 255.0f;
        };
        tColors[*tLayer] = Rgba{tNorm(tR), tNorm(tG), tNorm(tB), 1.0f};
    }
    mWindow.setLayerColors(tColors);
}

void XMapInstance::importTerrainSettings(const std::string& pText)
{
    Settings tColors;
    tColors.loadText(pText);
    TerrainPalette tPalette;
    tPalette.load(tColors);
    mPalette = tPalette;
    mPaletteId = mData.registerPalette(&mPalette);  // X-Plane renkleri uygulanir; ayni ayar = ayni numara
    if (mTerrainLayer)
    {
        mTerrainLayer->setPalette(mPalette, mPaletteId);  // yeni dokular yeni paletle istenir (hemen)
    }
    mWindow.invalidateImage();  // onbellekteki harita resmi eski renklerle: hemen yeniden ciz
    markChanged();
    if (mWindow.isCreated() && mPersistence != MapPersistence::Off)
    {
        writeTerrainFile();
    }
}

std::string XMapInstance::exportTerrainSettings() const
{
    Settings tColors;
    mPalette.save(tColors);
    tColors.setComment(XMapData::terrainSettingsComment());
    return tColors.toText({});
}

const TerrainPalette& XMapInstance::palette() const
{
    return mPalette;
}

void XMapInstance::markChanged()
{
    ++mExtraRevision;
}

void XMapInstance::loadFiles()
{
    const std::string tDirectory = directory();
    std::string tText;
    if (readTextFile(tDirectory + kTerrainFileName, &tText))
    {
        importTerrainSettings(tText);
    }
    if (readTextFile(tDirectory + kSettingsFileName, &tText))
    {
        importSettings(tText);
        log("map {} '{}': settings loaded from {}", mId, folderName(), tDirectory + kSettingsFileName);
    }
    else
    {
        log("map {} '{}': no settings file yet, using the values given through the API", mId, folderName());
    }
    if (readTextFile(tDirectory + kDrawingsFileName, &tText))
    {
        mDrawings.deserialize(tText);
        mWindow.drawingsChanged();
    }
}

bool XMapInstance::writeSettingsFile()
{
    std::error_code tError;
    std::filesystem::create_directories(directory(), tError);
    Settings tFile;
    tFile.loadText(exportSettings());
    tFile.setComment(kSettingsComment);
    const bool tOk = writeTextFile(directory() + kSettingsFileName, tFile.toText({}));
    mSavedSettings = mWindow.settingsRevision();  // exportSettings sayaci artirir: sonra oku
    mSavedState = stateSignature();
    mSavedExtra = mExtraRevision;
    return tOk;
}

bool XMapInstance::writeDrawingsFile()
{
    std::error_code tError;
    std::filesystem::create_directories(directory(), tError);
    const bool tOk = writeTextFile(directory() + kDrawingsFileName, mDrawings.serialize());
    mSavedDrawings = mWindow.drawingsRevision();
    return tOk;
}

bool XMapInstance::writeTerrainFile()
{
    std::error_code tError;
    std::filesystem::create_directories(directory(), tError);
    return writeTextFile(directory() + kTerrainFileName, exportTerrainSettings());
}

bool XMapInstance::saveNow()
{
    if (mPersistence == MapPersistence::Off)
    {
        return false;
    }
    bool tOk = writeSettingsFile();
    tOk = writeDrawingsFile() && tOk;
    tOk = writeTerrainFile() && tOk;
    if (!tOk)
    {
        log("WARNING: map {} '{}': could not write the settings files in {}", mId, folderName(), directory());
    }
    return tOk;
}

int XMapInstance::pendingTerrainImages() const
{
    return (mTerrainLayer && mTerrainLayer->isVisible() && mWindow.isVisible())
               ? mTerrainLayer->pendingTextureCount()
               : 0;
}

const std::string& XMapInstance::statusText() const
{
    return mStatusText;
}

bool XMapInstance::isDiagnosticsOn() const
{
    return mWindow.isDiagnosticsOn();
}

void XMapInstance::setDataLoadedFlag(std::atomic<bool>* pFlag)
{
    mDataLoadedFlag = pFlag;
}
