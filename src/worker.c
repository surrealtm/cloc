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
    Line_Result current_line;
    char previous_character;
    b8 inside_single_line_comment;
    b8 inside_multiline_comment;
    b8 only_char_in_this_line_was_slash;
} C_Parser;

thread_local C_Parser c_parser;

static
void c_reset_parser(C_Parser *parser) {
    parser->current_line               = LINE_RESULT_Blank;
    parser->previous_character         = 0;
    parser->inside_single_line_comment = false;
    parser->inside_multiline_comment   = false;
    parser->only_char_in_this_line_was_slash = false;
}

static
void c_eat_character(C_Parser *parser, char character) {
    if(character == '/' && parser->previous_character == '/') {
        if(parser->current_line == LINE_RESULT_Blank || parser->only_char_in_this_line_was_slash) parser->current_line = LINE_RESULT_Comment; // If this line already contained code, then we count it as code and not comment
        parser->inside_single_line_comment = true;
    } else if(character == '*' && parser->previous_character == '/') {
        if(parser->current_line == LINE_RESULT_Blank || parser->only_char_in_this_line_was_slash) parser->current_line = LINE_RESULT_Comment; // If this line already contained code, then we count it as code and not comment
        parser->inside_multiline_comment = true;
    } else if(character == '/' && parser->previous_character == '*') {
        parser->inside_multiline_comment = false;
    } else if(character > 32 && !parser->inside_single_line_comment && !parser->inside_multiline_comment) {
        // When encountering a '/', we don't know yet if this is code or the start of a comment.
        // Therefore, for now we assume that this is code, and then if we encounter the start of a comment
        // in the next character, we change from code to comment.
        parser->only_char_in_this_line_was_slash = character == '/' && parser->current_line == LINE_RESULT_Blank;
        parser->current_line = LINE_RESULT_Code;
    } else if(character > 32 && parser->inside_multiline_comment && parser->current_line == LINE_RESULT_Blank) {
        parser->current_line = LINE_RESULT_Comment;
    }
    
    parser->previous_character = character;
}

static
Line_Result c_finish_line(C_Parser *parser) {
    Line_Result result = parser->current_line;
    parser->current_line                     = LINE_RESULT_Blank;
    parser->inside_single_line_comment       = false;
    parser->only_char_in_this_line_was_slash = false;
    parser->previous_character               = '\n';
    return result;
}

typedef struct Jai_Parser {
    Line_Result current_line;
    char previous_character;
    b8 inside_single_line_comment;
    s64 multiline_comment_depth;
    b8 only_char_in_this_line_was_slash;
} Jai_Parser;

thread_local Jai_Parser jai_parser;

static
void jai_reset_parser(Jai_Parser *parser) {
    parser->current_line                     = LINE_RESULT_Blank;
    parser->previous_character               = 0;
    parser->inside_single_line_comment       = false;
    parser->multiline_comment_depth          = 0;
    parser->only_char_in_this_line_was_slash = false;
}

static
void jai_eat_character(Jai_Parser *parser, char character) {
    if(character == '/' && parser->previous_character == '/') {
        if(parser->current_line == LINE_RESULT_Blank || parser->only_char_in_this_line_was_slash) parser->current_line = LINE_RESULT_Comment; // If this line already contained code, then we count it as code and not comment
        parser->inside_single_line_comment = true;
    } else if(character == '*' && parser->previous_character == '/') {
        if(parser->current_line == LINE_RESULT_Blank || parser->only_char_in_this_line_was_slash) parser->current_line = LINE_RESULT_Comment; // If this line already contained code, then we count it as code and not comment
        ++parser->multiline_comment_depth;
    } else if(character == '/' && parser->previous_character == '*') {
        --parser->multiline_comment_depth;
    } else if(character > 32 && !parser->inside_single_line_comment && !parser->multiline_comment_depth) {
        // When encountering a '/', we don't know yet if this is code or the start of a comment.
        // Therefore, for now we assume that this is code, and then if we encounter the start of a comment
        // in the next character, we change from code to comment.
        parser->only_char_in_this_line_was_slash = character == '/' && parser->current_line == LINE_RESULT_Blank;
        parser->current_line = LINE_RESULT_Code;
    } else if(character > 32 && parser->multiline_comment_depth && parser->current_line == LINE_RESULT_Blank) {
        parser->current_line = LINE_RESULT_Comment;
    }
    
    parser->previous_character = character;
}

static
Line_Result jai_finish_line(Jai_Parser *parser) {
    Line_Result result = parser->current_line;
    parser->current_line                     = LINE_RESULT_Blank;
    parser->inside_single_line_comment       = false;
    parser->only_char_in_this_line_was_slash = false;
    parser->previous_character               = '\n';
    return result;
}


/* -------------------------------------------------- Worker -------------------------------------------------- */

static inline
void register_line(Worker *worker, File *file, Parser *parser) {
    Line_Result result = parser->finish_line(parser->user_data);
    switch(result) {
    case LINE_RESULT_Blank:   ++file->stats.blank; break;
    case LINE_RESULT_Comment: ++file->stats.comment; break;
    case LINE_RESULT_Code:    ++file->stats.code; break;
    }
}

int worker_thread(Worker *worker) {
    worker->file_buffer = malloc(FILE_BUFFER_SIZE);

    //
    // Set up the thread local parser table
    //
    Parser PARSER_TABLE[LANGUAGE_COUNT] = {
        { &c_parser,   (Reset) &c_reset_parser,  (Eat_Character) c_eat_character,   (Finish_Line) c_finish_line }, // C
        { &c_parser,   (Reset) &c_reset_parser,  (Eat_Character) c_eat_character,   (Finish_Line) c_finish_line }, // C Header
        { &c_parser,   (Reset) &c_reset_parser,  (Eat_Character) c_eat_character,   (Finish_Line) c_finish_line }, // Cpp
        { &jai_parser, (Reset) jai_reset_parser, (Eat_Character) jai_eat_character, (Finish_Line) jai_finish_line }, // Jai
    };

    File *file;
    while((file = get_next_file_to_parse(worker->cloc))) {
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
        s64 chunk_size = 0;
        
        while(offset_in_file < file_size) {
            //
            // Handle one chunk of the file
            //
            chunk_size = min(FILE_BUFFER_SIZE, file_size - offset_in_file);
            chunk_size = os_read_file(handle, worker->file_buffer, offset_in_file, chunk_size);
            
            for(s64 i = 0; i < chunk_size; ++i) {
                char character = worker->file_buffer[i];

                switch(character) {
                case '\r': break; // Ignore
                case '\n': register_line(worker, file, parser); break;
                default: parser->eat_character(parser->user_data, character); break;
                }
            }
        
            offset_in_file += chunk_size;
        }

        if(chunk_size > 0 && worker->file_buffer[chunk_size - 1] != '\n') register_line(worker, file, parser);
        
        os_close_file(handle);
    }
    return 0;
}
