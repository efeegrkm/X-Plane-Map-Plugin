// Onbellek dosyalari icin atomik yazi: once <yol>.tmp'ye yaz, sonra yeniden adlandir.
// Iki yonetici / iki isci ayni dosyaya ayni anda yazsa bile okuyan taraf ya eski dosyayi ya da
// tam yazilmis yenisini gorur; yarim yazilmis dosya asla gorunur olmaz (CacheTrim yarismasi dahil).
#ifndef PRIME_MAP_SRC_DATA_ATOMICFILE_H
#define PRIME_MAP_SRC_DATA_ATOMICFILE_H

#include <filesystem>
#include <fstream>
#include <functional>
#include <string>

// pWriter akisa icerigi yazar; akis hataliysa dosya yayinlanmaz (hedefte eski icerik kalir).
inline bool writeFileAtomic(const std::string& pPath, const std::function<void(std::ofstream&)>& pWriter)
{
    const std::string tTemporary = pPath + ".tmp";
    {
        std::ofstream tFile(tTemporary, std::ios::binary | std::ios::trunc);
        if (!tFile)
        {
            return false;
        }
        pWriter(tFile);
        if (!tFile)
        {
            return false;
        }
    }
    std::error_code tError;
    std::filesystem::rename(tTemporary, pPath, tError);
    return !tError;
}

#endif  // PRIME_MAP_SRC_DATA_ATOMICFILE_H
