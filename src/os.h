#if WIN32

#include <Windows.h>

# define PRIu64 "llu"
# define PRId64 "lld"
# define PRIx64 "llx"
# define FOUNDATION_LITTLE_ENDIAN true
# define true 1
# define false 0

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned long      u32;
typedef unsigned long long u64;

typedef signed char      s8;
typedef signed short     s16;
typedef signed long      s32;
typedef signed long long s64;

typedef float  f32;
typedef double f64;

typedef unsigned char b8;

typedef HANDLE Pid;

#elif LINUX

# define PRIu64 "llu"
# define PRId64 "lld"
# define PRIx64 "llx"
# define FOUNDATION_LITTLE_ENDIAN true
# define true 1
# define false 0

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;

typedef signed char      s8;
typedef signed short     s16;
typedef signed int       s32;
typedef signed long long s64;

typedef float  f32;
typedef double f64;

typedef bool b8;

#else
# error "This platform is not supported."
#endif
