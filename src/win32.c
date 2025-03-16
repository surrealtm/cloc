thread_local char win32_string_buffer[MAX_PATH];

OS_Path_Kind os_resolve_path_kind(char *path) {
    DWORD result = GetFileAttributesA(path);

    if(result != INVALID_FILE_ATTRIBUTES) {
        return result & FILE_ATTRIBUTE_DIRECTORY ? OS_PATH_Is_Directory : OS_PATH_Is_File;
    } else {
        return OS_PATH_Non_Existent;
    }
}

File_Iterator find_first_file(Arena *arena, char *directory_path) {
    WIN32_FIND_DATAA file_data;

    sprintf(win32_string_buffer, "%s\\*", directory_path);
    
    File_Iterator iterator;
    iterator.native_handle = FindFirstFileA(win32_string_buffer, &file_data);
    iterator.valid = iterator.native_handle != INVALID_HANDLE_VALUE;

    if(iterator.valid) {
        iterator.path = push_string(arena, file_data.cFileName);
        iterator.kind = file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ? OS_PATH_Is_Directory : OS_PATH_Is_File;
    } else {
        iterator.native_handle = INVALID_HANDLE_VALUE;
        iterator.path = NULL;
        iterator.kind = OS_PATH_Non_Existent;
    }

    return iterator;
}

void find_next_file(Arena *arena, File_Iterator *iterator) {
    WIN32_FIND_DATAA file_data;
    iterator->valid = FindNextFile(iterator->native_handle, &file_data);
    
    if(iterator->valid) {
        iterator->path = push_string(arena, file_data.cFileName);
        iterator->kind = file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ? OS_PATH_Is_Directory : OS_PATH_Is_File;
    } else {
        iterator->native_handle = INVALID_HANDLE_VALUE;
        iterator->path = NULL;
        iterator->kind = OS_PATH_Non_Existent;
    }
}

void close_file_iterator(File_Iterator *iterator) {
    FindClose(iterator->native_handle);
    iterator->valid = false;
    iterator->native_handle = INVALID_HANDLE_VALUE;
    iterator->path = NULL;
    iterator->kind = OS_PATH_Non_Existent;
}
