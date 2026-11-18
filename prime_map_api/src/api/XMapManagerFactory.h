// prime::XMapManagerFactory: yoneticiyi olusturan / birakan tek nokta. Her create cagrisi
// YENI bir yonetici (ve ayri onbellek butceleri) verir; singleton yoktur.
//
// createXMapManager kasitli olarak INLINE'dir: surum eslestirmesi HOST tarafinda derlenir.
// Host eski ya da yeni basliklarla derlendiyse (yuklu DLL baska surumde) cagriyi kabul etmeden
// once net bir WARNING yazilir (host built with X, loaded DLL is Y at <yol>).
#ifndef PRIME_MAP_SRC_API_XMAPMANAGERFACTORY_H
#define PRIME_MAP_SRC_API_XMAPMANAGERFACTORY_H

#include "XMapTypes.h"

namespace prime
{

class XMapManager;

class PRIME_MAP_EXPORT XMapManagerFactory
{
public:
    // Host'un derleme anindaki surumu yuklu DLL'in surumuyle uyusmuyorsa WARNING, sonra yonetici.
    // Cagri bicimi degismedi: eski host kodlari aynen derlenir (1.0.0, LIF-08).
    static XMapManager* createXMapManager()
    {
        if (!isLibraryVersionMatching())
        {
            reportLibraryVersionMismatch(hostVersionString());
        }
        return createXMapManagerImpl();
    }
    static void destroyXMapManager(XMapManager* pManager);

private:
    static XMapManager* createXMapManagerImpl();  // DLL tarafi; yalnizca sarmalayici cagirir
    // pHostVersion: host'un derleme anindaki surum metni; DLL kendi surumunu ve yolunu ekler
    static void reportLibraryVersionMismatch(std::string_view pHostVersion);

    virtual ~XMapManagerFactory() = default;
    explicit XMapManagerFactory() = default;
};

}  // namespace prime

#endif  // PRIME_MAP_SRC_API_XMAPMANAGERFACTORY_H
