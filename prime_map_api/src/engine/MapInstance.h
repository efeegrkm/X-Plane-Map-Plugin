// TEK bir haritanin her seyi: API'deki prime::XMap nesnesinin ic karsiligi. Ortak veri
// (nav, havalimanlari, zemin iscisi, onbellek) XMapData'dadir; katmanlar oraya referansla bakar.
#pragma once

#include <atomic>
#include <cstdint>
#include <string>

#include "Drawings.h"
#include "LayerManager.h"
#include "MapWindow.h"
#include "data/AircraftData.h"
#include "data/TerrainClasses.h"

class FmsLayer;
class XMapData;
class TerrainLayer;
class TrackLayer;

enum class MapPersistence
{
    Off,          // dosya okunmaz / yazilmaz
    SaveOnly,     // okunmaz (kodla verilen ayar her acilista gecerli), yazilir
    LoadAndSave,  // Open'da okunur, degisince yazilir (varsayilan)
};

class XMapInstance
{
public:
    XMapInstance(XMapData& pData, int pId);
    ~XMapInstance();

    XMapInstance(const XMapInstance&) = delete;
    XMapInstance& operator=(const XMapInstance&) = delete;

    // --- Kimlik ve dosyalar ---
    int id() const;
    const std::string& name() const;           // bos: adsiz (klasor map_<kimlik>)
    void setName(const std::string& pName);    // cagiran (API) acik haritada / cakismada reddeder
    std::string folderName() const;            // "ios_left" ya da "map_3" (dosya adina uygun hale getirilmis)
    std::string directory() const;             // <veri>/maps/<FolderName>/
    static std::string safeFolderName(const std::string& pName);  // harf, rakam, - _ . disi '_' olur
    void setPersistence(MapPersistence pPersistence);
    MapPersistence persistence() const;

    // --- Acma / kapama ---
    bool open();   // dosyalari okur (LoadAndSave), pencereyi acar. Zaten aciksa true.
    void close();  // dosyalari yazar, pencereyi kapatir (nesne kalir)
    bool isOpen() const;

    // Flight loop'tan her karede (XMapData): ucak, katman verisi, durum yazisi, otomatik kayit
    void update(float pNow);

    // --- Parcalar ---
    XMapWindow& window();
    const XMapWindow& window() const;
    LayerManager& layers();
    Drawings& userDrawings();
    AircraftData& aircraft();
    FmsLayer* fms();
    TrackLayer* track();
    XMapData& data();

    // --- Ayarlar ---
    // Haritanin butun ayarlari tek metin (map_settings.ini bicimi)
    std::string exportSettings();
    void importSettings(const std::string& pText);
    // Zemin ayarlari (terrain_colors.ini bicimi): hemen uygulanir (yeni dokular yeni paletle istenir)
    void importTerrainSettings(const std::string& pText);
    std::string exportTerrainSettings() const;
    const TerrainPalette& palette() const;
    // Dosyalari simdi yazar (Persistence::Off ise false)
    bool saveNow();
    // Kaynaklar degisti (ucak / rota: API cagirir): ayar dosyasina yazilsin
    void markChanged();

    // --- Durum ---
    int pendingTerrainImages() const;      // bu haritada bekleyen zemin dokusu (gorunmuyorsa 0)
    const std::string& statusText() const; // alt seritteki yazi (ortak durum + bu haritanin zemini)
    bool isDiagnosticsOn() const;
    // isDataLoaded() icin XMap'in bayragi; update() her karede gunceller
    void setDataLoadedFlag(std::atomic<bool>* pFlag);

private:
    void createLayers();
    void loadFiles();                 // Open: terrain_colors.ini, map_settings.ini, map_drawings.txt
    bool writeSettingsFile();
    bool writeDrawingsFile();
    bool writeTerrainFile();
    void applyWindowSection();        // [window]: Standalone pencerenin yeri / gorunurlugu (dosyadan)
    void applySourcesSection();
    void applyLayerColorsSection(const Settings& pStore);
    void storeExtraSections();        // [window], [sources] -> pencerenin bellekteki ayarlarina
    RectI savedGeometry() const;      // kaydedilecek yer (pop-out'ta X-Plane icindeki son yer)
    uint64_t stateSignature() const;  // revizyon sayaclarinin gormedigi degisiklikler (yer, gorunurluk)

    XMapData& mData;
    int mId = 0;
    std::string mName;
    MapPersistence mPersistence = MapPersistence::LoadAndSave;

    AircraftData mAircraft;
    Drawings mDrawings;
    TerrainPalette mPalette;       // bu haritanin zemin ayarlari (X-Plane renkleri uygulanmis)
    int mPaletteId = 0;
    LayerManager mLayers;          // katmanlar yukaridaki verilere bagli: onlardan SONRA tanimli olmali
    TerrainLayer* mTerrainLayer = nullptr;  // sahibi mLayers
    FmsLayer* mFmsLayer = nullptr;
    TrackLayer* mTrackLayer = nullptr;
    XMapWindow mWindow;             // mLayers / mAircraft / mDrawings'e bagli: en son

    // Otomatik kayit
    uint64_t mSavedSettings = 0;
    uint64_t mSavedDrawings = 0;
    uint64_t mSavedState = 0;
    uint64_t mExtraRevision = 0;   // markChanged ile artar
    uint64_t mSavedExtra = 0;
    float mLastSaveTime = -100.0f;
    RectI mFloatingGeometry;       // X-Plane icindeyken son yer (pop-out'ta degismez)
    std::string mStatusText;
    std::atomic<bool>* mDataLoadedFlag = nullptr;  // sahibi XMap; update() yazar
};
