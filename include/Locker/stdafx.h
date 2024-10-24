#ifdef _WIN32

#include <Windows.h>

#define LOCKER_LIB __declspec(dllexport)

#else

#define LOCKER_LIB

#endif