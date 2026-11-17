// prime::XMapManagerFactory: yoneticiyi olusturan / bırakan tek nokta. Her create cagrisi
// YENI bir yonetici (ve ayri onbellek butceleri) verir; singleton yoktur.
#ifndef PRIME_MAP_SRC_API_XMAPMANAGERFACTORY_H
#define PRIME_MAP_SRC_API_XMAPMANAGERFACTORY_H

#include "XMapTypes.h"

namespace prime
{

class XMapManager;

class PRIME_MAP_EXPORT XMapManagerFactory
{
public:
    static XMapManager* createXMapManager();  // her cagri yeni yonetici (ve ayri onbellek) uretir
    static void destroyXMapManager(XMapManager* pManager);

private:
    virtual ~XMapManagerFactory() = default;
    explicit XMapManagerFactory() = default;
};

}  // namespace prime

#endif  // PRIME_MAP_SRC_API_XMAPMANAGERFACTORY_H
