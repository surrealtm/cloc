struct Cloc;

typedef struct Worker {
    struct Cloc *cloc;
    Pid pid;
} Worker;

int worker_thread(Worker *worker);
