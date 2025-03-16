struct Arena;

typedef enum OS_Path_Kind {
    OS_PATH_Non_Existent,
    OS_PATH_Is_File,
    OS_PATH_Is_Directory,
} OS_Path_Kind;

#if WIN32
# include <Windows.h>
# include <psapi.h>

# define PRIu64 "llu"
# define PRId64 "lld"
# define PRIx64 "llx"
# define FOUNDATION_LITTLE_ENDIAN true
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

typedef struct File_Iterator {
    b8 valid;
    HANDLE native_handle;
    char *path;
    OS_Path_Kind kind;
} File_Iterator;

#else
# error "This platform is not supported."
#endif

OS_Path_Kind os_resolve_path_kind(char *path);
char *os_make_absolute_path(struct Arena *arena, char *path);

File_Iterator find_first_file(struct Arena *arena, char *directory_path);
void find_next_file(struct Arena *arena, File_Iterator *iterator);
void close_file_iterator(File_Iterator *iterator);

s64 os_get_hardware_thread_count();
Pid os_spawn_thread(int (*procedure)(void *), void *argument);
void os_join_thread(Pid pid);

typedef s64 Hardware_Time;

Hardware_Time os_get_hardware_time();
f64 os_convert_hardware_time_to_seconds(Hardware_Time delta);

s64 os_get_working_set_size();
