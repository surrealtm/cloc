thread_local char posix_string_buffer[PATH_MAX];

OS_Path_Kind os_resolve_path_kind(char *path) {
    struct stat filestat;
    if(stat(path, &filestat) == 0) {
        if(filestat.st_mode & S_IFREG) {
            return OS_PATH_Is_File;
        } else if(filestat.st_mode & S_IFDIR) {
            return OS_PATH_Is_Directory;
        } else {
            return OS_PATH_Non_Existent;
        }
    } else {
        return OS_PATH_Non_Existent;
    }
}

char *os_make_absolute_path(Arena *arena, char *path) {
    char *pointer = realpath(path, posix_string_buffer);
    return push_string(arena, pointer);
}

File_Handle os_open_file(char *path) {
    return open(path, O_RDONLY);
}

s64 os_get_file_size(File_Handle handle) {
    struct stat filestat;
    if(fstat(handle, &filestat) == 0) {
        return filestat.st_size;
    } else {
        return 0;
    }
}

s64 os_read_file(File_Handle handle, char *dst, s64 offset, s64 size) {
    lseek(handle, offset, SEEK_SET);
    return read(handle, dst, size);
}

void os_close_file(File_Handle handle) {
    close(handle);
}



File_Iterator find_first_file(Arena *arena, char *directory_path) {
    File_Iterator iterator;
    iterator.native_handle = opendir(directory_path);
    find_next_file(arena, &iterator);
    return iterator;
}

void find_next_file(Arena *arena, File_Iterator *iterator) {
    iterator->valid = false;

    struct dirent *entry = NULL;
    while(!iterator->valid && (entry = readdir(iterator->native_handle))) {
        switch(entry->d_type) {
        case DT_DIR:
            iterator->path  = entry->d_name;
            iterator->kind  = OS_PATH_Is_Directory;
            iterator->valid = true;
            break;

        case DT_REG:
            iterator->path = entry->d_name;
            iterator->kind = OS_PATH_Is_File;
            iterator->valid = true;
            break;
        }
    }
}

void close_file_iterator(File_Iterator *iterator) {
    closedir(iterator->native_handle);
    iterator->native_handle = NULL;
    iterator->valid = false;
}



s64 os_get_hardware_thread_count() {
    return sysconf(_SC_NPROCESSORS_ONLN);
}

Pid os_spawn_thread(int (*procedure)(void *), void *argument) {
    Pid pid;
    pthread_create(&pid, NULL, (void *) procedure, argument);
    return pid;
}

void os_join_thread(Pid pid) {
    void *return_value;
    pthread_join(pid, &return_value);
}

void *os_compare_and_swap(void *volatile *dst, void *exchange, void *comparand) {
    return __sync_val_compare_and_swap(dst, comparand, exchange);
}

Hardware_Time os_get_hardware_time() {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (Hardware_Time) (time.tv_sec * 1e9 + time.tv_nsec);
}

f64 os_convert_hardware_time_to_seconds(Hardware_Time time) {
    return (f64) time / 1e9;
}

s64 os_get_working_set_size() {
    //
    // Linux doesn't actually calculate the "working-set-size", but instead the "resident-set-size". The latter
    // one is the number of kilobytes of pages that actually reside in main memory, which seems close enough for
    // a rough performance overview...
    // Note: This only gives us the MAX Rss, not the current one...
    //
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss * 1000;
}
