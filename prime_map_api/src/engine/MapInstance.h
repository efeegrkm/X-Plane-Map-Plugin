// MapInstance.h
// TEK bir haritanin her seyi (0.23, coklu harita). API'deki prime::Map nesnesinin ic karsiligi.
//
//   MapInstance
//     |- LayerManager + katmanlar   (bu haritanin katman / alt katman durumu, zoom araliklari)
//     |- Drawings                   (bu haritanin cizimleri)
//     |- AircraftData               (ucak kaynagi harita basina: X-Plane ya da host)
//     |- TerrainPalette + numarasi  (bu haritanin zemin ayarlari)
//     |- MapWindow                  (gorunum, mod, UI, fare; XPLM penceresi acik ya da degil)
//     `- ayar dosyalari             (<veri>/maps/<ad>/ altinda)
//
// Ortak veri (nav, havalimanlari, hava yollari, zemin iscisi, onbellek) MapData'dadir;
// katmanlar oraya referansla bakar.
//
// Yasam dongusu:
//   kurulur (pencere YOK)  ->  ayarlar API ile verilir  ->  Open(): dosyalar okunur, pencere acilir
//   ->  acikken her ayar degisebilir, dosyalar kendiliginden yazilir  ->  Close(): yazilir, pencere
//   kapanir (nesne ve ayarlari kalir; yeniden Open edilebilir)
//
// Ayar dosyalari (Persistence::Off degilse), <veri>/maps/<ad ya da map_<kimlik>>/ altinda:
//   map_settings.ini    [window] [view] [render] [text] [tools] [layers.<mod>] [layer_zoom] [sources]
//   map_drawings.txt    kullanici cizimleri
//   terrain_colors.ini  bu haritanin zemin renkleri ve ayarlari
// Oncelik: Open() sirasinda dosyadaki deger, Open()'dan ONCE API ile verilen degerin yerine gecer
// (LoadAndSave); dosyada olmayan anahtar icin API ile verilen deger kalir. SaveOnly: dosya okunmaz
// (kod her acilista kazanir) ama yazilir. Off: dosya yok.
#pragma once

#include <cstdint>
#include <string>

#include "Drawings.h"
#include "LayerManager.h"
#include "MapWindow.h"
#include "data/AircraftData.h"
#include "data/TerrainClasses.h"

class FmsLayer;
class MapData;
class TerrainLayer;
class TrackLayer;

enum class MapPersistence
{
    Off,          // dosya okunmaz / yazilmaz
    SaveOnly,     // okunmaz (kodla verilen ayar her acilista gecerli), yazilir
    LoadAndSave,  // Open'da okunur, degisince yazilir (varsayilan)
};

class MapInstance
{
public:
    MapInstance(MapData& pData, int pId);
    ~MapInstance();

    MapInstance(const MapInstance&) = delete;
    MapInstance& operator=(const MapInstance&) = delete;

    // --- Kimlik ve dosyalar ---
    int Id() const;
    const std::string& Name() const;           // bos: adsiz (klasor map_<kimlik>)
    void SetName(const std::string& pName);    // cagiran (API) acik haritada / cakismada reddeder
    std::string FolderName() const;            // "ios_left" ya da "map_3" (dosya adina uygun hale getirilmis)
    std::string Directory() const;             // <veri>/maps/<FolderName>/
    static std::string SafeFolderName(const std::string& pName);  // harf, rakam, - _ . disi '_' olur
    void SetPersistence(MapPersistence pPersistence);
    MapPersistence Persistence() const;

    // --- Acma / kapama ---
    bool Open();   // dosyalari okur (LoadAndSave), pencereyi acar. Zaten aciksa true.
    void Close();  // dosyalari yazar, pencereyi kapatir (nesne kalir)
    bool IsOpen() const;

    // Flight loop'tan her karede (MapData): ucak, katman verisi, durum yazisi, otomatik kayit
    void Update(float pNow);

    // --- Parcalar ---
    MapWindow& Window();
    const MapWindow& Window() const;
    LayerManager& Layers();
    Drawings& UserDrawings();
    AircraftData& Aircraft();
    FmsLayer* Fms();
    TrackLayer* Track();
    MapData& Data();

    // --- Ayarlar ---
    // Haritanin butun ayarlari tek metin (map_settings.ini bicimi)
    std::string ExportSettings();
    void ImportSettings(const std::string& pText);
    // Zemin ayarlari (terrain_colors.ini bicimi): hemen uygulanir (yeni dokular yeni paletle istenir)
    void ImportTerrainSettings(const std::string& pText);
    std::string ExportTerrainSettings() const;
    const TerrainPalette& Palette() const;
    // Dosyalari simdi yazar (Persistence::Off ise false)
    bool SaveNow();
    // Kaynaklar degisti (ucak / rota: API cagirir): ayar dosyasina yazilsin
    void MarkChanged();

    // --- Durum ---
    int PendingTerrainImages() const;      // bu haritada bekleyen zemin dokusu (gorunmuyorsa 0)
    const std::string& StatusText() const; // alt seritteki yazi (ortak durum + bu haritanin zemini)
    bool IsDiagnosticsOn() const;

private:
    void CreateLayers();
    void LoadFiles();                 // Open: terrain_colors.ini, map_settings.ini, map_drawings.txt
    bool WriteSettingsFile();
    bool WriteDrawingsFile();
    bool WriteTerrainFile();
    void ApplyWindowSection();        // [window]: Standalone pencerenin yeri / gorunurlugu (dosyadan)
    void ApplySourcesSection();       // [sources]: ucak / rota kaynagi
    void StoreExtraSections();        // [window], [sources] -> pencerenin bellekteki ayarlarina
    RectI SavedGeometry() const;      // kaydedilecek yer (pop-out'ta X-Plane icindeki son yer)
    uint64_t StateSignature() const;  // revizyon sayaclarinin gormedigi degisiklikler (yer, gorunurluk)

    MapData& mData;
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
    MapWindow mWindow;             // mLayers / mAircraft / mDrawings'e bagli: en son

    // Otomatik kayit
    uint64_t mSavedSettings = 0;
    uint64_t mSavedDrawings = 0;
    uint64_t mSavedState = 0;
    uint64_t mExtraRevision = 0;   // MarkChanged ile artar
    uint64_t mSavedExtra = 0;
    float mLastSaveTime = -100.0f;
    RectI mFloatingGeometry;       // X-Plane icindeyken son yer (pop-out'ta degismez)
    std::string mStatusText;
};
