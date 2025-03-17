// --- C Includes ---
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

// --- Local Headers ---
#include "os.h"
#include "worker.h"
#include "cloc.h"

// --- Local Sources ---
#include "worker.c"

#if WIN32
# include "win32.c"
#endif


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



/* ----------------------------------------------- Entry Point ----------------------------------------------- */

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
    create_string_builder(&builder, &cloc->arena);
    append_string(&builder, directory_path);
    if(directory_path[directory_path_length - 1] != '/' && directory_path[directory_path_length - 1] != '\\')
        append_char(&builder, '/');
    append_string(&builder, file_path);
    
    return finish_string_builder(&builder);
}

void register_file_to_parse(Cloc *cloc, char *file_path) {
    // @Incomplete: Ignore file paths with unrecognized extensions
    // @Incomplete: Set the language depending on the file extension
    
    File *entry      = push_arena(&cloc->arena, sizeof(File));
    entry->next      = cloc->first_file;
    entry->file_path = os_make_absolute_path(&cloc->arena, file_path);
    entry->stats.blank      = 0;
    entry->stats.comment    = 0;
    entry->stats.code       = 0;
    entry->stats.file_count = 1;
    cloc->first_file = entry;
    cloc->next_file  = entry;
    ++cloc->file_count;

    if(cloc->common_prefix) {
        //
        // Find the common prefix between this new file and the stored prefix.
        // If this new file doesn't share the complete stored prefix, we shorten the stored prefix
        // until all files can share this prefix again.
        //
        s64 file_path_length = strlen(entry->file_path);
        s64 max_length = min(cloc->common_prefix_length, file_path_length);
        s64 index = 0;
        
        while(index < max_length && entry->file_path[index] == cloc->common_prefix[index]) {
            if(entry->file_path[index] == '\\') {
                cloc->common_prefix_length = index + 1;
            } else if(entry->file_path[index] == '/') {
                cloc->common_prefix_length = index + 1;
            }
            ++index;
        }
    } else {
        cloc->common_prefix = entry->file_path;
        cloc->common_prefix_length = strlen(entry->file_path);
        while(cloc->common_prefix_length > 0 && cloc->common_prefix[cloc->common_prefix_length - 1] != '/' && cloc->common_prefix[cloc->common_prefix_length - 1] != '\\') {
            --cloc->common_prefix_length;
        }
    }
}

void register_directory_to_parse(Cloc *cloc, char *directory_path) {
    File_Iterator iterator = find_first_file(&cloc->arena, directory_path);
    
    while(iterator.valid) {
        if(strcmp(iterator.path, ".") == 0 || strcmp(iterator.path, "..") == 0) {
            // Ignore these paths
        } else if(iterator.kind == OS_PATH_Is_Directory) {
            register_directory_to_parse(cloc, combine_file_paths(cloc, directory_path, iterator.path));
        } else if(iterator.kind == OS_PATH_Is_File) {
            register_file_to_parse(cloc, combine_file_paths(cloc, directory_path, iterator.path));
        }

        find_next_file(&cloc->arena, &iterator);
    }
}

static
void print_separator_line(Cloc *cloc, const char *content) {
    const char DELIMITER_CHAR = '-';

    s64 content_length = strlen(content);
    if(content_length > 0) {
        String_Builder builder;
        create_string_builder(&builder, &cloc->arena);
    
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
    create_string_builder(&builder, &cloc->arena);

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
void print_table_entry_line(Cloc *cloc, const char *ident, Stats stats, b8 include_file_count) {
    String_Builder builder;
    create_string_builder(&builder, &cloc->arena);
    append_string_with_max_length(&builder, ident, (include_file_count ? FILE_COUNT_COLUMN_OFFSET : EMPTY_LINES_COLUMN_OFFSET) - 3);
    if(include_file_count)
        append_right_justified_integer_at_offset(&builder, stats.file_count,    ' ', FILE_COUNT_COLUMN_OFFSET);
    append_right_justified_integer_at_offset(&builder, stats.blank,         ' ', EMPTY_LINES_COLUMN_OFFSET);
    append_right_justified_integer_at_offset(&builder, stats.comment,       ' ', COMMENT_LINES_COLUMN_OFFSET);
    append_right_justified_integer_at_offset(&builder, stats.code,          ' ', CODE_LINES_COLUMN_OFFSET);
    print_string_builder_as_line(&builder);    
}

int main(int argc, char *argv[]) {
    Hardware_Time start = os_get_hardware_time();

    //
    // Set up the global instance
    //
    Cloc cloc = { 0 };
    create_arena(&cloc.arena, 1024 * 1024);
    
    {
        cloc.cli_valid = true;
        cloc.output_mode = OUTPUT_By_Language;
        
        for(int i = 1; i < argc; ++i) {
            char *argument = argv[i];

            // @Incomplete: Normalize the input paths

            if(argument[0] != '-') {
                //
                // Register new files to parse
                //
                OS_Path_Kind path_kind = os_resolve_path_kind(argument);
                switch(path_kind) {
                case OS_PATH_Is_File:
                    register_file_to_parse(&cloc, argument);
                    break;

                case OS_PATH_Is_Directory:
                    register_directory_to_parse(&cloc, argument);
                    break;

                case OS_PATH_Non_Existent:
                    printf("[ERROR]: The file path '%s' doesn't exist.\n", argument);
                    cloc.cli_valid = false;
                    break;
                }
            } else if(strcmp(argument, "--by-lang") == 0) {
                cloc.output_mode = OUTPUT_By_Language;
            } else if(strcmp(argument, "--by-file") == 0) {
                cloc.output_mode = OUTPUT_By_File;
            } else {
                printf("[ERROR]: Unrecognized command line option '%s'.\n", argument);
                cloc.cli_valid = false;
            }
        }

        if(cloc.cli_valid && cloc.first_file == NULL) {
            printf("[ERROR]: Please specify at least one source file to cloc.\n");
            cloc.cli_valid = false;
        }
    }

    printf("Prefix: '%.*s'\n", cloc.common_prefix_length, cloc.common_prefix);
    
    if(cloc.cli_valid) {
        //
        // Set up and spawn the thread workers
        //
        s64 cpu_cores = os_get_hardware_thread_count();
        cloc.active_workers = min(cpu_cores, cloc.file_count); 
        
        for(int i = 0; i < cloc.active_workers; ++i) {
            cloc.workers[i].cloc = &cloc;
            cloc.workers[i].pid = os_spawn_thread(worker_thread, &cloc.workers[i]);
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
        
        // @Incomplete: Sort the results by their code line count

        switch(cloc.output_mode) {
        case OUTPUT_By_File: {
            for(File *file = cloc.first_file; file != NULL; file = file->next) {
                print_table_entry_line(&cloc, &file->file_path[cloc.common_prefix_length], file->stats, false);
                combine_stats(&sum_stats, &file->stats);
            }
        } break;

        case OUTPUT_By_Language: {
            Stats languages[LANGUAGE_COUNT];
            memset(&languages, 0, sizeof(languages));

            for(File *file = cloc.first_file; file != NULL; file = file->next) {
                combine_stats(&sum_stats, &file->stats);
                combine_stats(&languages[file->language], &file->stats);
            }

            for(s64 i = 0; i < LANGUAGE_COUNT; ++i) {
                if(languages[i].file_count > 0)
                    print_table_entry_line(&cloc, LANGUAGE_STRINGS[i], languages[i], true);
            }
        } break;
        }
        
        if(sum_stats.file_count > 1) {
            print_separator_line(&cloc, "");
            print_table_entry_line(&cloc, "SUM:", sum_stats, cloc.output_mode != OUTPUT_By_File);
        }
        
        Hardware_Time end = os_get_hardware_time();
        f64 seconds   = os_convert_hardware_time_to_seconds(end - start);
        f64 megabytes = os_get_working_set_size() / 1000000;
        print_separator_line(&cloc, aprint(&cloc.arena, "%.2fs / %.1fmb", seconds, megabytes));
    }

    return cloc.cli_valid ? 0 : -1;
}
