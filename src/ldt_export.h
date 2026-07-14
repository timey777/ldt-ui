// ldt_export.h
#pragma once

#if defined(_WIN32) || defined(_WIN64)
  #if defined(LDT_EXPORTS)
    #define LDT_API __declspec(dllexport)
  #elif defined(LDT_STATIC)
    #define LDT_API
  #else
    #define LDT_API __declspec(dllimport)
  #endif
#else
  #if defined(__GNUC__) && __GNUC__ >= 4
    #define LDT_API __attribute__((visibility("default")))
  #else
    #define LDT_API
  #endif
#endif
