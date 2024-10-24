#ifdef _WIN32

#include <Windows.h>

#define CGIMYSQL_LIB __declspec(dllexport)

#else

#define CGIMYSQL_LIB

#endif