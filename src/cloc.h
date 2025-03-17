#define MAX_WORKERS 24
#define OUTPUT_LINE_WIDTH 80
#define CLOC_VERSION_STRING "cloc v0.1"

#define USE_CAS true // IF THIS IS FALSE, WE ARE NOT THREAD-SAFE!

#define FILE_COUNT_COLUMN_OFFSET    30
#define EMPTY_LINES_COLUMN_OFFSET   50
#define COMMENT_LINES_COLUMN_OFFSET 65
#define CODE_LINES_COLUMN_OFFSET    80

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
void append_string_with_max_length(String_Builder *builder, const char *data, s64 max_length);
void append_right_justified_string_at_offset(String_Builder *builder, const char *string, char pad, s64 offset);
void append_right_justified_integer_at_offset(String_Builder *builder, s64 integer, char pad, s64 offset);
void append_char(String_Builder *builder, char data);
void append_repeated_char(String_Builder *builder, char data, s64 n);
void print_string_builder(String_Builder *builder);
void print_string_builder_as_line(String_Builder *builder);
char *finish_string_builder(String_Builder *builder);

typedef enum Language {
    LANGUAGE_C,
    LANGUAGE_Cpp,
    LANGUAGE_Jai,
    LANGUAGE_COUNT,
} Language;

const char *LANGUAGE_STRINGS[LANGUAGE_COUNT] = { "C", "C++", "Jai" };

typedef enum Output_Mode {
    OUTPUT_By_File,
    OUTPUT_By_Language,
} Output_Mode;

typedef struct Stats {
    const char *ident;
    s64 blank;
    s64 comment;
    s64 code;
    s64 file_count;
} Stats;

typedef struct File {
    struct File *next;
    char *file_path;
    Language language;
    Stats stats;
} File;

typedef struct Cloc {
    Arena arena;

    b8 cli_valid;

    Output_Mode output_mode;

    // Over all outputted line table entries, we find the common prefix that we can then omit in the output table.
    // This avoids having very long paths when all the files are in the same directory.
    const char *common_prefix;
    s64 common_prefix_length;
    
    File *first_file;
    File *next_file;
    s64 file_count;
    
    Worker workers[MAX_WORKERS];
    s64 active_workers;
} Cloc;

File *get_next_file_to_parse(Cloc *cloc);
