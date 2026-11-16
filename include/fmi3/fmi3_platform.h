#ifndef FMI3_PLATFORM_H_
#define FMI3_PLATFORM_H_

#if defined(_WIN32)
#if defined(FMI3_BUILD)
#define FMI3_EXPORT __declspec(dllexport)
#else
#define FMI3_EXPORT __declspec(dllimport)
#endif
#define FMI3_CALL __cdecl
#else
#define FMI3_EXPORT __attribute__((visibility("default")))
#define FMI3_CALL
#endif

#endif  // FMI3_PLATFORM_H_
