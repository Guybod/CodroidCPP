#ifndef CODROID_EXPORT_H
#define CODROID_EXPORT_H

#if defined(_WIN32) || defined(__CYGWIN__)
  #ifdef CODROID_EXPORTS
    #define CODROID_API __declspec(dllexport)
  #else
    #define CODROID_API __declspec(dllimport)
  #endif
#else
  #if __GNUC__ >= 4
    #define CODROID_API __attribute__ ((visibility ("default")))
  #else
    #define CODROID_API
  #endif
#endif

#endif