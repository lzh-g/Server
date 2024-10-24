#ifdef _WIN32

#include <Windows.h>

#define HTTP_LIB __declspec(dllexport)

#else

#define HTTP_LIB

#endif