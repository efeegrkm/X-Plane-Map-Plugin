// primemapglobal.h
// Prime Map kutuphanesinin export makrosu.
//
// Kutuphaneyi DERLEYEN taraf PRIME_MAP_LIBRARY tanimiyla derler: semboller
// disa aktarilir (dllexport). Tuketen taraf bu tanimi yapmaz: semboller
// disaridan aktarilir (dllimport). Tanim CMake tarafindan yapilir
// (prime_map hedefine PRIVATE PRIME_MAP_LIBRARY).
#ifndef PRIME_MAP_GLOBAL_H
#define PRIME_MAP_GLOBAL_H

#if defined(_MSC_VER) || defined(_WIN32)
    #if defined(PRIME_MAP_LIBRARY)
        #define PRIME_MAP_EXPORT __declspec(dllexport)
    #else
        #define PRIME_MAP_EXPORT __declspec(dllimport)
    #endif
#else
    #define PRIME_MAP_EXPORT __attribute__((visibility("default")))
#endif

#endif  // PRIME_MAP_GLOBAL_H
