// --- C Includes ---
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <string.h>

// --- Local Headers ---
#include "os.h"
#include "worker.h"
#include "cloc.h"

// --- Local Sources ---
#include "worker.c"

#if WIN32
# include "win32.c"
#elif POSIX
# include "posix.c"
#endif

typedef struct File_Extension_Map {
    const char *extension;
    Language language;
} File_Extension_Map;

static File_Extension_Map FILE_EXTENSION_MAP[] = {
    { "c", LANGUAGE_C },
    { "h", LANGUAGE_C_Header },
    { "cpp", LANGUAGE_Cpp },
    { "hpp", LANGUAGE_Cpp },
    { "inl", LANGUAGE_Cpp },
    { "jai", LANGUAGE_Jai },
};

const s64 FILE_EXTENSION_MAP_SIZE = sizeof(FILE_EXTENSION_MAP) / sizeof(FILE_EXTENSION_MAP[0]);

/* -------------------------------------------------- Arena -------------------------------------------------- */

void create_arena(Arena *arena, s64 reserved) {
    arena->base      = (char *) malloc(reserved);
    arena->reserved  = reserved;
    arena->committed = 0;
    assert(arena->base != NULL && "Arena reservation is too large!");
}

void *push_arena(Arena *arena, s64 bytes) {
    assert(arena->committed + bytes <= arena->reserved && "Arena ran out of space!");
    void *pointer = (void *) (arena->base + arena->committed);
    arena->committed += bytes;
    return pointer;
}

char *push_string(Arena *arena, char *input) {
    s64 length = strlen(input);
    char *output = push_arena(arena, length + 1);
    strcpy(output, input);
    return output;
}

s64 mark_arena(Arena *arena) {
    return arena->committed;
}

void reset_arena(Arena *arena, s64 mark) {
    arena->committed = mark;
}

void destroy_arena(Arena *arena) {
    free(arena->base);
    arena->base      = NULL;
    arena->reserved  = 0;
    arena->committed = 0;
}



/* ---------------------------------------------- String Builder ---------------------------------------------- */

#define aprint(arena, format, ...) push_arena(arena, sprintf((arena)->base + (arena)->committed, format, ##__VA_ARGS__) + 1)
#define sprint(builder, format, ...) push_string_builder(builder, sprintf((builder)->arena->base + (builder)->arena->committed, format, ##__VA_ARGS__))

static
char *push_string_builder(String_Builder *builder, s64 characters) {
    assert(builder->pointer + builder->size_in_characters == builder->arena->base + builder->arena->committed); // Make sure we are still contiguous inside the arena and nothing else has used the arena since. Otherwise, our string content would be corrupted!
    char *pointer = push_arena(builder->arena, characters);
    builder->size_in_characters += characters;
    return pointer;
}

void create_string_builder(String_Builder *builder, Arena *arena) {
    builder->arena   = arena;
    builder->pointer = push_arena(builder->arena, 0);
    builder->size_in_characters = 0;
}

void append_string(String_Builder *builder, const char *data) {
    s64 length = strlen(data);
    char *pointer = push_string_builder(builder, length);
    memcpy(pointer, data, length);
}

void append_string_with_max_length(String_Builder *builder, const char *data, s64 max_length) {
    s64 actual_length = strlen(data);
    s64 length    = min(max_length, actual_length);
    char *pointer = push_string_builder(builder, length);
    s64 offset    = max(actual_length - max_length, 0);
    memcpy(pointer, &data[offset], length);    
}

void append_char(String_Builder *builder, char data) {
    char *pointer = push_string_builder(builder, 1);
    *pointer = data;
}

void append_repeated_char(String_Builder *builder, char data, s64 n) {
    char *pointer = push_string_builder(builder, n);
    memset(pointer, data, n);
}

void append_right_justified_string_at_offset(String_Builder *builder, const char *string, char pad, s64 offset) {
    s64 string_length = strlen(string);

    while(builder->size_in_characters < offset - string_length) {
        append_char(builder, pad);
    }

    append_string(builder, string);
}

void append_right_justified_integer_at_offset(String_Builder *builder, s64 integer, char pad, s64 offset) {
    s64 string_length = snprintf(NULL, 0, "%" PRId64, integer);

    while(builder->size_in_characters < offset - string_length) {
        append_char(builder, pad);
    }

    sprint(builder, "%" PRId64, integer);
}

void print_string_builder(String_Builder *builder) {
    printf("%.*s", (int) builder->size_in_characters, builder->pointer);
}

void print_string_builder_as_line(String_Builder *builder) {
    printf("%.*s\n", (int) builder->size_in_characters, builder->pointer);
}

char *finish_string_builder(String_Builder *builder) {
    append_char(builder, 0);
    return builder->pointer;
}



/* ----------------------------------------------- String List ----------------------------------------------- */

String_List *append_string_list(Arena *arena, String_List *previous, char *content) {
    String_List *next = push_arena(arena, sizeof(String_List));
    next->next        = previous;
    next->content     = content;
    return next;
}

b8 string_list_contains(String_List *list, char *needle) {
    while(list) {
        if(strcmp(list->content, needle) == 0) return true;
        list = list->next;
    }
    return false;
}



/* ----------------------------------------------- Cloc Helpers ----------------------------------------------- */

File *get_next_file_to_parse(Cloc *cloc) {
#if USE_CAS
    File *current;

    do {
        current  = cloc->next_file;
    } while(current != NULL && current != os_compare_and_swap((void *volatile *) &cloc->next_file, current->next, current));

    return current;
#else
    if(cloc->next_file == NULL) return NULL;

    File *current = cloc->next_file;
    cloc->next_file = current->next;
    return current;
#endif
}



/* ---------------------------------------------- Stats Handling ---------------------------------------------- */

static
void combine_stats(Stats *dst, Stats *src) {
    dst->blank   += src->blank;
    dst->comment += src->comment;
    dst->code    += src->code;
    dst->file_count += src->file_count;
}

static
char *combine_file_paths(Cloc *cloc, char *directory_path, char *file_path) {
    s64 directory_path_length = strlen(directory_path);

    if(directory_path_length == 0) return file_path;
    
    String_Builder builder;
    create_string_builder(&builder, &cloc->scratch);
    append_string(&builder, directory_path);
    if(directory_path[directory_path_length - 1] != '/' && directory_path[directory_path_length - 1] != '\\')
        append_char(&builder, '/');
    append_string(&builder, file_path);
    
    return finish_string_builder(&builder);
}

static
char *find_file_extension(char *file_path) {
    s64 index = strlen(file_path);
    while(index > 0 && file_path[index - 1] != '.') --index;
    return index ? &file_path[index] : NULL;
}

static
void register_file_to_parse(Cloc *cloc, char *file_path) {
    char *file_extension = find_file_extension(file_path);
    if(!file_extension) return; // Files without a file extension are unsupported

    Language language = LANGUAGE_COUNT;

    for(s64 i = 0; i < FILE_EXTENSION_MAP_SIZE && language == LANGUAGE_COUNT; ++i) {
        if(strcmp(FILE_EXTENSION_MAP[i].extension, file_extension) == 0) {
            language = FILE_EXTENSION_MAP[i].language;
        }
    }

    if(language == LANGUAGE_COUNT) return; // Unrecognized language, ignore
    
    File *entry      = push_arena(&cloc->perm, sizeof(File));
    entry->next      = cloc->first_file;
    entry->file_path = os_make_absolute_path(&cloc->perm, file_path);
    entry->language  = language;
    entry->stats.ident      = entry->file_path;
    entry->stats.blank      = 0;
    entry->stats.comment    = 0;
    entry->stats.code       = 0;
    entry->stats.file_count = 1;
    cloc->first_file = entry;
    cloc->next_file  = entry;
    ++cloc->file_count;
}

static
void register_directory_to_parse(Cloc *cloc, char *directory_path) {
    s64 mark = mark_arena(&cloc->scratch);
    
    char *resolved_path = os_make_absolute_path(&cloc->scratch, directory_path); // Resolve any tricks in this path here to make our future easier.
    
    File_Iterator iterator = find_first_file(&cloc->scratch, resolved_path);
    
    while(iterator.valid) {
        if(strcmp(iterator.path, ".") == 0 || strcmp(iterator.path, "..") == 0) {
            // Ignore these paths
        } else if(iterator.kind == OS_PATH_Is_Directory && !string_list_contains(cloc->excluded_directories, iterator.path)) {
            register_directory_to_parse(cloc, combine_file_paths(cloc, resolved_path, iterator.path));
        } else if(iterator.kind == OS_PATH_Is_File) {
            register_file_to_parse(cloc, combine_file_paths(cloc, resolved_path, iterator.path));
        }

        find_next_file(&cloc->scratch, &iterator);
    }

    reset_arena(&cloc->scratch, mark);
}



/* ----------------------------------------------- Table Output ----------------------------------------------- */

static
void print_separator_line(Cloc *cloc, const char *content) {
    const char DELIMITER_CHAR = '-';

    s64 content_length = strlen(content);
    if(content_length > 0) {
        String_Builder builder;
        create_string_builder(&builder, &cloc->scratch);
    
        s64 total_stars = (OUTPUT_LINE_WIDTH - content_length - 2);
        s64 lhs_stars = total_stars / 2;
        s64 rhs_stars = total_stars / 2 + total_stars % 2;

        append_repeated_char(&builder, DELIMITER_CHAR, lhs_stars);
        append_char(&builder, ' ');
        append_string(&builder, content);
        append_char(&builder, ' ');
        append_repeated_char(&builder, DELIMITER_CHAR, rhs_stars);
    
        print_string_builder_as_line(&builder);
    } else {
        char line[OUTPUT_LINE_WIDTH];
        memset(line, DELIMITER_CHAR, sizeof(line));
        printf("%.*s\n", OUTPUT_LINE_WIDTH, line);
    }
}

static
void print_table_header_line(Cloc *cloc) {
    String_Builder builder;
    create_string_builder(&builder, &cloc->scratch);

    switch(cloc->output_mode) {
    case OUTPUT_By_File:
        append_string(&builder, "File");
        append_right_justified_string_at_offset(&builder, "Empty",   ' ', EMPTY_LINES_COLUMN_OFFSET);
        append_right_justified_string_at_offset(&builder, "Comment", ' ', COMMENT_LINES_COLUMN_OFFSET);
        append_right_justified_string_at_offset(&builder, "Code",    ' ', CODE_LINES_COLUMN_OFFSET);
        break;

    case OUTPUT_By_Language:
        append_string(&builder, "Language");
        append_right_justified_string_at_offset(&builder, "Files",    ' ', FILE_COUNT_COLUMN_OFFSET);
        append_right_justified_string_at_offset(&builder, "Empty",    ' ', EMPTY_LINES_COLUMN_OFFSET);
        append_right_justified_string_at_offset(&builder, "Comment",  ' ', COMMENT_LINES_COLUMN_OFFSET);
        append_right_justified_string_at_offset(&builder, "Code",     ' ', CODE_LINES_COLUMN_OFFSET);
        break;
    }

    print_string_builder_as_line(&builder);
}

static
void print_table_entry_line(Cloc *cloc, Stats *stats, b8 is_language_entries) {
    String_Builder builder;
    create_string_builder(&builder, &cloc->scratch);
    append_string_with_max_length(&builder, &stats->ident[cloc->common_prefix_length], (is_language_entries ? FILE_COUNT_COLUMN_OFFSET : EMPTY_LINES_COLUMN_OFFSET) - 3);
    if(is_language_entries)
        append_right_justified_integer_at_offset(&builder, stats->file_count,    ' ', FILE_COUNT_COLUMN_OFFSET);
    append_right_justified_integer_at_offset(&builder, stats->blank,         ' ', EMPTY_LINES_COLUMN_OFFSET);
    append_right_justified_integer_at_offset(&builder, stats->comment,       ' ', COMMENT_LINES_COLUMN_OFFSET);
    append_right_justified_integer_at_offset(&builder, stats->code,          ' ', CODE_LINES_COLUMN_OFFSET);
    print_string_builder_as_line(&builder);    
}

static
void set_initial_common_prefix(Cloc *cloc, const char *ident) {
    cloc->common_prefix = ident;
    cloc->common_prefix_length = strlen(cloc->common_prefix);
    while(cloc->common_prefix_length > 0 && cloc->common_prefix[cloc->common_prefix_length - 1] != '/' && cloc->common_prefix[cloc->common_prefix_length - 1] != '\\') {
        --cloc->common_prefix_length;
    }
}

static
void adapt_common_prefix(Cloc *cloc, const char *ident) {
    //
    // Find the common prefix between this new file and the stored prefix.
    // If this new file doesn't share the complete stored prefix, we shorten the stored prefix
    // until all files can share this prefix again.
    //
    s64 file_path_length = strlen(ident);
    s64 max_length = min(cloc->common_prefix_length, file_path_length);
    s64 index = 0;
        
    while(index < max_length && ident[index] == cloc->common_prefix[index]) {
        if(ident[index] == '\\') {
            cloc->common_prefix_length = index + 1;
        } else if(ident[index] == '/') {
            cloc->common_prefix_length = index + 1;
        }
        ++index;
    }
}

static
void prepare_stats(Cloc *cloc, Stats *stats, s64 stat_count, b8 set_common_prefix) {
    if(!stat_count) return;

    for(s64 i = 0; i < stat_count; ++i) {
        for(s64 j = i + 1; j < stat_count; ++j) {
            if(stats[j].code > stats[i].code || (stats[j].code == stats[i].code && stats[j].file_count > stats[i].file_count)) {
                Stats tmp = stats[i];
                stats[i] = stats[j];
                stats[j] = tmp;
            }
        }
    }

    if(set_common_prefix) {
        set_initial_common_prefix(cloc, stats[0].ident);
        
        for(s64 i = 1; i < stat_count; ++i) {
            adapt_common_prefix(cloc, stats[i].ident);
        }
    }
}



/* ----------------------------------------------- Entry Point ----------------------------------------------- */

int main(int argc, char *argv[]) {
    Hardware_Time start = os_get_hardware_time();

    //
    // Set up the global instance
    //
    Cloc cloc = { 0 };
    create_arena(&cloc.perm, 1024 * 1024);
    create_arena(&cloc.scratch, 1024 * 1024);
    
    {

#define EXPECT_ADDITIONAL_ARG() if(i + 1 >= argc || argv[i + 1][0] == '-') { \
            printf("[ERROR]: The option '%s' expects an additional argument.\n", argv[i]); \
            cloc.cli_valid = false;                                     \
            i += 1;                                                     \
            continue;                                                   \
        }

        cloc.cli_valid   = true;
        cloc.output_mode = OUTPUT_By_Language;
        cloc.no_jobs     = false;
        
        //
        // Do the argument parsing in two stages, so that the order in which arguments and file paths are
        // specified doesn't matter.
        //
        String_List *filepaths = NULL;

        for(int i = 1; i < argc;) {
            char *argument = argv[i];

            if(argument[0] != '-') {
                filepaths = append_string_list(&cloc.scratch, filepaths, argument);
                ++i;
            } else if(strcmp(argument, "--by-lang") == 0) {
                cloc.output_mode = OUTPUT_By_Language;
                ++i;
            } else if(strcmp(argument, "--by-file") == 0) {
                cloc.output_mode = OUTPUT_By_File;
                ++i;
            } else if(strcmp(argument, "--no-jobs") == 0) {
                cloc.no_jobs = true;
                ++i;
            } else if(strcmp(argument, "--exclude-dir") == 0) {
                EXPECT_ADDITIONAL_ARG();
                cloc.excluded_directories = append_string_list(&cloc.perm, cloc.excluded_directories, argv[i + 1]);
                i += 2;
            } else {
                printf("[ERROR]: Unrecognized command line option '%s'.\n", argument);
                cloc.cli_valid = false;
                ++i;
            }
        }

        for(String_List *filepath = filepaths; filepath; filepath = filepath->next) {
            //
            // Register new files to parse
            //
            OS_Path_Kind path_kind = os_resolve_path_kind(filepath->content);
            switch(path_kind) {
            case OS_PATH_Is_File:
                register_file_to_parse(&cloc, filepath->content);
                break;

            case OS_PATH_Is_Directory:
                register_directory_to_parse(&cloc, filepath->content);
                break;

            case OS_PATH_Non_Existent:
                printf("[ERROR]: The file path '%s' doesn't exist.\n", filepath->content);
                cloc.cli_valid = false;
                break;
            }
        }
        
        if(cloc.cli_valid && cloc.first_file == NULL) {
            printf("[ERROR]: Please specify at least one source file to cloc.\n");
            cloc.cli_valid = false;
        }
        
#undef EXPECT_ADDITIONAL_ARG
    }
    
    if(cloc.cli_valid) {
        //
        // Set up and spawn the thread workers
        //
        s64 cpu_cores = os_get_hardware_thread_count();
        cloc.active_workers = cloc.no_jobs ? 1 : min(cpu_cores, cloc.file_count);
        
        for(int i = 0; i < cloc.active_workers; ++i) {
            cloc.workers[i].cloc = &cloc;
            cloc.workers[i].pid = os_spawn_thread((int(*)(void *)) worker_thread, &cloc.workers[i]);
        }
        
        //
        // Wait for all thread workers to complete
        //
        for(int i = 0; i < cloc.active_workers; ++i) {
            os_join_thread(cloc.workers[i].pid);
        }
        
        //
        // Finalize the result
        //
        print_separator_line(&cloc, CLOC_VERSION_STRING);
        print_table_header_line(&cloc);
        print_separator_line(&cloc, "");
        
        Stats sum_stats = { 0 };
        sum_stats.ident = "SUM:";

        switch(cloc.output_mode) {
        case OUTPUT_By_File: {
            Stats *sorted_stats = push_arena(&cloc.scratch, cloc.file_count * sizeof(Stats));
            memset(sorted_stats, 0, cloc.file_count * sizeof(Stats));

            s64 index = 0;
            for(File *file = cloc.first_file; file != NULL; file = file->next) {
                combine_stats(&sum_stats, &file->stats);
                sorted_stats[index] = file->stats;
                ++index;
            }

            prepare_stats(&cloc, sorted_stats, cloc.file_count, true);
            
            for(s64 i = 0; i < cloc.file_count; ++i) {
                print_table_entry_line(&cloc, &sorted_stats[i], false);
            }
        } break;

        case OUTPUT_By_Language: {
            Stats sorted_stats[LANGUAGE_COUNT];
            memset(&sorted_stats, 0, LANGUAGE_COUNT * sizeof(Stats));

            for(s64 i = 0; i < LANGUAGE_COUNT; ++i) {
                sorted_stats[i].ident = LANGUAGE_STRINGS[i];
            }

            for(File *file = cloc.first_file; file != NULL; file = file->next) {
                combine_stats(&sum_stats, &file->stats);
                combine_stats(&sorted_stats[file->language], &file->stats);
            }

            prepare_stats(&cloc, sorted_stats, LANGUAGE_COUNT, false);
            
            for(s64 i = 0; i < LANGUAGE_COUNT; ++i) {
                if(sorted_stats[i].file_count > 0) print_table_entry_line(&cloc, &sorted_stats[i], true);
            }
        } break;
        }
        
        if(sum_stats.file_count > 1) {
            cloc.common_prefix = NULL;
            cloc.common_prefix_length = 0;
            print_separator_line(&cloc, "");
            print_table_entry_line(&cloc, &sum_stats, cloc.output_mode != OUTPUT_By_File);
        }

        Hardware_Time end = os_get_hardware_time();
        f64 seconds   = os_convert_hardware_time_to_seconds(end - start);
        f64 lps       = (sum_stats.blank + sum_stats.comment + sum_stats.code) / seconds;
        f64 megabytes = os_get_working_set_size() / 1000000;
        print_separator_line(&cloc, aprint(&cloc.scratch, "%.2fs // %" PRId64 " l/s // %.1fmb", seconds, (s64) lps, megabytes));
    }

    destroy_arena(&cloc.perm);
    destroy_arena(&cloc.scratch);
    
    return cloc.cli_valid ? 0 : -1;
}


/*
 TODO:
 - [x] Port to linux
 - [x] Add a lines / second calculation
 - [x] Make a scratch arena for temporary file path conversion, so that hopefully we can cloc all of C:/source
 - [ ] Implement an assembly parser
 - [ ] Support exclusion of directories
*/
