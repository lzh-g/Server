#ifdef _WIN32

#include <Windows.h>

#define THREADPOOL_LIB __declspec(dllexport)

#else

#define THREADPOOL_LIB

#endif