// UCLA CS 111 Lab 1 command reading

// Copyright 2012-2014 Paul Eggert.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "command.h"
#include "command-internals.h"
#include "alloc.h"

#include <error.h>

/* FIXME: You may need to add #include directives, macro definitions,
   static function definitions, etc.  */

/* FIXME: Define the type 'struct command_stream' here.  This should
   complete the incomplete type declaration in command.h.  */

char*
read_bytes(int (*get_next_byte) (void *),
        void *get_next_byte_argument) {
    size_t n, buf_size;
    char next_byte;

    buf_size = 1024;
    n = 0;

    char *buf = (char *) checked_malloc(sizeof (char) * buf_size);

    char next_byte;
    ;

    while ((next_byte = get_next_byte(get_next_byte_argument)) != EOF) {
        //case where next_byte is a comment - ignore the rest of the line
        if (next_byte == '#') {
            next_byte = get_next_byte(get_next_byte_argument);
            while (next)byte != '\n' && next_byte != EOF)
                next_byte = get_next_byte(get_next_byte_argument);
        }

        if (next_byte != EOF) {
            buf[n] = next_byte;
            n++;

            if (n == buf_size)
                buf = checked_grow_alloc(buf, &buf_size);
        }
    }

    if (n < buf_size - 1) {
        buf[n] = '\0';
    } else {
        buf = (char *) checked_grow_alloc(buf, &buf_size);
        buf[n] = '\0';
    }

    return buf;
}

token_stream_t
tokenize_bytes(char* buf) {



}

command_stream_t
make_command_stream(int (*get_next_byte) (void *),
        void *get_next_byte_argument) {

    char* buf = read_bytes(get_next_byte, get_next_byte_argument);

    token_stream_t ts = tokenize_bytes(buf);


    error(1, 0, "command reading not yet implemented");
    return 0;
}

void validate_tokens(token_stream_t tstream) {
    enum token_type next
            enum token_type prev;
    token_stream_t tCur;
    token_stream_t tNext;
    token_stream_t tPrev;

    tCur = tstream;
    tNext = tPrev = NULL;

    //counting necessary to validate end of if statements, commands, etc. 
    int numParens = 0;
    int numIf = 0;
    int numDone = 0;

    while (tCur != NULL) {
        tNext = tCur->next;
        //    if (tNext == NULL)
        //      next = UNKNOWN_TOKEN;    
        //    else
        //      next = tNext->m_token.type;

        next = (tNext == NULL) ? tNext->m_token.type : UNKNOWN_TOKEN;

        tPrev = tCur->prev;
        //    if (tPrev == NULL)
        //      prev = UNKNOWN_TOKEN; 
        //    else
        //      prev = tPrev->m_token.type;

        next = (tPrev == NULL) ? tPrev->m_token.type : UNKNOWN_TOKEN;

        switch (tCur->m_token.type) {
            case WORD_TOKEN:
                if (next == IF_TOKEN || next == FI_TOKEN || next == THEN_TOKEN || next == ELSE_TOKEN || next == DO_TOKEN || next == DONE_TOKEN || next == WHILE_TOKEN || next == UNTIL_TOKEN)
                    next = WORD_TOKEN;
                break;

            case SEMICOLON_TOKEN:
                if (next == SEMICOLON_TOKEN) {
                    fprintf(stderr, "Error in validate() at line %i: SEMICOLON_TOKEN cannot be followed by another semicolon\n", tCur->m_token.line_num);
                    exit(1);
                }

                if (tCur == tstream) {
                    fprintf(stderr, "Error in validate() at line %i: SEMICOLON_TOKEN cannot be the first token\n", tCur->m_token.line_num);
                    exit(1);
                }
                break;

            case PIPE_TOKEN:
                if (tCur == tstream) {
                    fprintf(stderr, "Error in validate() at line %i: PIPE op can't be first token\n", tCur->m_token.line_num);
                    exit(1);
                }
                if (tCur->m_token.type == next) {
                    fprintf(stderr, "Error in validate() at line %i: PIPE op can't have repeated token of type %i\n", tCur->m_token.line_num, next);
                    exit(1);
                }
                if (next == SEMICOLON_TOKEN) {
                    fprintf(stderr, "Error in validate() at line %i: PIPE op can't be followed by another PIPE op\n", tCur->m_token.line_num);
                    exit(1);
                }
                break;

            case LEFT_PAREN_TOKEN:
                if (next == RIGHT_PAREN_TOKEN) {
                    fprintf(stderr, "Error in validate() at line %i: shouldn't have an empty subcommand\n", tCur->m_token.line_num);
                    exit(1);
                }
                numParens++;
                break;

            case RIGHT_PAREN_TOKEN:
                numParens--;
                break;

            case GREATER_TOKEN:
            case LESS_TOKEN:
                if (tNext == NULL || next != WORD_TOKEN) {
                    fprintf(stderr, "Error in validate() at line %i: expected word following redirection op\n", tCur->m_token.line_num);
                    exit(1);
                }
                break;

            case NEWLINE_TOKEN:
                if (tNext == NULL)
                    break;
                if (next == WORD_TOKEN || 
                    next == LEFT_PAREN_TOKEN || 
                    next == RIGHT_PAREN_TOKEN && 
                    ((numParens != 0 && prev != LEFT_PAREN_TOKEN) ||
                    (numIf != 0 && !(prev == IF_TOKEN ||
                    prev == THEN_TOKEN ||
                    prev == ELSE_TOKEN)) ||
                    (numDone != 0 && !(prev == WHILE_TOKEN ||
                    prev == UNTIL_TOKEN || prev == DO_TOKEN)))) {
                    
                        tCur->m_token.type = SEMICOLON_TOKEN;
                } else {
                    switch (next) {
                        case IF_TOKEN:
                        case FI_TOKEN:
                        case THEN_TOKEN:
                        case ELSE_TOKEN:
                        case LEFT_PAREN_TOKEN:
                        case RIGHT_PAREN_TOKEN:
                        case WORD_TOKEN:
                        case DO_TOKEN:
                        case DONE_TOKEN:
                        case WHILE_TOKEN:
                        case UNTIL_TOKEN:
                            break;
                        default:
                            fprintf(stderr, "Error in validate() at line %i: New lines can only appear before certain tokens\n", tCur->m_token.line_num);
                            exit(1);
                    }
                }
                break;

            case THEN_TOKEN:
            case ELSE_TOKEN:
                if (tNext == NULL || next == FI_TOKEN) {
                    fprintf(stderr, "Error in validate() at line %i: special words cannot have tokens of type %i right after/next token is null\n", tCur->m_token.line_num, next);
                    exit(1);
                }               
            case DO_TOKEN:
                if (prev == WORD_TOKEN) {
                    tCur->m_token.type = WORD_TOKEN;
                }
                if (prev != NEWLINE_TOKEN && prev != SEMICOLON_TOKEN) {
                    fprintf(stderr, "Error in validate() at line %i: special words must have a semicolon or newline after\n", tCur->m_token.line_num);
                    exit(1);
                }
                if (tCur->m_token.type == next) {
                    fprintf(stderr, "Error in validate() at line %i: cannot have repeated token of type %i\n", tCur->m_token.line_num, next);
                    exit(1);
                }
                if (tNext == NULL || next == SEMICOLON_TOKEN) {
                    fprintf(stderr, "Error in validate() at line %i: special words cannot have tokens of type %i right after/next token is null\n", tCur->m_token.line_num, next);
                    exit(1);
                }
                break;

            case DONE_TOKEN:
            case FI_TOKEN:
                if (next == WORD_TOKEN) {
                    fprintf(stderr, "Error in validate() at line %i: cannot have a word right after DONE, FI\n", tCur->m_token.line_num);
                    exit(1);
                }
            case IF_TOKEN:
            case WHILE_TOKEN:
            case UNTIL_TOKEN:
                if (next == SEMICOLON_TOKEN || tNext == NULL) {
                    fprintf(stderr, "Error in validate() at line %i: special words can't have tokens of type %i right after/next token is null\n",
                            tCur->m_token.line_num, next);
                    exit(1);
                }
                if (prev == WORD_TOKEN) {
                    tCur->m_token.type = WORD_TOKEN;
                }
                if (tCur->m_token.type == IF_TOKEN)
                    numIf++;
                else if (tCur->m_token.type == FI_TOKEN)
                    numIf--;
                else if (tCur->m_token.type == DONE_TOKEN)
                    numDone--;
                else if (tCur->m_token.type == WHILE_TOKEN)
                    numDone++;
                else if (tCur->m_token.type == UNTIL_TOKEN)
                    numDone++; 
                break;
            default:
                break;
        }
        tCur = tNext;
    } 

    if (numParens != 0 || numIf != 0 ||
            numDone != 0) {
        fprintf(stderr, "Error in validate(): Unmatched closing statements\n");
        exit(1);
    }

}

command_t
read_command_stream(command_stream_t s) {
    /* FIXME: Replace this with your implementation too.  */
    error(1, 0, "command reading not yet implemented");
    return 0;
}
