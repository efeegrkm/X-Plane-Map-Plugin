// Yonetici olusturan ve silen fabrika; olusturmada host ile DLL surumunu karsilastirir.
#ifndef PRIME_MAP_SRC_API_XMAPMANAGERFACTORY_H
#define PRIME_MAP_SRC_API_XMAPMANAGERFACTORY_H

#include "XMapTypes.h"

namespace prime
{
class XMapManager;

class PRIME_MAP_EXPORT XMapManagerFactory
{
public:
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
    static XMapManager* createXMapManagerImpl();
    static void reportLibraryVersionMismatch(std::string_view pHostVersion);

    virtual ~XMapManagerFactory() = default;
    explicit XMapManagerFactory() = default;
};
}

#endif
