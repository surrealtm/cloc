/* ------------------------------------------------ Parser API ------------------------------------------------ */

typedef enum Line_Result {
    LINE_RESULT_Blank,
    LINE_RESULT_Comment,
    LINE_RESULT_Code,
} Line_Result;

typedef void(*Reset)(void *user_data);
typedef void(*Eat_Character)(void *user_data, char character);
typedef Line_Result(*Finish_Line)(void *user_data);

typedef struct Parser {
    void *user_data;
    Reset reset;
    Eat_Character eat_character;
    Finish_Line finish_line;
} Parser;



/* ------------------------------------------ Parser Implementation ------------------------------------------ */

typedef struct C_Parser {
    b8 current_line_is_blank;
    b8 inside_multiline_comment;
} C_Parser;

thread_local C_Parser c_parser;

static
void c_reset_parser(C_Parser *parser) {
    
}

static
void c_eat_character(C_Parser *parser, char character) {

}

static
Line_Result c_finish_line(C_Parser *parser) {
    return LINE_RESULT_Code;
}



/* -------------------------------------------------- Worker -------------------------------------------------- */

int worker_thread(Worker *worker) {
    worker->file_buffer = malloc(FILE_BUFFER_SIZE);

    //
    // Set up the thread local parser table
    //
    Parser PARSER_TABLE[LANGUAGE_COUNT] = {
        { &c_parser, c_reset_parser, c_eat_character, c_finish_line },
        { &c_parser, c_reset_parser, c_eat_character, c_finish_line },
        { &c_parser, c_reset_parser, c_eat_character, c_finish_line },
    };

    File *file;
    while(file = get_next_file_to_parse(worker->cloc)) {
        //
        // Get the appropriate parser for this file
        //
        Parser *parser = &PARSER_TABLE[file->language];
        parser->reset(parser->user_data);
        
        //
        // Handle one file
        //
        File_Handle handle = os_open_file(file->file_path);

        s64 file_size = os_get_file_size(handle);
        s64 offset_in_file = 0;
    
        while(offset_in_file < file_size) {
            //
            // Handle one chunk of the file
            //
            s64 chunk_size = min(FILE_BUFFER_SIZE, file_size - offset_in_file);
            chunk_size = os_read_file(handle, worker->file_buffer, offset_in_file, chunk_size);

            for(s64 i = 0; i < chunk_size; ++i) {
                char character = worker->file_buffer[i];

                switch(character) {
                case '\r': break; // Ignore

                case '\n': {
                    Line_Result result = parser->finish_line(parser->user_data);
                    switch(result) {
                    case LINE_RESULT_Blank: ++file->stats.blank; break;
                    case LINE_RESULT_Comment: ++file->stats.comment; break;
                    case LINE_RESULT_Code: ++file->stats.code; break;
                    }
                } break;

                default:
                    parser->eat_character(parser->user_data, character);
                    break;
                }
            }
        
            offset_in_file += chunk_size;
        }

        os_close_file(handle);
    }
    return 0;
}
