struct Cloc;

#define FILE_BUFFER_SIZE 1024 * 1024

typedef struct Worker {
    struct Cloc *cloc;
    Pid pid;
    char *file_buffer;
} Worker;

int worker_thread(Worker *worker);
