// Prime Map export makrosu: kutuphaneyi derleyen taraf PRIME_MAP_LIBRARY tanimlar
// (dllexport), tuketen taraf yapmaz (dllimport). Tanim CMake tarafindan yapilir.
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
