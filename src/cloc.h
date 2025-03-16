#define MAX_WORKERS 24
#define OUTPUT_LINE_WIDTH 80
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

typedef struct String_Builder {
    Arena *arena;
    char *pointer;
    s64 size_in_characters;
} String_Builder;

void create_string_builder(String_Builder *builder, Arena *arena);
void append_string(String_Builder *builder, const char *data);
void append_char(String_Builder *builder, char data);
void append_repeated_char(String_Builder *builder, char data, s64 n);
void print_string_builder(String_Builder *builder);
void print_string_builder_as_line(String_Builder *builder);

typedef enum Output_Mode {
    OUTPUT_By_File,
    OUTPUT_Total,
} Output_Mode;

typedef struct File {
    struct File *next;
    char *file_path;

    s64 lines;
} File;

typedef struct Cloc {
    Arena arena;

    b8 cli_valid;

    Output_Mode output_mode;
    
    File *first_file;
    File *next_file;
    s64 file_count;
    
    Worker workers[MAX_WORKERS];
    s64 active_workers;
} Cloc;

void register_file_to_parse(Cloc *cloc, char *file_path);
