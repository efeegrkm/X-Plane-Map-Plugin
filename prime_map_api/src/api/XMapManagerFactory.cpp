#include "api/XMapManagerFactory.h"

#include "Log.h"
#include "api/XMapManager.h"

namespace prime
{
XMapManager* XMapManagerFactory::createXMapManagerImpl()
{
    return new XMapManager();
}

void XMapManagerFactory::destroyXMapManager(XMapManager* pManager)
{
    delete pManager;
}

void XMapManagerFactory::reportLibraryVersionMismatch(std::string_view pHostVersion)
{
    log("WARNING: host built with Prime Map API {}, loaded DLL is {} at {}; rebuild the host with the "
        "matching headers",
        pHostVersion, versionString(), libraryPath());
}
}
