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

void append_char(String_Builder *builder, char data) {
    char *pointer = push_string_builder(builder, 1);
    *pointer = data;
}

void append_repeated_char(String_Builder *builder, char data, s64 n) {
    char *pointer = push_string_builder(builder, n);
    memset(pointer, data, n);
}

void print_string_builder(String_Builder *builder) {
    printf("%.*s", (int) builder->size_in_characters, builder->pointer);
}

void print_string_builder_as_line(String_Builder *builder) {
    printf("%.*s\n", (int) builder->size_in_characters, builder->pointer);
}



/* ----------------------------------------------- Entry Point ----------------------------------------------- */

void register_file_to_parse(Cloc *cloc, char *file_path) {
    File *entry      = push_arena(&cloc->arena, sizeof(File));
    entry->next      = cloc->first_file;
    entry->file_path = file_path;
    cloc->first_file = entry;
    cloc->next_file  = entry;
    ++cloc->file_count;
}

void register_directory_to_parse(Cloc *cloc, char *directory_path) {
    File_Iterator iterator = find_first_file(&cloc->arena, directory_path);
    
    while(iterator.valid) {
        if(strcmp(iterator.path, ".") == 0 || strcmp(iterator.path, "..") == 0) {
            // Ignore these paths
        } else if(iterator.kind == OS_PATH_Is_Directory) {
            register_directory_to_parse(cloc, iterator.path);
        } else if(iterator.kind == OS_PATH_Is_File) {
            register_file_to_parse(cloc, iterator.path);
        }

        find_next_file(&cloc->arena, &iterator);
    }
}

static
void print_separator_line(Cloc *cloc, const char *content) {
    String_Builder builder;
    create_string_builder(&builder, &cloc->arena);

    s64 total_stars = (OUTPUT_LINE_WIDTH - strlen(content) - 2);
    s64 lhs_stars = total_stars / 2;
    s64 rhs_stars = total_stars / 2 + total_stars % 2;

    append_repeated_char(&builder, '*', lhs_stars);
    append_char(&builder, ' ');
    append_string(&builder, content);
    append_char(&builder, ' ');
    append_repeated_char(&builder, '*', rhs_stars);
    
    print_string_builder_as_line(&builder);
}

static
void print_output_line(Cloc *cloc, char *identifier, s64 lines) {
    
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
        cloc.output_mode = OUTPUT_Total;
        
        for(int i = 1; i < argc; ++i) {
            char *argument = argv[i];

            if(argument[0] != '-') {
                //
                // Register new files to parse
                //
                OS_Path_Kind path_kind = os_resolve_path_kind(argument);
                switch(path_kind) {
                case OS_PATH_Is_Directory:
                    register_directory_to_parse(&cloc, argument);
                    break;

                case OS_PATH_Is_File:
                    register_file_to_parse(&cloc, argument);
                    break;

                case OS_PATH_Non_Existent:
                    printf("[ERROR]: The file path '%s' doesn't exist.\n", argument);
                    cloc.cli_valid = false;
                    break;
                }
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

        switch(cloc.output_mode) {
        case OUTPUT_By_File: {
            for(File *file = cloc.first_file; file != NULL; file = file->next) {
                printf(" > %s: %" PRId64 " lines\n", file->file_path, file->lines);
            }
        } break;

        case OUTPUT_Total: {
            
        } break;
        }

        Hardware_Time end = os_get_hardware_time();
        f64 seconds = os_convert_hardware_time_to_seconds(end - start);
        print_separator_line(&cloc, aprint(&cloc.arena, "%.2fs", seconds));
    }

    return cloc.cli_valid ? 0 : -1;
}
