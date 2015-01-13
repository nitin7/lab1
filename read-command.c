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
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>


/* FIXME: You may need to add #include directives, macro definitions,
   static function definitions, etc.  */

/* FIXME: Define the type 'struct command_stream' here.  This should
   complete the incomplete type declaration in command.h.  */

token_stream_t tsStack = NULL; 
command_t *c_stack = NULL;
void ts_push(token_stream_t tStream);
enum token_type ts_peek();
token_stream_t ts_pop();
void c_push(command_t cmd, int *top, size_t *size);
command_t c_pop(int *top);
int stack_precedence(enum token_type type);
int stream_precedence(enum token_type type);

char* read_bytes(int (*get_next_byte) (void *),
        void *get_next_byte_argument) {
    size_t n, buf_size;
    char next_byte;

    buf_size = 1024;
    n = 0;

    char *buf = (char *) checked_malloc(sizeof (char) * buf_size);

    while ((next_byte = get_next_byte(get_next_byte_argument)) != EOF) {
        //case where next_byte is a comment - ignore the rest of the line
        if (next_byte == '#') {
            next_byte = get_next_byte(get_next_byte_argument);
            while (next_byte != '\n' && next_byte != EOF)
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

int check_if_word(char c) {
    if (isalnum(c) || (c == '!') || (c == '%') || (c == '!') || (c == '+') || (c == '-') || (c == ',') || (c == '.') || (c == '/') || (c == ':') || (c == '@') || (c == '^') || (c == '_'))
        return 1;

    return 0;
}

token_stream_t tokenize_bytes(char* buf) {
    enum token_type type;
    struct token_stream *head = NULL;
    struct token_stream *cur = head;

    //case where buffer empty
    int i = 0;
    int line = 1;
    char ch;
    while ((ch = buf[i]) != '\0') {
        switch (ch) {
                //comments check
            case '\n':
                line++;
                type = NEWLINE_TOKEN;
                char nextCh = buf[i + i];
                if (nextCh == '\n' && nextCh != '\0') {
                    i++;
                    continue;
                }
                break;
            case '\t':
                i++;
                continue;
                break;
            case ' ':
                i++;
                continue;
                break;
            case ';':
                type = SEMICOLON_TOKEN;
                break;
            case '|':
                type = PIPE_TOKEN;
                break;
            case '(':
                type = LEFT_PAREN_TOKEN;
                break;
            case ')':
                type = RIGHT_PAREN_TOKEN;
                break;
            case '>':
                type = GREATER_TOKEN;
                break;
            case '<':
                type = LESS_TOKEN;
                break;
            default:
                type = UNKNOWN_TOKEN;
        }

        int nChars = 0;
        int startChar = i;

        if (check_if_word(ch) == 1) {
            while (check_if_word(buf[nChars + 1 + i]) == 1)
                nChars++;
            type = WORD_TOKEN;
            i += nChars;
        }

        struct token_stream* ts = (struct token_stream *) checked_malloc(sizeof (struct token_stream));
        ts->m_token.type = type;
        ts->m_token.length = nChars;
        ts->m_token.line_num = line;
        //default initialize to null in command-internals.h
        ts->prev = NULL;
        ts->next = NULL;

        if (type == NEWLINE_TOKEN)
            ts->m_token.line_num--;
        else if (type == UNKNOWN_TOKEN) {
            ts->m_token.t_word = NULL;
            fprintf(stderr, "Error: found unknown token %c on line %i\n", ch, line);
            exit(1);
        } else if (type = WORD_TOKEN) {
            ts->m_token.t_word = (char *) checked_malloc(sizeof (char) * nChars + 1);
		int k;
            for (k = 0; k < nChars; k++)
                ts->m_token.t_word[k] = buf[startChar + k];

            ts->m_token.t_word[nChars] = '\0';

            if (strcmp(ts->m_token.t_word, "if") == 0) {
                ts->m_token.type = IF_TOKEN;
            } else if (strcmp(ts->m_token.t_word, "then") == 0) {
                ts->m_token.type = THEN_TOKEN;
            } else if (strcmp(ts->m_token.t_word, "else") == 0) {
                ts->m_token.type = ELSE_TOKEN;
            } else if (strcmp(ts->m_token.t_word, "fi") == 0) {
                ts->m_token.type = FI_TOKEN;
            } else if (strcmp(ts->m_token.t_word, "while") == 0) {
                ts->m_token.type = WHILE_TOKEN;
            } else if (strcmp(ts->m_token.t_word, "do") == 0) {
                ts->m_token.type = DO_TOKEN;
            } else if (strcmp(ts->m_token.t_word, "done") == 0) {
                ts->m_token.type = DONE_TOKEN;
            } else if (strcmp(ts->m_token.t_word, "until") == 0) {
                ts->m_token.type = UNTIL_TOKEN;
            }
        } else
            ts->m_token.t_word = NULL;

        if (cur != NULL) {
            ts->prev = cur;
            ts->next = NULL;
            cur->next = ts;
            cur = ts;
        } else {
            head = ts;
            cur = head;
        }
        i++;
    }
    return head;
}

void validate_tokens(token_stream_t tstream) {
    enum token_type next;
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

command_t new_command() {
    command_t item = (command_t) checked_malloc(sizeof (struct command));

    item->type = SIMPLE_COMMAND;
    item->status = -1;
    item->input = item->output = NULL;
    item->u.word = NULL;

    return item;
}

command_t combine_commands(command_t cmdA, command_t cmdB, token_stream_t tStream) {
    if (tStream == NULL) {
        fprintf(stderr, "Error in combine_commands(): attempting to pop from empty ts_stack\n");
        exit(1);
    }

    command_t combined = NULL;

    if (cmdA == NULL || cmdB == NULL) {
        char token_type[20];
        switch (tStream->m_token.type) {
            case 0: strcpy(token_type, "WORD_TOKEN");
                break;
            case 1: strcpy(token_type, "SEMICOLON_TOKEN");
                break;
            case 2: strcpy(token_type, "PIPE_TOKEN");
                break;
            case 3: strcpy(token_type, "LEFT_PAREN_TOKEN");
                break;
            case 4: strcpy(token_type, "RIGHT_PAREN_TOKEN");
                break;
            case 5: strcpy(token_type, "LESS_TOKEN");
                break;
            case 6: strcpy(token_type, "GREATER_TOKEN");
                break;
            case 7: strcpy(token_type, "IF_TOKEN");
                break;
            case 8: strcpy(token_type, "THEN_TOKEN");
                break;
            case 9: strcpy(token_type, "ELSE_TOKEN");
                break;
            case 10: strcpy(token_type, "FI_TOKEN");
                break;
            case 11: strcpy(token_type, "WHILE_TOKEN");
                break;
            case 12: strcpy(token_type, "DO_TOKEN");
                break;
            case 13: strcpy(token_type, "DONE_TOKEN");
                break;
            case 14: strcpy(token_type, "UNTIL_TOKEN");
                break;
            case 15: strcpy(token_type, "NEWLINE_TOKEN");
                break;
            case 16: strcpy(token_type, "UNKNOWN_TOKEN");
                break;
            case 17: strcpy(token_type, "UNKNOWN_TOKEN");
                break;
            default: strcpy(token_type, "ERROR in combine()");
                break;
        }
        fprintf(stderr,
                "Error in combine_commands(): Either cmd_A or cmd_B wrong\n");
        if (cmdA == NULL)
            fprintf(stderr, "Poop. cmd_A is NULL\n");
        else
            fprintf(stderr, "Crap. cmd_B is NULL\n");
        exit(1);
    }
    
    combined = new_command();
    combined->u.command[0] = cmdA;
    combined->u.command[1] = cmdB;

    switch (tStream->m_token.type) {
        case SEMICOLON_TOKEN: combined->type = SEQUENCE_COMMAND;
            break;
        case PIPE_TOKEN: combined->type = PIPE_COMMAND;
            break;
        case RIGHT_PAREN_TOKEN: combined->type = SUBSHELL_COMMAND;
            break;
        default:
            fprintf(stderr, "Error in combine_commands(): unidentified type token of type %i in 2nd switch statement\n", tStream->m_token.type);
            exit(1);
    }

    return combined;
}

command_stream_t append_to_cstream(command_stream_t cStream1, command_stream_t cStream2) {
    if (cStream2 == NULL)
        return cStream1;

    if (cStream1 == NULL) {
        cStream1 = cStream2;
        cStream1->prev = cStream2;
        cStream1->next = NULL;
        cStream1->number = 1;
        cStream1->iterator = 0;
    } else // cstream already has command_stream_t nodes in it so append to end
    {
        cStream2->number = (cStream1->prev->number) + 1;
        cStream2->prev = cStream1->prev;
        cStream2->next = NULL;
        cStream2->iterator = 0;

        (cStream1->prev)->next = cStream2;
        cStream1->prev = cStream2;
    }
    return cStream1;
}

// TODO: Refactor and combine similar cases

command_stream_t tokens_to_command_stream(token_stream_t tstream) {
    token_stream_t tCur, tNext, tPrev;
    command_t cTemp1, cTemp2, cTemp3, cmdA, cmdB, cmdC;
    command_stream_t csTemp1, csTemp2;

    enum token_type nextToken;

    tCur = tstream;
    tNext = tPrev = NULL;
    cTemp1 = cTemp2 = cmdA = cmdB = cmdC = NULL;
    csTemp1 = csTemp2 = NULL;

    int countParens = 0;
    int countIf = 0;
    int countWhile = 0;
    int countUntil = 0;

    char **word = NULL;
    size_t countWord = 0;
    size_t maxWord = 150;

    int top = -1;
    size_t cmdSizeStack = 10 * sizeof (command_t);
    c_stack = (command_t *) checked_malloc(cmdSizeStack);

    while (tCur != NULL) {
        tNext = tCur->next;
        tPrev = tCur->prev;
        if (tNext != NULL)
            nextToken = tNext->m_token.type;

        switch (tCur->m_token.type) {
            case WORD_TOKEN:
                if (cTemp1 == NULL) {
                    countWord = 0;
                    cTemp1 = new_command();
                    word = (char **) checked_malloc(maxWord * sizeof (char *));
                    cTemp1->u.word = word;
                }
                if (countWord == maxWord - 1) {
                    maxWord += 50 * sizeof (char *);

                    cTemp1->u.word = (char **) checked_grow_alloc(cTemp1->u.word, &maxWord);

                    word = cTemp1->u.word;

                    size_t i = 0;
                    while (i < countWord) {
                        word = ((cTemp1->u.word)++);
                        i++;
                    }
                    maxWord++;
                }

                *word = tCur->m_token.t_word;
                countWord++;
                *(++word) = NULL;

                break;

            case SEMICOLON_TOKEN:
                c_push(cTemp1, &top, &cmdSizeStack);

                while (stack_precedence(ts_peek()) >
                        stream_precedence(tCur->m_token.type)) {
                    cmdB = c_pop(&top);
                    cmdA = c_pop(&top);
                    cTemp1 = combine_commands(cmdA, cmdB, ts_pop());
                    c_push(cTemp1, &top, &cmdSizeStack);
                }
                cTemp1 = NULL;
                word = NULL;
                if (!(nextToken == THEN_TOKEN || nextToken == ELSE_TOKEN ||
                        nextToken == FI_TOKEN || nextToken == DO_TOKEN ||
                        nextToken == DONE_TOKEN)) {
                    ts_push(tCur);
                }
                break;

            case PIPE_TOKEN:
                c_push(cTemp1, &top, &cmdSizeStack);
                while (stack_precedence(ts_peek()) >
                        stream_precedence(tCur->m_token.type)) {
                    cmdB = c_pop(&top);
                    cmdA = c_pop(&top);
                    cTemp1 = combine_commands(cmdA, cmdB, ts_pop());
                    c_push(cTemp1, &top, &cmdSizeStack);
                }
                cTemp1 = cmdB = cmdA = NULL;
                word = NULL;
                ts_push(tCur);
                break;

            case LEFT_PAREN_TOKEN:
                c_push(cTemp1, &top, &cmdSizeStack);
                cTemp1 = NULL;
                word = NULL;
                countParens++;
                ts_push(tCur);
                break;

            case RIGHT_PAREN_TOKEN:
                if (countParens == 0) {
                    fprintf(stderr, "Error in tokens_to_command_stream(): unmatched ')'\n");
                    exit(1);
                }
                c_push(cTemp1, &top, &cmdSizeStack);
                countParens--;

                while (ts_peek() != LEFT_PAREN_TOKEN) {
                    if (ts_peek() == UNKNOWN_TOKEN) {
                        fprintf(stderr, "Error in tokens_to_command_stream(): Unknown token returned in RIGHT_PAREN_TOKEN case\n");
                        exit(1);
                    }
                    cmdB = c_pop(&top);
                    cmdA = c_pop(&top);
                    cTemp1 = combine_commands(cmdA, cmdB, ts_pop());
                    c_push(cTemp1, &top, &cmdSizeStack);
                    cTemp1 = NULL;
                }
                cTemp2 = new_command();
                cTemp2->type = SUBSHELL_COMMAND;
                cTemp2->u.command[0] = c_pop(&top); 

    
                c_push(cTemp2, &top, &cmdSizeStack);

                cTemp1 = cTemp2 = NULL;
                word = NULL;
                ts_pop();
                break;

            case LESS_TOKEN:
            case GREATER_TOKEN:
                c_push(cTemp1, &top, &cmdSizeStack);
                if (tNext != NULL && tNext->m_token.type == WORD_TOKEN) {
                    cTemp1 = c_pop(&top);
                    if (cTemp1 == NULL) {
                        fprintf(stderr, "Error in tokens_to_command_stream(): wrong command type left of redirection operator\n");
                        exit(1);
                    }
                    
                    if (tCur->m_token.type == LESS_TOKEN)
                        cTemp1->input = tNext->m_token.t_word;
                    else if (tCur->m_token.type == GREATER_TOKEN)
                        cTemp1->output = tNext->m_token.t_word;

                    c_push(cTemp1, &top, &cmdSizeStack);
                    cTemp1 = NULL;
                    word = NULL;

                    tCur = tNext;
                    if (tCur != NULL)
                        tNext = tCur->next;
                } else {
                    fprintf(stderr, "Error in tokens_to_command_stream(): wrong token type after redirection operator\n");
                    exit(1);
                }
                break;

            case WHILE_TOKEN:
            case UNTIL_TOKEN:
            case IF_TOKEN:
                c_push(cTemp1, &top, &cmdSizeStack);
                cTemp1 = NULL;
                word = NULL;
                if (tCur->m_token.type == WHILE_TOKEN)
                    countWhile++;
                else if (tCur->m_token.type == UNTIL_TOKEN)
                    countUntil++;
                else
                    countIf++;
                ts_push(tCur);
                break;

            case THEN_TOKEN:
            case ELSE_TOKEN:
            case DO_TOKEN:
                c_push(cTemp1, &top, &cmdSizeStack);
                cTemp1 = NULL;
                word = NULL;
                ts_push(tCur);
                break;

            case DONE_TOKEN:
                if (countWhile == 0 && countUntil == 0) {
                    fprintf(stderr, "Error in tokens_to_command_stream(): unmatched 'done' for while' or 'until' command\n");
                    exit(1);
                }
                c_push(cTemp1, &top, &cmdSizeStack);

                if (ts_peek() == DO_TOKEN) {
                    cTemp1 = c_pop(&top);
                    ts_pop();
                } else if (ts_peek() == UNKNOWN_TOKEN) {
                    fprintf(stderr, "Error in tokens_to_command_stream(): null token returned in DONE case\n");
                    exit(1);
                }

                if (ts_peek() == WHILE_TOKEN || ts_peek() == UNTIL_TOKEN) {
                    cTemp2 = c_pop(&top);
                } else if (ts_peek() == UNKNOWN_TOKEN) {
                    fprintf(stderr, "Error in tokens_to_command_stream(): null token returned in DONE case\n");
                    exit(1);
                }
                cTemp3 = new_command();
                if (ts_peek() == WHILE_TOKEN) {
                    cTemp3->type = WHILE_COMMAND;
                    countWhile--;
                } else {
                    cTemp3->type = UNTIL_COMMAND;
                    countUntil--;
                }
                cTemp3->u.command[0] = cTemp2; 
                cTemp3->u.command[1] = cTemp1;

                c_push(cTemp3, &top, &cmdSizeStack);

                cTemp1 = cTemp2 = cTemp3 = NULL;
                word = NULL;
                ts_pop();
                break;

            case FI_TOKEN:
                if (countIf == 0) {
                    fprintf(stderr, "Error in tokens_to_command_stream(): unmatched 'fi' for 'IF' command\n");
                    exit(1);
                }
                c_push(cTemp1, &top, &cmdSizeStack);

                if (ts_peek() == ELSE_TOKEN) {
                    cmdC = c_pop(&top);
                    ts_pop();
                }
                else if (ts_peek() == UNKNOWN_TOKEN) {
                    fprintf(stderr, "Error in tokens_to_command_stream(): unknown token returned in FI case\n");
                    exit(1);
                }

                if (ts_peek() == THEN_TOKEN) {
                    cmdB = c_pop(&top);
                    ts_pop();
                } else if (ts_peek() == UNKNOWN_TOKEN) {
                    fprintf(stderr, "Error in tokens_to_command_stream(): unknown token returned in FI case\n");
                    exit(1);
                }
                if (ts_peek() == IF_TOKEN) {
                    cmdA = c_pop(&top);
                } else if (ts_peek() == UNKNOWN_TOKEN) {
                    fprintf(stderr, "Error in tokens_to_command_stream(): unknown token returned in FI case\n");
                    exit(1);
                }

                cTemp1 = new_command();
                cTemp1->type = IF_COMMAND;

                if (cmdC != NULL)
                    cTemp1->u.command[2] = cmdC;
                else
                    cTemp1->u.command[2] = NULL;

                cTemp1->u.command[1] = cmdB; 
                cTemp1->u.command[0] = cmdA;


                c_push(cTemp1, &top, &cmdSizeStack);
                countIf--;

                cTemp1 = cmdA = cmdB = cmdC = NULL;
                word = NULL;
                ts_pop(); 

                break;

            case NEWLINE_TOKEN:
                c_push(cTemp1, &top, &cmdSizeStack);
                while (stack_precedence(ts_peek()) >
                        stream_precedence(tCur->m_token.type)) {
                    cmdB = c_pop(&top);
                    cmdA = c_pop(&top);
                    cTemp1 = combine_commands(cmdA, cmdB, ts_pop());
                    c_push(cTemp1, &top, &cmdSizeStack);
                }
                if (countParens == 0 && countIf == 0 && countWhile == 0
                        && countUntil == 0) {
                    csTemp1 = (command_stream_t) checked_malloc(sizeof (struct command_stream));
                    csTemp1->m_command = c_pop(&top);
                    csTemp2 = append_to_cstream(csTemp2, csTemp1);
                }

                cTemp1 = NULL;
                word = NULL;
                break;
            default:
                break;

        }

        tCur = tNext;

    } 

    if (countParens != 0 && countIf != 0 &&
            countWhile != 0 && countUntil != 0) {
        fprintf(stderr, "%s\n", "Error in tokens_to_command_stream(): out of while loop, closing pairs don't match");
        exit(1);
    }

    c_push(cTemp1, &top, &cmdSizeStack);
    while (ts_peek() != UNKNOWN_TOKEN) {
        cmdB = c_pop(&top);
        cmdA = c_pop(&top);
        cTemp1 = combine_commands(cmdA, cmdB, ts_pop());
        c_push(cTemp1, &top, &cmdSizeStack);
    }
    
    csTemp1 = (command_stream_t) checked_malloc(sizeof (struct command_stream));
    csTemp1->m_command = c_pop(&top);

    csTemp2 = append_to_cstream(csTemp2, csTemp1);
    cTemp1 = NULL;

    if (c_pop(&top) != NULL) {
        fprintf(stderr, "%s\n", "Error in tokens_to_command_stream(): c_stack should be empty");
        exit(1);
    }
    return csTemp2;

} // END of tokens_to_command_stream()

void ts_push(token_stream_t tStream) {
    if (tStream == NULL)
        return;

    if (tsStack == NULL) // If ts_stack is empty, just set it to item
    {
        tsStack = tStream;
        tsStack->prev = tsStack->next = NULL;
    } else {
        tStream->next = NULL; // Goes to the top
        tStream->prev = tsStack;

        tsStack->next = tStream;
        tsStack = tStream; // ts_stack points to the top
    }
    // printf("PUSHED IN a TOKEN of type %d into ts_stack\n", 
    // item->m_token.type);
}

enum token_type ts_peek() {
    if (tsStack == NULL) // If ts_stack is empty
        return UNKNOWN_TOKEN;
    else
        return tsStack->m_token.type;
}

token_stream_t ts_pop() {
    if (tsStack == NULL)
        return NULL;

    token_stream_t top = tsStack;

    tsStack = tsStack->prev;

    if (tsStack != NULL)
        tsStack->next = NULL;

    top->prev = top->next = NULL;

    return top;
}

void c_push(command_t cmd, int *top, size_t *size) {
    if (cmd == NULL)
        return;

    if (*size <= (*top + 1) * sizeof (command_t))
        c_stack = (command_t *) checked_grow_alloc(c_stack, size);

    (*top)++;
    c_stack[*top] = cmd;

    // printf("PUSHED in a COMMAND of type %d into c_stack[%d]\n", item->type, 
    // *top);
}

command_t c_pop(int *top) {
    if (*top == -1) // If c_stack is empty
        return NULL;

    command_t item = NULL;
    item = c_stack[*top];

    // printf("POPPED out a COMMAND of type %d out of c_stack[%d]\n", 
    // item->type, *top);

    (*top)--;
    return item;
}

int stack_precedence(enum token_type type) {
    switch (type) {
        case LEFT_PAREN_TOKEN:
            return -6;

        case UNTIL_TOKEN:
        case WHILE_TOKEN:
        case IF_TOKEN:
            return -4;

        case THEN_TOKEN:
        case DO_TOKEN:
            return -2;

        case ELSE_TOKEN:
            return 0;

        case LESS_TOKEN:
        case GREATER_TOKEN:
            return 10;

        case PIPE_TOKEN:
            return 12;

        case NEWLINE_TOKEN:
        case SEMICOLON_TOKEN:
            return 14;

        default:
            return -10;

    }
}

int stream_precedence(enum token_type type) {
    switch (type) {
        case LEFT_PAREN_TOKEN:
            return 19;

        case UNTIL_TOKEN:
        case WHILE_TOKEN:
        case IF_TOKEN:
            return 17;

        case THEN_TOKEN:
        case DO_TOKEN:
            return 15;

        case ELSE_TOKEN:
            return 13;

        case LESS_TOKEN:
        case GREATER_TOKEN:
            return 7;

        case PIPE_TOKEN:
            return 5;

        case NEWLINE_TOKEN:
        case SEMICOLON_TOKEN:
            return 3;

        default:
            return -10;
    }
}

command_stream_t make_command_stream(int (*get_next_byte) (void *),
        void *get_next_byte_argument) {

    struct token_stream *ts;
    struct command_stream *cs;

    char* buf = read_bytes(get_next_byte, get_next_byte_argument);

    ts = tokenize_bytes(buf);
    if (ts == NULL)
    { 
        fprintf(stderr, "%s\n", "Error in make_command_stream(): returning an empty token stream");
        return NULL;
    }
    cs = tokens_to_command_stream(ts);
    return cs;
}

command_t read_command_stream(command_stream_t s) {
    /* FIXME: Replace this with your implementation too.  */
  if (s == NULL)
    return NULL;
  else if (s->iterator == 0)  // 0 = node has not been iterated over yet
  {
    s->iterator = 1;
    return (s->m_command);
  }
  else if (s->next != NULL)
    return read_command_stream(s->next);
  else
    return NULL;
}
