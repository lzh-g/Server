#ifdef _WIN32

#include <Windows.h>

#define TIMER_LIB __declspec(dllexport)

#else

#define TIMER_LIB

#endif