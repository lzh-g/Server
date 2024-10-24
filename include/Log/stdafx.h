#ifdef _WIN32

#include <Windows.h>

#define LOG_LIB __declspec(dllexport)

#else

#define LOG_LIB

#endif