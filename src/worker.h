typedef struct Worker {
    Cloc *cloc;
    Pid global_thread_handle;
} Worker;

void worker_thread(Worker *worker);
