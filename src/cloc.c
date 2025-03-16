// --- C Includes ---
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

// --- Local Headers ---
#include "os.h"
#include "cloc.h"
#include "worker.h"

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



/* ----------------------------------------------- Entry Point ----------------------------------------------- */

void register_file_to_parse(Cloc *cloc, char *file_path) {
    File *entry      = push_arena(&cloc->arena, sizeof(File));
    entry->next      = cloc->first_file;
    entry->file_path = file_path;
    cloc->first_file = entry;
    cloc->next_file  = entry;
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

int main(int argc, char *argv[]) {
    //
    // Set up the global instance
    //
    Cloc cloc = { 0 };
    create_arena(&cloc.arena, 1024 * 1024);

    {
        cloc.cli_valid = true;
        
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
        Worker workers[MAX_WORKERS];
        int active_workers = 12; // @Incomplete: Get the cpu core count from the OS

        for(int i = 0; i < active_workers; ++i) {
            workers[i].cloc = &cloc;
            // @Incomplete: Spawn the thread
        }
        
        //
        // Wait for all thread workers to complete
        //
        for(int i = 0; i < active_workers; ++i) {
            // @Incomplete: Join the thread
        }
        
        //
        // Finalize the result
        //
        // @Incomplete: Print all files or the sum of their results...
    }
    
    return cloc.cli_valid ? 0 : -1;
}
