#define MAX_WORKERS 24
#define CLOC_VERSION_STRING "cloc v0.1"

typedef struct Arena {
    char *base;
    s64 reserved;
    s64 committed;
} Arena;

void create_arena(Arena *arena, s64 reserved);
void *push_arena(Arena *arena, s64 bytes);
char *push_string(Arena *arena, char *input);
void destroy_arena(Arena *arena);

typedef struct File {
    struct File *next;
    char *file_path;
} File;

typedef struct Cloc {
    Arena arena;

    b8 cli_valid;
    
    File *first_file;
    File *next_file;
} Cloc;

void register_file_to_parse(Cloc *cloc, char *file_path);
