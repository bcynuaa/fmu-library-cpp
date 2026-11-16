#ifndef FMI2_PLATFORM_H_
#define FMI2_PLATFORM_H_

#if defined(_WIN32)
#if defined(FMI2_BUILD)
#define FMI2_EXPORT __declspec(dllexport)
#else
#define FMI2_EXPORT __declspec(dllimport)
#endif
#define FMI2_CALL __cdecl
#else
#define FMI2_EXPORT __attribute__((visibility("default")))
#define FMI2_CALL
#endif

#endif  // FMI2_PLATFORM_H_
