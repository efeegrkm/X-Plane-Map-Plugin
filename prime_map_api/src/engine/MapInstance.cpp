// MapInstance.cpp
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
const int kMinStandaloneWidth = 400;   // Standalone pencerenin kullanilabilir en kucuk boyutu (boxel)
const int kMinStandaloneHeight = 260;

const char* kSettingsComment =
    "Prime Map settings of ONE map (written by the library; safe to edit while X-Plane is closed).\n"
    "[window] left/top/right/bottom/visible: restored only for Standalone maps (the host places Embedded maps).\n"
    "[view] mode (scenery|vfr|ifr_low|ifr_high|dark), center_lat, center_lon, zoom, follow_aircraft,\n"
    "show_controls, mouse_input, min_zoom, max_zoom, zoom_step, pan_step.\n"
    "[render] cache (1: map image cache / FBO), max_fps (map image redraw rate). [text] renderer = atlas | xplane.\n"
    "[tools]: drawing palette. [layers.<mode>]: <layer or layer.sublayer> = 1/0, per map mode.\n"
    "[layer_zoom]: <layer or layer.sublayer> = <min zoom> <max zoom> (only limited layers are listed).\n"
    "[sources] aircraft / route = xplane | host.";

bool ReadTextFile(const std::string& pPath, std::string* pText)
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

bool WriteTextFile(const std::string& pPath, const std::string& pText)
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

void ScreenBounds(int* pLeft, int* pTop, int* pRight, int* pBottom)
{
    XPLMGetScreenBoundsGlobal(pLeft, pTop, pRight, pBottom);
}

// Ana X-Plane penceresinin ortasinda, ekranin %70'i (en fazla 1000x700)
RectI DefaultGeometry()
{
    int tLeft = 0;
    int tTop = 0;
    int tRight = 0;
    int tBottom = 0;
    ScreenBounds(&tLeft, &tTop, &tRight, &tBottom);
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
bool IsStandaloneGeometryUsable(const RectI& pGeometry)
{
    if (pGeometry.Width() < kMinStandaloneWidth || pGeometry.Height() < kMinStandaloneHeight)
    {
        return false;
    }
    int tLeft = 0;
    int tTop = 0;
    int tRight = 0;
    int tBottom = 0;
    ScreenBounds(&tLeft, &tTop, &tRight, &tBottom);
    const int tTitleBar = 30;
    const bool tTitleVisible = pGeometry.mTop + tTitleBar <= tTop && pGeometry.mTop >= tBottom + 20;
    const bool tHorizontalVisible = pGeometry.mRight >= tLeft + 100 && pGeometry.mLeft <= tRight - 100;
    return tTitleVisible && tHorizontalVisible;
}

}  // namespace

MapInstance::MapInstance(MapData& pData, int pId)
    : mData(pData)
    , mId(pId)
    , mWindow(mLayers, mAircraft, mDrawings, pData.Airports())
{
    mAircraft.Init();
    mPaletteId = mData.RegisterPalette(&mPalette);  // varsayilan zemin ayarlari (cogu zaman palet 0)
    CreateLayers();
    // MapWindow, katmanlar eklenmeden kuruldu: modun varsayilan katman durumunu simdi uygula
    mLayers.ResetToDefaults(ThemeFor(mWindow.GetMapMode()));
    mData.AddInstance(this);
}

MapInstance::~MapInstance()
{
    Close();
    mData.RemoveInstance(this);
}

void MapInstance::CreateLayers()
{
    // Alttan uste cizim sirasi (ayrinti: MIMARI.pdf bolum 7). Etiketler de bu sirayla
    // yer bulur: ustteki satirdaki katmanin etiketi onceliklidir (FMS ve
    // kullanici cizimleri etiketlerinin yerini onceden ayirir).
    // Veri depolari ortak (MapData); katman nesneleri ve durumlari bu haritanin.
    AirportGeometryStore& tAirports = mData.AirportGeometry();
    AirportDetailStore& tDetails = mData.AirportDetails();
    std::unique_ptr<TerrainLayer> tTerrain = std::make_unique<TerrainLayer>(mData.Hub(), mPalette, mPaletteId);
    mTerrainLayer = tTerrain.get();
    mLayers.Add(std::move(tTerrain));  // en altta: zemin
    mLayers.Add(std::make_unique<RoadLayer>(mData.Hub()));  // DSF yollari: zeminin ustu, yuzeylerin alti
    mLayers.Add(std::make_unique<AirportSurfaceLayer>(tAirports, tDetails));
    mLayers.Add(std::make_unique<GridLayer>());
    // ILS tuyleri pistlerin ALTINDA: buyuk yari saydam alanlar pistleri ortmesin
    mLayers.Add(std::make_unique<NavaidLayer>(NavaidType::Ils, mData.Navs(), &tAirports));
    mLayers.Add(std::make_unique<RunwayLayer>(tAirports));
    mLayers.Add(std::make_unique<TaxiRouteLayer>(tAirports, tDetails));  // varsayilan kapali
    mLayers.Add(std::make_unique<AirportObjectsLayer>(tAirports, tDetails));  // pistlerin ustunde
    // Hava yollari navaid sembollerinin altinda (cizgiler sembolleri ortmesin)
    mLayers.Add(std::make_unique<AirwayLayer>(mData.Airways(), false));
    mLayers.Add(std::make_unique<AirwayLayer>(mData.Airways(), true));
    mLayers.Add(std::make_unique<AirportLayer>(mData.Navs(), &tAirports));
    mLayers.Add(std::make_unique<NavaidLayer>(NavaidType::Vor, mData.Navs()));
    mLayers.Add(std::make_unique<NavaidLayer>(NavaidType::Ndb, mData.Navs()));
    mLayers.Add(std::make_unique<NavaidLayer>(NavaidType::Fix, mData.Navs()));
    std::unique_ptr<FmsLayer> tFms = std::make_unique<FmsLayer>();
    mFmsLayer = tFms.get();
    mLayers.Add(std::move(tFms));
    std::unique_ptr<TrackLayer> tTrack = std::make_unique<TrackLayer>(mAircraft);
    mTrackLayer = tTrack.get();
    mLayers.Add(std::move(tTrack));
    mLayers.Add(std::make_unique<AircraftLayer>(mAircraft));
    mLayers.Add(std::make_unique<DrawingLayer>(mDrawings));
}

// --- Kimlik ve dosyalar --------------------------------------------------------------

int MapInstance::Id() const
{
    return mId;
}

const std::string& MapInstance::Name() const
{
    return mName;
}

void MapInstance::SetName(const std::string& pName)
{
    mName = pName;
}

std::string MapInstance::SafeFolderName(const std::string& pName)
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

std::string MapInstance::FolderName() const
{
    const std::string tSafe = SafeFolderName(mName);
    return tSafe.empty() ? std::format("map_{}", mId) : tSafe;
}

std::string MapInstance::Directory() const
{
    return mData.MapsDirectory() + FolderName() + "/";
}

void MapInstance::SetPersistence(MapPersistence pPersistence)
{
    mPersistence = pPersistence;
}

MapPersistence MapInstance::Persistence() const
{
    return mPersistence;
}

// --- Acma / kapama -------------------------------------------------------------------

bool MapInstance::Open()
{
    if (mWindow.IsCreated())
    {
        return true;
    }
    if (mPersistence == MapPersistence::LoadAndSave)
    {
        LoadFiles();
    }
    // Yer verilmemisse (ya da Standalone pencere ekran disinda kalacaksa) varsayilan yer
    const RectI tGeometry = mWindow.GetGeometry();
    const bool tUsable = mWindow.IsEmbedded() ? (tGeometry.Width() > 0 && tGeometry.Height() > 0)
                                              : IsStandaloneGeometryUsable(tGeometry);
    if (!tUsable)
    {
        mWindow.SetGeometry(DefaultGeometry());
    }
    mFloatingGeometry = mWindow.GetGeometry();
    mWindow.Create();
    if (!mWindow.IsCreated())
    {
        return false;
    }
    mData.OpenMapsChanged();  // ortak onbellek sinirlari acik harita sayisina gore buyur
    if (mPersistence != MapPersistence::Off)
    {
        // Dosyalar ilk acilista olussun (kullanici acip duzenleyebilsin)
        std::error_code tError;
        std::filesystem::create_directories(Directory(), tError);
        const std::string tTerrainPath = Directory() + kTerrainFileName;
        if (!std::filesystem::exists(tTerrainPath, tError) || !mPalette.LoadedCurrentVersion())
        {
            WriteTerrainFile();
        }
        WriteSettingsFile();
    }
    mSavedSettings = mWindow.SettingsRevision();
    mSavedDrawings = mWindow.DrawingsRevision();
    mSavedState = StateSignature();
    mSavedExtra = mExtraRevision;
    Log("map {} '{}' opened ({})", mId, FolderName(),
        mPersistence == MapPersistence::Off ? std::string("no settings files") : "settings: " + Directory());
    return true;
}

void MapInstance::Close()
{
    if (!mWindow.IsCreated())
    {
        return;
    }
    if (!mWindow.IsPoppedOut())
    {
        mFloatingGeometry = mWindow.GetGeometry();
    }
    SaveNow();
    mWindow.Destroy();
    mData.OpenMapsChanged();
    Log("map {} '{}' closed", mId, FolderName());
}

bool MapInstance::IsOpen() const
{
    return mWindow.IsCreated();
}

// --- Her kare ------------------------------------------------------------------------

void MapInstance::Update(float pNow)
{
    mAircraft.Refresh();
    mLayers.UpdateAll(pNow);

    // Alt seritteki yazi: ortak durum + BU haritanin zemin durumu. Ekranda istenen kalitede olmayan
    // (kaba yedek / bos) doku oldugu surece yaz; isci bosta ise bekleyen bir sey yoktur (uretilemeyen
    // parca): yazma, takili kalmasin.
    std::string tText = mData.SharedStatusText();
    std::string tTerrain = mData.TerrainWorkerStatus();
    const int tPending = PendingTerrainImages();
    if (tPending > 0 && !mData.IsTerrainIdle())
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
    mWindow.SetStatusMessage(mStatusText);
    mWindow.SetDataBusy(mData.IsBusy());

    if (!mWindow.IsCreated())
    {
        return;
    }
    mWindow.Update(pNow);
    if (!mWindow.IsPoppedOut())
    {
        mFloatingGeometry = mWindow.GetGeometry();
    }

    // Otomatik kayit: bir sey degistiyse, en erken autosave_seconds arayla
    const prime::LibrarySettings& tLibrary = mData.LibrarySettings();
    if (mPersistence == MapPersistence::Off || !tLibrary.mAutoSave ||
        pNow - mLastSaveTime < static_cast<float>(tLibrary.mAutoSaveSeconds))
    {
        return;
    }
    if (mWindow.SettingsRevision() != mSavedSettings || StateSignature() != mSavedState ||
        mExtraRevision != mSavedExtra)
    {
        mLastSaveTime = pNow;
        if (!WriteSettingsFile())
        {
            Log("WARNING: could not write {}{}", Directory(), kSettingsFileName);
        }
    }
    if (mWindow.DrawingsRevision() != mSavedDrawings)
    {
        mLastSaveTime = pNow;
        if (!WriteDrawingsFile())
        {
            Log("WARNING: could not write {}{}", Directory(), kDrawingsFileName);
        }
    }
}

// --- Parcalar ------------------------------------------------------------------------

MapWindow& MapInstance::Window()
{
    return mWindow;
}

const MapWindow& MapInstance::Window() const
{
    return mWindow;
}

LayerManager& MapInstance::Layers()
{
    return mLayers;
}

Drawings& MapInstance::UserDrawings()
{
    return mDrawings;
}

AircraftData& MapInstance::Aircraft()
{
    return mAircraft;
}

FmsLayer* MapInstance::Fms()
{
    return mFmsLayer;
}

TrackLayer* MapInstance::Track()
{
    return mTrackLayer;
}

MapData& MapInstance::Data()
{
    return mData;
}

// --- Ayarlar -------------------------------------------------------------------------

RectI MapInstance::SavedGeometry() const
{
    return mWindow.IsCreated() && !mWindow.IsPoppedOut() ? mWindow.GetGeometry() : mFloatingGeometry;
}

uint64_t MapInstance::StateSignature() const
{
    // Kullanicinin fareyle yaptigi, revizyon sayacini artirmayan degisiklik: pencerenin yeri / boyutu.
    // Gorunum (merkez, zoom) bilerek YOK: ucak takibinde her karede degisir, dosya surekli yazilirdi.
    // Gorunum, baska bir ayar degistiginde ve harita kapanirken yazilir.
    uint64_t tHash = 1469598103934665603ull;
    auto tMix = [&tHash](uint64_t pValue)
    {
        tHash = (tHash ^ pValue) * 1099511628211ull;
    };
    const RectI tGeometry = SavedGeometry();
    tMix(static_cast<uint64_t>(static_cast<int64_t>(tGeometry.mLeft)));
    tMix(static_cast<uint64_t>(static_cast<int64_t>(tGeometry.mTop)));
    tMix(static_cast<uint64_t>(static_cast<int64_t>(tGeometry.mRight)));
    tMix(static_cast<uint64_t>(static_cast<int64_t>(tGeometry.mBottom)));
    return tHash;
}

void MapInstance::StoreExtraSections()
{
    Settings& tStore = mWindow.SettingsStore();
    const RectI tGeometry = SavedGeometry();
    tStore.SetInt(kWindowSection, "left", tGeometry.mLeft);
    tStore.SetInt(kWindowSection, "top", tGeometry.mTop);
    tStore.SetInt(kWindowSection, "right", tGeometry.mRight);
    tStore.SetInt(kWindowSection, "bottom", tGeometry.mBottom);
    tStore.SetBool(kWindowSection, "visible", mWindow.IsCreated() ? mWindow.IsVisible() : mWindow.WantsVisible());
    tStore.SetString(kSourcesSection, "aircraft", mAircraft.IsFromHost() ? "host" : "xplane");
    tStore.SetString(kSourcesSection, "route", mFmsLayer && mFmsLayer->IsFromHost() ? "host" : "xplane");
}

void MapInstance::ApplyWindowSection()
{
    // Sadece Standalone: kullanici pencereyi tasir / kapatir, bir sonraki acilista ayni yerde bulur.
    // Embedded haritanin yerini ve gorunurlugunu host belirler (kendi arayuzunun bir parcasi).
    if (mWindow.IsEmbedded())
    {
        return;
    }
    const Settings& tStore = mWindow.SettingsStore();
    if (tStore.Has(kWindowSection, "left") && tStore.Has(kWindowSection, "right"))
    {
        RectI tGeometry;
        tGeometry.mLeft = tStore.GetInt(kWindowSection, "left", 0);
        tGeometry.mTop = tStore.GetInt(kWindowSection, "top", 0);
        tGeometry.mRight = tStore.GetInt(kWindowSection, "right", 0);
        tGeometry.mBottom = tStore.GetInt(kWindowSection, "bottom", 0);
        if (IsStandaloneGeometryUsable(tGeometry))
        {
            mWindow.SetGeometry(tGeometry);
            mFloatingGeometry = tGeometry;
        }
    }
    if (tStore.Has(kWindowSection, "visible"))
    {
        const bool tVisible = tStore.GetBool(kWindowSection, "visible", true);
        if (tVisible != (mWindow.IsCreated() ? mWindow.IsVisible() : mWindow.WantsVisible()))
        {
            mWindow.SetVisible(tVisible);
        }
    }
}

void MapInstance::ApplySourcesSection()
{
    const Settings& tStore = mWindow.SettingsStore();
    if (tStore.Has(kSourcesSection, "aircraft"))
    {
        mAircraft.SetFromHost(tStore.GetString(kSourcesSection, "aircraft", "xplane") == "host");
    }
    if (tStore.Has(kSourcesSection, "route") && mFmsLayer)
    {
        mFmsLayer->SetFromHost(tStore.GetString(kSourcesSection, "route", "xplane") == "host");
    }
}

std::string MapInstance::ExportSettings()
{
    StoreExtraSections();
    mWindow.SaveSettings();  // gorunum, mod, katmanlar ... bellekteki ayarlara
    return mWindow.SettingsStore().ToText({kWindowSection, "view", "render", "text", "tools", "layers*",
                                           "layer_zoom", kSourcesSection});
}

void MapInstance::ImportSettings(const std::string& pText)
{
    mWindow.ImportSettings(pText);
    // [window] / [sources] sadece metinde varsa uygulanir (ImportSettings bellekteki ayarlara ekledi)
    Settings tGiven;
    tGiven.LoadText(pText);
    if (tGiven.Has(kWindowSection, "left") || tGiven.Has(kWindowSection, "visible"))
    {
        ApplyWindowSection();
    }
    ApplySourcesSection();
}

void MapInstance::ImportTerrainSettings(const std::string& pText)
{
    Settings tColors;
    tColors.LoadText(pText);
    TerrainPalette tPalette;
    tPalette.Load(tColors);
    mPalette = tPalette;
    mPaletteId = mData.RegisterPalette(&mPalette);  // X-Plane renkleri uygulanir; ayni ayar = ayni numara
    if (mTerrainLayer)
    {
        mTerrainLayer->SetPalette(mPalette, mPaletteId);  // yeni dokular yeni paletle istenir (hemen)
    }
    mWindow.InvalidateImage();  // onbellekteki harita resmi eski renklerle: hemen yeniden ciz
    MarkChanged();
    if (mWindow.IsCreated() && mPersistence != MapPersistence::Off)
    {
        WriteTerrainFile();
    }
}

std::string MapInstance::ExportTerrainSettings() const
{
    Settings tColors;
    mPalette.Save(tColors);
    tColors.SetComment(MapData::TerrainSettingsComment());
    return tColors.ToText({});
}

const TerrainPalette& MapInstance::Palette() const
{
    return mPalette;
}

void MapInstance::MarkChanged()
{
    ++mExtraRevision;
}

void MapInstance::LoadFiles()
{
    const std::string tDirectory = Directory();
    std::string tText;
    if (ReadTextFile(tDirectory + kTerrainFileName, &tText))
    {
        ImportTerrainSettings(tText);
    }
    if (ReadTextFile(tDirectory + kSettingsFileName, &tText))
    {
        ImportSettings(tText);
        Log("map {} '{}': settings loaded from {}", mId, FolderName(), tDirectory + kSettingsFileName);
    }
    else
    {
        Log("map {} '{}': no settings file yet, using the values given through the API", mId, FolderName());
    }
    if (ReadTextFile(tDirectory + kDrawingsFileName, &tText))
    {
        mDrawings.Deserialize(tText);
        mWindow.DrawingsChanged();
    }
}

bool MapInstance::WriteSettingsFile()
{
    std::error_code tError;
    std::filesystem::create_directories(Directory(), tError);
    Settings tFile;
    tFile.LoadText(ExportSettings());
    tFile.SetComment(kSettingsComment);
    const bool tOk = WriteTextFile(Directory() + kSettingsFileName, tFile.ToText({}));
    mSavedSettings = mWindow.SettingsRevision();  // ExportSettings sayaci artirir: sonra oku
    mSavedState = StateSignature();
    mSavedExtra = mExtraRevision;
    return tOk;
}

bool MapInstance::WriteDrawingsFile()
{
    std::error_code tError;
    std::filesystem::create_directories(Directory(), tError);
    const bool tOk = WriteTextFile(Directory() + kDrawingsFileName, mDrawings.Serialize());
    mSavedDrawings = mWindow.DrawingsRevision();
    return tOk;
}

bool MapInstance::WriteTerrainFile()
{
    std::error_code tError;
    std::filesystem::create_directories(Directory(), tError);
    return WriteTextFile(Directory() + kTerrainFileName, ExportTerrainSettings());
}

bool MapInstance::SaveNow()
{
    if (mPersistence == MapPersistence::Off)
    {
        return false;
    }
    bool tOk = WriteSettingsFile();
    tOk = WriteDrawingsFile() && tOk;
    tOk = WriteTerrainFile() && tOk;
    if (!tOk)
    {
        Log("WARNING: map {} '{}': could not write the settings files in {}", mId, FolderName(), Directory());
    }
    return tOk;
}

// --- Durum ---------------------------------------------------------------------------

int MapInstance::PendingTerrainImages() const
{
    return (mTerrainLayer && mTerrainLayer->IsVisible() && mWindow.IsVisible())
               ? mTerrainLayer->PendingTextureCount()
               : 0;
}

const std::string& MapInstance::StatusText() const
{
    return mStatusText;
}

bool MapInstance::IsDiagnosticsOn() const
{
    return mWindow.IsDiagnosticsOn();
}
