#ifndef ETHERNETIP_MASTER__VISIBILITY_CONTROL_HPP_
#define ETHERNETIP_MASTER__VISIBILITY_CONTROL_HPP_

#if defined _WIN32 || defined __CYGWIN__
  #ifdef __GNUC__
    #define ETHERNETIP_MASTER_EXPORT __attribute__((dllexport))
    #define ETHERNETIP_MASTER_IMPORT __attribute__((dllimport))
  #else
    #define ETHERNETIP_MASTER_EXPORT __declspec(dllexport)
    #define ETHERNETIP_MASTER_IMPORT __declspec(dllimport)
  #endif
  #ifdef ETHERNETIP_MASTER_BUILDING_LIBRARY
    #define ETHERNETIP_MASTER_PUBLIC ETHERNETIP_MASTER_EXPORT
  #else
    #define ETHERNETIP_MASTER_PUBLIC ETHERNETIP_MASTER_IMPORT
  #endif
  #define ETHERNETIP_MASTER_LOCAL
#else
  #define ETHERNETIP_MASTER_EXPORT __attribute__((visibility("default")))
  #define ETHERNETIP_MASTER_IMPORT
  #if __GNUC__ >= 4
    #define ETHERNETIP_MASTER_PUBLIC __attribute__((visibility("default")))
    #define ETHERNETIP_MASTER_LOCAL __attribute__((visibility("hidden")))
  #else
    #define ETHERNETIP_MASTER_PUBLIC
    #define ETHERNETIP_MASTER_LOCAL
  #endif
#endif

#endif  // ETHERNETIP_MASTER__VISIBILITY_CONTROL_HPP_
