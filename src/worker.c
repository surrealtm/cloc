static
void parse_one_file(Worker *worker, File *file) {
    file->stats.code = 2;
}

int worker_thread(Worker *worker) {
    File *file;
    while(file = get_next_file_to_parse(worker->cloc)) {
        parse_one_file(worker, file);
    }
    return 0;
}
