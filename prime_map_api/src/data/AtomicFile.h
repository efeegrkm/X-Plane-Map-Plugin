// Onbellek dosyalari icin atomik yazi: once benzersiz gecici dosyaya yaz, sonra yeniden adlandir.
// Benzersiz ad (surec + is + sayac): ayni dosyaya yazan iki yonetici / iki isci birbirinin gecici
// dosyasini ezemez. Iki taraf da tamamlarsa hedefte son yazanin icerigi kalir; okuyan taraf ya eski
// dosyayi ya da tam yazilmis yenisini gorur; yarim yazilmis dosya asla gorunur olmaz (CacheTrim
// yarismasi dahil). Cokerkenden kalan gecici dosyalari CacheTrim temizler.
#ifndef PRIME_MAP_SRC_DATA_ATOMICFILE_H
#define PRIME_MAP_SRC_DATA_ATOMICFILE_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <process.h>  // _getpid (Windows)
#include <string>
#include <thread>

// pWriter akisa icerigi yazar; akis hataliysa dosya yayinlanmaz (hedefte eski icerik kalir).
inline bool writeFileAtomic(const std::string& pPath, const std::function<void(std::ofstream&)>& pWriter)
{
    // Benzersiz gecici ad: <yol>.<pid>.<is>.<sayac>.tmp (statik sayac: surec icinde tekrar etmez)
    static std::atomic<uint64_t> kCounter{0};
    const std::uint64_t tCounter = kCounter.fetch_add(1);
    const std::string tTemporary = pPath + "." + std::to_string(_getpid()) + "." +
                                   std::to_string(std::hash<std::thread::id>()(std::this_thread::get_id())) +
                                   "." + std::to_string(tCounter) + ".tmp";
    {
        std::ofstream tFile(tTemporary, std::ios::binary | std::ios::trunc);
        if (!tFile)
        {
            return false;
        }
        pWriter(tFile);
        if (!tFile)
        {
            std::error_code tCleanup;
            std::filesystem::remove(tTemporary, tCleanup);  // yarim gecici dosya kalmasin
            return false;
        }
    }
    std::error_code tError;
    // Hedef dosyayi baska bir is okurken Windows rename'inin yerine koymasi kisaca reddedilir:
    // kisa bekleyip yeniden dene (okuyan taraf dosyayi hemen kapatan RAII akislaridir)
    for (int tAttempt = 0; tAttempt < 10; ++tAttempt)
    {
        std::filesystem::rename(tTemporary, pPath, tError);
        if (!tError)
        {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2 * (tAttempt + 1)));
    }
    std::error_code tCleanup;
    std::filesystem::remove(tTemporary, tCleanup);  // yeniden adlandirilamadi: gecici dosyayi sil
    return false;
}

#endif  // PRIME_MAP_SRC_DATA_ATOMICFILE_H
