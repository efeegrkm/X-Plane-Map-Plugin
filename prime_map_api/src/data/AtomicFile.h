// Onbellek dosyalarini benzersiz gecici dosya ve yeniden adlandirma ile atomik yazar.
#ifndef PRIME_MAP_SRC_DATA_ATOMICFILE_H
#define PRIME_MAP_SRC_DATA_ATOMICFILE_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <random>
#include <string>
#include <thread>

inline bool writeFileAtomic(const std::string& pPath, const std::function<void(std::ofstream&)>& pWriter)
{
    static const std::uint64_t gProcessTag =
        (static_cast<std::uint64_t>(std::random_device{}()) << 32) ^
        static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    static std::atomic<std::uint64_t> gCounter{0};
    const std::uint64_t tCounter = gCounter.fetch_add(1);
    const std::string tTemporary = pPath + "." + std::to_string(gProcessTag) + "." +
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
            std::filesystem::remove(tTemporary, tCleanup);
            return false;
        }
    }
    std::error_code tError;
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
    std::filesystem::remove(tTemporary, tCleanup);
    return false;
}

#endif
