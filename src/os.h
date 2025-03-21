struct Arena;

#if WIN32
# include <Windows.h>
# include <psapi.h>

# define PRIu64 "llu"
# define PRId64 "lld"
# define PRIx64 "llx"
# define true 1
# define false 0
# define thread_local __declspec(thread)

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
typedef HANDLE File_Handle;
typedef HANDLE File_Iterator_Handle;

#elif POSIX
# include <linux/limits.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <unistd.h>
# include <dirent.h>
# include <pthread.h>
# include <sys/resource.h>

# define PRIu64 "lu"
# define PRId64 "ld"
# define PRIx64 "lx"
# define true 1
# define false 0
# define thread_local __thread

typedef unsigned char   u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef unsigned long  u64;

typedef signed char   s8;
typedef signed short s16;
typedef signed int   s32;
typedef signed long  s64;

typedef float  f32;
typedef double f64;

typedef unsigned char b8;

typedef u64 Pid;
typedef int File_Handle;
typedef DIR *File_Iterator_Handle;

#else
# error "This platform is not supported."
#endif

#define min(lhs, rhs) ((lhs) < (rhs) ? (lhs) : (rhs))
#define max(lhs, rhs) ((lhs) > (rhs) ? (lhs) : (rhs))

typedef enum OS_Path_Kind {
    OS_PATH_Non_Existent,
    OS_PATH_Is_File,
    OS_PATH_Is_Directory,
} OS_Path_Kind;

OS_Path_Kind os_resolve_path_kind(char *path);
char *os_make_absolute_path(struct Arena *arena, char *path);
File_Handle os_open_file(char *path);
s64 os_get_file_size(File_Handle handle);
s64 os_read_file(File_Handle handle, char *dst, s64 offset, s64 size);
void os_close_file(File_Handle handle);

typedef struct File_Iterator {
    b8 valid;
    File_Iterator_Handle native_handle;
    char *path;
    OS_Path_Kind kind;
} File_Iterator;

File_Iterator find_first_file(struct Arena *arena, char *directory_path);
void find_next_file(struct Arena *arena, File_Iterator *iterator);
void close_file_iterator(File_Iterator *iterator);

s64 os_get_hardware_thread_count();
Pid os_spawn_thread(int (*procedure)(void *), void *argument);
void os_join_thread(Pid pid);
void *os_compare_and_swap(void *volatile *dst, void *exchange, void *comparand);

typedef s64 Hardware_Time;

Hardware_Time os_get_hardware_time();
f64 os_convert_hardware_time_to_seconds(Hardware_Time delta);

s64 os_get_working_set_size();
void os_sleep(f64 seconds);
