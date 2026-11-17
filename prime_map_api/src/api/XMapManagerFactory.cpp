// prime::XMapManagerFactory uygulamasi.
#include "api/XMapManagerFactory.h"

#include "api/XMapManager.h"

namespace prime
{

XMapManager* XMapManagerFactory::createXMapManager()
{
    // Her cagri ayri yonetici (ve ayri onbellek) uretir; bir X-Plane surecinde bir tane yeterlidir. Fazlası önerilmez.
    return new XMapManager();
}

void XMapManagerFactory::destroyXMapManager(XMapManager* pManager)
{
    delete pManager;
}

}  // namespace prime
