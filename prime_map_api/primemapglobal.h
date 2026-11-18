// DLL disa aktarim makrosu: kutuphane PRIME_MAP_LIBRARY ile dllexport, host dllimport kullanir.
#ifndef PRIME_MAP_PRIMEMAPGLOBAL_H
#define PRIME_MAP_PRIMEMAPGLOBAL_H

#if defined(_MSC_VER) || defined(_WIN32)
    #if defined(PRIME_MAP_LIBRARY)
        #define PRIME_MAP_EXPORT __declspec(dllexport)
    #else
        #define PRIME_MAP_EXPORT __declspec(dllimport)
    #endif
#else
    #define PRIME_MAP_EXPORT __attribute__((visibility("default")))
#endif

#endif
