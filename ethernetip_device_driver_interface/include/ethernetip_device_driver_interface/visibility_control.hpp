#ifndef ETHERNETIP_DEVICE_DRIVER_INTERFACE__VISIBILITY_CONTROL_HPP_
#define ETHERNETIP_DEVICE_DRIVER_INTERFACE__VISIBILITY_CONTROL_HPP_

#if defined _WIN32 || defined __CYGWIN__
  #ifdef __GNUC__
    #define ETHERNETIP_DEVICE_EXPORT __attribute__((dllexport))
    #define ETHERNETIP_DEVICE_IMPORT __attribute__((dllimport))
  #else
    #define ETHERNETIP_DEVICE_EXPORT __declspec(dllexport)
    #define ETHERNETIP_DEVICE_IMPORT __declspec(dllimport)
  #endif
  #ifdef ETHERNETIP_DEVICE_BUILDING_LIBRARY
    #define ETHERNETIP_DEVICE_PUBLIC ETHERNETIP_DEVICE_EXPORT
  #else
    #define ETHERNETIP_DEVICE_PUBLIC ETHERNETIP_DEVICE_IMPORT
  #endif
  #define ETHERNETIP_DEVICE_LOCAL
#else
  #define ETHERNETIP_DEVICE_EXPORT __attribute__((visibility("default")))
  #define ETHERNETIP_DEVICE_IMPORT
  #if __GNUC__ >= 4
    #define ETHERNETIP_DEVICE_PUBLIC __attribute__((visibility("default")))
    #define ETHERNETIP_DEVICE_LOCAL __attribute__((visibility("hidden")))
  #else
    #define ETHERNETIP_DEVICE_PUBLIC
    #define ETHERNETIP_DEVICE_LOCAL
  #endif
#endif

#endif  // ETHERNETIP_DEVICE_DRIVER_INTERFACE__VISIBILITY_CONTROL_HPP_
