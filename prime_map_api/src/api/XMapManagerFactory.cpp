// prime::XMapManagerFactory uygulamasi.
#include "api/XMapManagerFactory.h"

#include "Log.h"
#include "api/XMapManager.h"

namespace prime
{

XMapManager* XMapManagerFactory::createXMapManagerImpl()
{
    // Her cagri ayri yonetici (ve ayri onbellek) uretir; bir X-Plane surecinde bir tane yeterlidir.
    // Fazlasi onerilmez.
    return new XMapManager();
}

void XMapManagerFactory::destroyXMapManager(XMapManager* pManager)
{
    delete pManager;
}

void XMapManagerFactory::reportLibraryVersionMismatch(std::string_view pHostVersion)
{
    // Sarmalayici (header) host tarafinda derlenir; surum sabitleri host ikilisine islenmistir.
    // Burasi DLL'de calisir: kendi surumunu ve yolunu soyleyebilir.
    log("WARNING: host built with Prime Map API {}, loaded DLL is {} at {}; rebuild the host with the "
        "matching headers",
        pHostVersion, versionString(), libraryPath());
}

}  // namespace prime
