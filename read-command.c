
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

//-----------------------------------------------------------------------------
//
//  read-command.c: Reads in shell commands from a file and outputs them in
//  in a standard format already supplied by code skeleton.
//
//-----------------------------------------------------------------------------
#include <string.h>
#include <error.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "command.h"
#include "command-internals.h"
#include "alloc.h"

#define OFFSET 2
#define STACK_MAX 100

struct command_stream
{
    command_stream_t next;
    command_stream_t prev;
    command_t command_node;
    int index;
    int count;
};

#define COMMAND_STREAM_SIZE sizeof(struct command_stream)

struct token_struct
{
    enum token_type type;
    char *token_val;
    int line_no;
    int token_len;
};

struct token_stream
{
    token_stream_t next;
    token_stream_t prev;
    struct token_struct token_node;
};

//-----COMMAND STACK----
struct CStack {
    command_t *c_stack;
    int top;
};

typedef struct CStack CStack;

void push_command_stack(struct command *cmd, size_t *size, struct CStack *cstack){
    if (cmd != NULL){
        if (*size <= (cstack->top) * sizeof(command_t))
            cstack->c_stack = (command_t *) checked_grow_alloc(cstack->c_stack, size);
        (cstack->top)++;
        cstack->c_stack[cstack->top] = cmd;
    }
}

command_t pop_command_stack(struct CStack *cstack){
    command_t cmd = NULL;
    if (cstack->top != 0){
        cmd = cstack->c_stack[cstack->top];
        (cstack->top)--;
        return cmd;
    }
    return cmd;
}

//-----TOKEN STACK-----
struct TStack {
    token_stream_t tks[STACK_MAX];
    int n_items;
};

typedef struct TStack TStack;

enum token_type top_token_stack(TStack *stack_t){
    if (stack_t->n_items == 0) {
        return OTHER;
    }
    return stack_t->tks[stack_t->n_items - 1]->token_node.type;
}

void push_token_stack(TStack *stack_t, struct token_stream *tks){
    if (tks == NULL)
        return;
    
    if (stack_t->n_items < STACK_MAX) {
        stack_t->tks[stack_t->n_items++] = tks;
    }
    else
        return;
}

token_stream_t pop_token_stack(TStack *stack_t){
    struct token_stream *top;
    if (stack_t->n_items > 0) {
        top = stack_t->tks[stack_t->n_items - 1];
        stack_t->n_items--;
    }
    else {
        fprintf(stderr, "Error. Popping from empty stack.\n");
    }
    return top;
}

//--------------------

int check_if_word(char c){
    if ((c == '!') || (c == '%') || (c == '!') || (c == '+') || (c == '-') || (c == ',') || (c == '.') || (c == '/') || (c == ':') || (c == '@') || (c == '^') || (c == '_') || isalnum(c))
        return 1;
    
    return 0;
}

void writeError(char* funcName, struct token_stream *cur){
    if (cur!= NULL){
        fprintf(stderr, "%i: Function %s has an error\n", cur->token_node.line_no, funcName);
    }else{
        fprintf(stderr, "Function %s has an error\n", funcName);
    }
    exit(1);
}

char* read_into_buffer(int (*get_next_byte) (void *), void *get_next_byte_argument){
    size_t n, buf_size;
    char next_byte;
    
    buf_size = 1024;
    n = 0;
    
    char *buf = (char *) checked_malloc(sizeof (char) * buf_size);
    
    while ((next_byte = get_next_byte(get_next_byte_argument)) != EOF) {
        
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

token_stream_t make_tokens_from_bytes(char *buf){
    struct token_stream *head = NULL;
    struct token_stream *ptr = head;
    char* itr = buf;
    int nLines = 1;
    char ch;
    enum token_type tk_type;
    
    while (*itr != '\0')
    {
        ch = *itr;
        struct token_stream *tk_stream = (struct token_stream *) checked_malloc(sizeof(struct token_stream));
        int word_n_chars = 1;
        char* word_start_itr;
        
        if (ch == '\n') {
            nLines++;
            tk_type = NEW_LINE;
            char nextCh = *(itr + 1);
            if (nextCh == '\n' && nextCh != '\0') {
                itr++;
                continue;
            }
        }
        else if (ch == '\t') {
            itr++;
            continue;
        }
        else if (ch == ' ') {
            itr++;
            continue;
        }
        else if (ch == ';') {
            tk_type = SEMICOLON;
        }
        else if (ch == '|') {
            tk_type = PIPE;
        }
        else if (ch == '(') {
            tk_type = OPEN_PAREN;
        }
        else if (ch == ')') {
            tk_type = CLOSE_PAREN;
        }
        else if (ch == '>') {
            tk_type = GREATER_THAN;
        }
        else if (ch == '<') {
            tk_type = LESS_THAN;
        }
        else {
            word_n_chars = 1;
            word_start_itr = itr;
            
            if (check_if_word(ch)){
                while (check_if_word(*(itr + word_n_chars)))
                    word_n_chars++;
                itr += word_n_chars - 1;
                tk_type = WORD;
            }
            else{
                tk_stream->token_node.token_val = NULL;
                fprintf(stderr, "Line %i in function make_tokens_from_bytes() has an error; the token \'%c\' is invalid.\n", nLines, ch);
                exit(1);
            }
        }
        
        tk_stream->prev = NULL;
        tk_stream->next = NULL;
        tk_stream->token_node.type = tk_type;
        tk_stream->token_node.token_len = word_n_chars;
        tk_stream->token_node.line_no = nLines;
        
        
        if (tk_type == NEW_LINE)
            tk_stream->token_node.line_no = nLines - 1;
        else if (tk_type == WORD){
            size_t word_size = 1 + sizeof(char) * word_n_chars;
            tk_stream->token_node.token_val = (char *) checked_malloc(word_size);
            int k;
            for (k = 0; k < word_n_chars; k++)
                tk_stream->token_node.token_val[k] = *(word_start_itr + k);
            
            tk_stream->token_node.token_val[word_n_chars] = '\0';
            
            if (strcmp(tk_stream->token_node.token_val, "if") == 0) {
                tk_stream->token_node.type = IF;
            } else if (strcmp(tk_stream->token_node.token_val, "then") == 0) {
                tk_stream->token_node.type = THEN;
            } else if (strcmp(tk_stream->token_node.token_val, "else") == 0) {
                tk_stream->token_node.type = ELSE;
            } else if (strcmp(tk_stream->token_node.token_val, "fi") == 0) {
                tk_stream->token_node.type = FI;
            } else if (strcmp(tk_stream->token_node.token_val, "while") == 0) {
                tk_stream->token_node.type = WHILE;
            } else if (strcmp(tk_stream->token_node.token_val, "do") == 0) {
                tk_stream->token_node.type = DO;
            } else if (strcmp(tk_stream->token_node.token_val, "done") == 0) {
                tk_stream->token_node.type = DONE;
            } else if (strcmp(tk_stream->token_node.token_val, "until") == 0) {
                tk_stream->token_node.type = UNTIL;
            }
            
        }
        else
            tk_stream->token_node.token_val = NULL;
        
        if (ptr != NULL)
        {
            tk_stream->prev = ptr;
            tk_stream->next = NULL;
            ptr->next = tk_stream;
            ptr = tk_stream;
        }
        else
        {
            head = tk_stream;
            ptr = head;
        }
        
        itr++;
    }
    return head;
}

command_stream_t combineStreams(struct command_stream *cStream1, struct command_stream *cStream2){
    command_stream_t final = NULL;
    if (cStream2 == NULL){
        final = cStream1;
    }else if (cStream1 != NULL){
        command_stream_t cTemp1 = cStream1 -> prev;
        int temp = cTemp1->index;
        cStream2->index = temp + 1;
        cStream2->prev = cTemp1;
        (cStream1->prev)->next = cStream2;
        cStream1->prev = cStream2;
        final = cStream1;
    }else{
        cStream1 = cStream2;
        cStream1->prev = cStream2;
        cStream1->index = 1;
        final = cStream1;
    }
    return final;
}

command_t join(struct token_stream *tstream, struct command *command0, struct command *command1){
    
    size_t command_struct_size = sizeof(struct command);
    command_t join_commands = (command_t) checked_malloc(command_struct_size);
    
    join_commands->status = -1;
    join_commands->type = SIMPLE_COMMAND;
    join_commands->u.word = NULL;
    join_commands->input = NULL;
    join_commands->output = NULL;
    join_commands->u.command[0] = command0;
    join_commands->u.command[1] = command1;
    
    enum token_type tstream_type = tstream->token_node.type;
    if (tstream == NULL)
    {
        writeError("join()", NULL);
    }
    else if (tstream_type == PIPE)
        join_commands->type = PIPE_COMMAND;
    else if (tstream_type == CLOSE_PAREN)
        join_commands->type = SUBSHELL_COMMAND;
    else if (tstream_type == SEMICOLON)
        join_commands->type = SEQUENCE_COMMAND;
    else
    {
        writeError("join()", NULL);
    }
    
    return join_commands;
}

int calcPVal(int multiplier, int stack){
    int value = (stack) ? multiplier*OFFSET : multiplier*OFFSET+1;
    return value;
}

int precedence(enum token_type type, int stack){
    if(stack){
        if (type == ELSE){
            return calcPVal(0, stack);
        }else if (type == GREATER_THAN || type == LESS_THAN){
            return calcPVal(5, stack);
        }else if (type == PIPE){
            return calcPVal(6, stack);
        }else if (type == SEMICOLON || type == NEW_LINE){
            return calcPVal(7, stack);
        }else if (type == DO || type == THEN){
            return calcPVal(-1, stack);
        }else if (type == IF || type == WHILE || type == UNTIL){
            return calcPVal(-2, stack);
        }else if (type == OPEN_PAREN){
            return calcPVal(-3, stack);
        }else{
            return calcPVal(-5, stack);
        }
    }else{
        if (type == ELSE){
            return calcPVal(6, stack);
        }else if (type == GREATER_THAN || type == LESS_THAN){
            return calcPVal(3, stack);
        }else if (type == PIPE){
            return calcPVal(2, stack);
        }else if (type == SEMICOLON || type == NEW_LINE){
            return calcPVal(1, stack);
        }else if (type == DO || type == THEN){
            return calcPVal(7, stack);
        }else if (type == IF || type == WHILE || type == UNTIL){
            return calcPVal(8, stack);
        }else if (type == OPEN_PAREN){
            return calcPVal(9, stack);
        }else{
            return calcPVal(-5, stack);
        }
    }
}

void checkSyntax(struct token_stream *token_str){

    struct token_stream *cur_token_stream;
    struct token_stream *next_token_stream;
    struct token_stream *prev_token_stream;
    int doneCount = 0;
    int parensCount = 0;
    int ifCount = 0;
    enum token_type next_token;
    enum token_type prev_tokens;

    cur_token_stream = token_str;
    
    while(cur_token_stream != NULL){
        next_token_stream = cur_token_stream->next;
        next_token = (next_token_stream == NULL) ? OTHER : next_token_stream->token_node.type;
        prev_token_stream = cur_token_stream->prev;
        prev_tokens = (prev_token_stream == NULL) ? OTHER : prev_token_stream->token_node.type;
        
        int wordCheck = next_token == IF || next_token == FI || next_token == THEN || next_token == ELSE || next_token == DO || next_token == DONE || next_token == UNTIL || next_token == WHILE;
        int semicolonCheck1 = ((next_token == OPEN_PAREN || next_token == CLOSE_PAREN || next_token == WORD) && ((parensCount != 0 && prev_tokens != OPEN_PAREN) ||
                                                                                                  (ifCount != 0 && !(prev_tokens == IF || prev_tokens == THEN || prev_tokens == ELSE)) || (doneCount != 0 && !(prev_tokens == WHILE || prev_tokens == UNTIL || prev_tokens == DO))));
        int semicolonCheck2 = next_token != IF && next_token != FI && next_token != THEN && next_token != ELSE && next_token != OPEN_PAREN && next_token != CLOSE_PAREN && next_token != WORD && next_token != DO && next_token != DONE && next_token != WHILE && next_token != UNTIL;
        
        enum token_type ttCur = cur_token_stream->token_node.type;
        if (ttCur == WORD && wordCheck)
            next_token = WORD;
        else if (ttCur == SEMICOLON && (next_token == SEMICOLON || cur_token_stream == token_str))
            writeError("checkSyntax()", cur_token_stream);
        else if (ttCur == PIPE && (cur_token_stream->token_node.type == next_token || cur_token_stream == token_str || next_token == SEMICOLON))
            writeError("checkSyntax()", cur_token_stream);
        else if (ttCur == OPEN_PAREN){
            if (next_token == CLOSE_PAREN)
                writeError("checkSyntax()", cur_token_stream);
            parensCount++;
        }else if (ttCur == CLOSE_PAREN)
            parensCount--;
        else if ((ttCur == LESS_THAN || ttCur == GREATER_THAN) && (next_token != WORD || next_token_stream == NULL))
            writeError("checkSyntax()", cur_token_stream);
        else if (ttCur == NEW_LINE && next_token_stream != NULL){
            if (semicolonCheck1)
                cur_token_stream->token_node.type = SEMICOLON;
            else if(semicolonCheck2)
                writeError("checkSyntax()", cur_token_stream);
        }else if ((ttCur == THEN || ttCur == ELSE) && (next_token_stream == NULL || next_token == FI))
            writeError("checkSyntax()", cur_token_stream);
        else if (ttCur == DO){
            if (prev_tokens == WORD){
                cur_token_stream->token_node.type = WORD;
            }
            if (cur_token_stream->token_node.type == next_token || (prev_tokens != NEW_LINE && prev_tokens != SEMICOLON) || (next_token_stream == NULL || next_token == SEMICOLON))
                writeError("checkSyntax()", cur_token_stream);
        }else{
            switch (ttCur){
                case DONE:
                case FI:
                    if (next_token == WORD)
                        writeError("checkSyntax()", cur_token_stream);
                case IF:
                case WHILE:
                case UNTIL:
                    if (next_token == SEMICOLON || next_token_stream == NULL)
                        writeError("checkSyntax()", cur_token_stream);
                    if (prev_tokens == WORD)
                        cur_token_stream->token_node.type = WORD;
                    if (cur_token_stream->token_node.type == IF)
                        ifCount++;
                    else if (cur_token_stream->token_node.type == FI)
                        ifCount--;
                    else if (cur_token_stream->token_node.type == DONE)
                        doneCount--;
                    else if (cur_token_stream->token_node.type == UNTIL)
                        doneCount++;
                    else if (cur_token_stream->token_node.type == WHILE)
                        doneCount++;
                    break;
                default:
                    break;
            }
        }
        cur_token_stream = next_token_stream;
    }
    
    if(doneCount != 0 || ifCount != 0 || parensCount != 0)
        writeError("checkSyntax()", cur_token_stream);
}

command_stream_t commandBuilder(struct token_stream *token_struct_ptr){
    
    size_t size_of_stack = STACK_MAX * sizeof(command_t);
    CStack *stack_of_cmd_ptrs = checked_malloc(sizeof(CStack));
    stack_of_cmd_ptrs->c_stack = (command_t *) checked_malloc(size_of_stack);
    stack_of_cmd_ptrs->top = 0;
    
    TStack* stack_of_token_streams = checked_malloc(sizeof(TStack));
    stack_of_token_streams->n_items = 0;
    
    char **word = NULL;
    enum token_type tokenNext;
    
    token_stream_t tsCur = token_struct_ptr;
    token_stream_t tsNext = NULL;
    token_stream_t tsPrev = NULL;
    
    command_t cmdTemp1 = NULL;
    command_t cmdTemp2 = NULL;
    command_t cmdTemp3 = NULL;
    
    command_t cmdA = NULL;
    command_t cmdB = NULL;
    command_t cmdC = NULL;
    
    command_stream_t csTemp1 = NULL;
    command_stream_t csTemp2 = NULL;
    
    int countIfs = 0;
    int countParens = 0;
    int countWhiles = 0;
    int countUntils = 0;
    
    size_t countWords = 0;
    size_t maxWord = 150;
    
    while (tsCur != NULL)
    {
        tsPrev = tsCur->prev;
        tsNext = tsCur->next;
        if (tsNext != NULL)
            tokenNext = tsNext->token_node.type;
        
        enum token_type ttCur = tsCur->token_node.type;
        
        if(ttCur == WORD){
            if (cmdTemp1 == NULL){
                countWords = 0;
                size_t struct_command_size = sizeof(struct command);
                cmdTemp1 = (command_t) checked_malloc(struct_command_size);
                cmdTemp1->status = -1;
                cmdTemp1->type = SIMPLE_COMMAND;
                cmdTemp1->u.word = NULL;
                cmdTemp1->input = NULL;
                cmdTemp1->output = NULL;
                word = (char **) checked_malloc(maxWord * sizeof(char *));
                cmdTemp1->u.word = word;
            }
            if (countWords == maxWord - 1){
                maxWord += 50 * sizeof(char *);
                cmdTemp1->u.word = (char **) checked_grow_alloc(cmdTemp1->u.word, &maxWord);
                word = cmdTemp1->u.word;
                size_t i;
                for (i=0;i < countWords; i++){
                    word = ((cmdTemp1->u.word)++);
                }
                maxWord++;
            }
            *word = tsCur->token_node.token_val;
            countWords++;
            *(++word) = NULL;
            
        }else if (ttCur == SEMICOLON){
            push_command_stack(cmdTemp1, &size_of_stack, stack_of_cmd_ptrs);
            
            while (precedence(top_token_stack(stack_of_token_streams), 1) > precedence(tsCur->token_node.type, 0)){
                cmdB = pop_command_stack( stack_of_cmd_ptrs);
                cmdA = pop_command_stack(stack_of_cmd_ptrs);
                cmdTemp1 = join(pop_token_stack(stack_of_token_streams), cmdA, cmdB);
                push_command_stack(cmdTemp1, &size_of_stack, stack_of_cmd_ptrs);
            }
            cmdTemp1 = NULL;
            word = NULL;
            if (tokenNext != THEN && tokenNext != ELSE && tokenNext != FI && tokenNext != DO &&
                tokenNext != DONE){
                push_token_stack(stack_of_token_streams, tsCur);
            }
            
        }else if (ttCur == PIPE){
            push_command_stack(cmdTemp1, &size_of_stack, stack_of_cmd_ptrs);
            while (precedence(top_token_stack(stack_of_token_streams), 1) > precedence(tsCur->token_node.type, 0)){
                cmdB = pop_command_stack( stack_of_cmd_ptrs);
                cmdA = pop_command_stack( stack_of_cmd_ptrs);
                cmdTemp1 = join(pop_token_stack(stack_of_token_streams), cmdA, cmdB);
                push_command_stack(cmdTemp1,  &size_of_stack, stack_of_cmd_ptrs);
            }
            cmdTemp1 = cmdB = cmdA = NULL;
            word = NULL;
            push_token_stack(stack_of_token_streams, tsCur);
            
        }else if (ttCur == OPEN_PAREN){
            push_command_stack(cmdTemp1, &size_of_stack, stack_of_cmd_ptrs);
            cmdTemp1 = NULL;
            word = NULL;
            countParens++;
            push_token_stack(stack_of_token_streams, tsCur);
            
        }else if (ttCur == CLOSE_PAREN){
            if (countParens != 0) {
                push_command_stack(cmdTemp1, &size_of_stack, stack_of_cmd_ptrs);
                countParens--;
                while (top_token_stack(stack_of_token_streams) != OPEN_PAREN){
                    if (top_token_stack(stack_of_token_streams) != OTHER){
                        cmdB = pop_command_stack( stack_of_cmd_ptrs);
                        cmdA = pop_command_stack( stack_of_cmd_ptrs);
                        cmdTemp1 = join(pop_token_stack(stack_of_token_streams), cmdA, cmdB);
                        push_command_stack(cmdTemp1,  &size_of_stack, stack_of_cmd_ptrs);
                        cmdTemp1 = NULL;
                    }else{
                        writeError("commandBuilder()", NULL);
                    }
                }
                size_t struct_command_size = sizeof(struct command);
                cmdTemp2 = (command_t) checked_malloc(struct_command_size);
                cmdTemp2->status = -1;
                cmdTemp2->u.word = NULL;
                cmdTemp2->input = NULL;
                cmdTemp2->output = NULL;
                cmdTemp2->type = SUBSHELL_COMMAND;
                cmdTemp2->u.command[0] = pop_command_stack( stack_of_cmd_ptrs);
                push_command_stack(cmdTemp2,  &size_of_stack, stack_of_cmd_ptrs);
                cmdTemp1 = cmdTemp2 = NULL;
                word = NULL;
                pop_token_stack(stack_of_token_streams);
            }else{
                writeError("commandBuilder()", NULL);
            }
            
        }else if (ttCur == LESS_THAN || ttCur == GREATER_THAN){
            push_command_stack(cmdTemp1,  &size_of_stack, stack_of_cmd_ptrs);
            if (tsNext == NULL || tsNext->token_node.type != WORD){
                writeError("commandBuilder()", NULL);
            }else{
                cmdTemp1 = pop_command_stack( stack_of_cmd_ptrs);
                if (cmdTemp1 != NULL) {
                    if (tsCur->token_node.type == GREATER_THAN)
                        cmdTemp1->output = tsNext->token_node.token_val;
                    else if (tsCur->token_node.type == LESS_THAN)
                        cmdTemp1->input = tsNext->token_node.token_val;
                    push_command_stack(cmdTemp1,  &size_of_stack, stack_of_cmd_ptrs);
                    cmdTemp1 = NULL;
                    word = NULL;
                    tsCur = tsNext;
                    if (tsCur != NULL)
                        tsNext = tsCur->next;
                }else{
                    writeError("commandBuilder()", NULL);
                }
            }
            
        }else if (ttCur == WHILE || ttCur == UNTIL || ttCur == IF){
            push_command_stack(cmdTemp1,  &size_of_stack, stack_of_cmd_ptrs);
            cmdTemp1 = NULL;
            word = NULL;
            if (tsCur->token_node.type == UNTIL)
                countUntils++;
            else if (tsCur->token_node.type == WHILE)
                countWhiles++;
            else
                countIfs++;
            push_token_stack(stack_of_token_streams, tsCur);
            
        }else if (ttCur == THEN || ttCur == ELSE || ttCur == DO){
            push_command_stack(cmdTemp1, &size_of_stack, stack_of_cmd_ptrs);
            cmdTemp1 = NULL;
            word = NULL;
            push_token_stack(stack_of_token_streams, tsCur);
            
        }else if (ttCur == DONE){
            if (countUntils == 0 && countWhiles == 0) {
                writeError("commandBuilder()", NULL);
            }
            push_command_stack(cmdTemp1, &size_of_stack, stack_of_cmd_ptrs);
            
            enum token_type ttTemp1 = top_token_stack(stack_of_token_streams);
            if (ttTemp1 == OTHER){
                writeError("commandBuilder()", NULL);
            }else if (ttTemp1 == DO){
                cmdTemp1 = pop_command_stack( stack_of_cmd_ptrs);
                pop_token_stack(stack_of_token_streams);
            }
            
            ttTemp1 = top_token_stack(stack_of_token_streams);
            if (ttTemp1 == OTHER ){
                writeError("commandBuilder()", NULL);
            }else if (ttTemp1 == WHILE || ttTemp1 == UNTIL){
                cmdTemp2 = pop_command_stack( stack_of_cmd_ptrs);
            }
            
            size_t struct_command_size = sizeof(struct command);
            cmdTemp3 = (command_t) checked_malloc(struct_command_size);
            cmdTemp3->status = -1;
            cmdTemp3->u.word = NULL;
            cmdTemp3->input = NULL;
            cmdTemp3->output = NULL;
            ttTemp1 = top_token_stack(stack_of_token_streams);
            if (ttTemp1 != WHILE){
                cmdTemp3->type = UNTIL_COMMAND;
                countUntils--;
            }else{
                cmdTemp3->type = WHILE_COMMAND;
                countWhiles--;
            }
            cmdTemp3->u.command[0] = cmdTemp2;
            cmdTemp3->u.command[1] = cmdTemp1;
            
            push_command_stack(cmdTemp3, &size_of_stack, stack_of_cmd_ptrs);
            
            cmdTemp1 = cmdTemp2 = cmdTemp3 = NULL;
            word = NULL;
            pop_token_stack(stack_of_token_streams);
            
        }else if (ttCur == FI){
            if (countIfs == 0){
                writeError("commandBuilder()", NULL);
            }
            push_command_stack(cmdTemp1,  &size_of_stack, stack_of_cmd_ptrs);
            
            enum token_type ttTemp2 = top_token_stack(stack_of_token_streams);
            if (ttTemp2 == OTHER ){
                writeError("commandBuilder()", NULL);
            }else if (ttTemp2 == ELSE){
                cmdC = pop_command_stack( stack_of_cmd_ptrs);
                pop_token_stack(stack_of_token_streams);
            }
            
            ttTemp2 = top_token_stack(stack_of_token_streams);
            if (ttTemp2 == OTHER){
                writeError("commandBuilder()", NULL);
            }else if (ttTemp2 == THEN){
                cmdB = pop_command_stack( stack_of_cmd_ptrs);
                pop_token_stack(stack_of_token_streams);
            }
            
            ttTemp2 = top_token_stack(stack_of_token_streams);
            if (ttTemp2 == OTHER){
                writeError("commandBuilder()", NULL);
            }else if (ttTemp2 == IF){
                cmdA = pop_command_stack( stack_of_cmd_ptrs);
            }
            
            size_t struct_command_size = sizeof(struct command);
            cmdTemp1 = (command_t) checked_malloc(struct_command_size);
            cmdTemp1->status = -1;
            cmdTemp1->type = IF_COMMAND;
            cmdTemp1->u.word = NULL;
            cmdTemp1->input = NULL;
            cmdTemp1->output = NULL;
            if (cmdC == NULL)
                cmdTemp1->u.command[2] = NULL;
            else
                cmdTemp1->u.command[2] = cmdC;
            cmdTemp1->u.command[0] = cmdA;
            cmdTemp1->u.command[1] = cmdB;
            
            push_command_stack(cmdTemp1, &size_of_stack, stack_of_cmd_ptrs);
            countIfs--;
            
            cmdA = cmdB = cmdC = cmdTemp1 = NULL;
            word = NULL;
            pop_token_stack(stack_of_token_streams);
            
        }else if (ttCur == NEW_LINE){
            push_command_stack(cmdTemp1, &size_of_stack, stack_of_cmd_ptrs);
            while (precedence(top_token_stack(stack_of_token_streams), 1) > precedence(tsCur->token_node.type, 0)){
                cmdB = pop_command_stack( stack_of_cmd_ptrs);
                cmdA = pop_command_stack(stack_of_cmd_ptrs);
                cmdTemp1 = join(pop_token_stack(stack_of_token_streams), cmdA, cmdB);
                push_command_stack(cmdTemp1, &size_of_stack, stack_of_cmd_ptrs);
            }
            if (countParens == 0 && countIfs == 0 && countWhiles == 0 && countUntils == 0){
                csTemp1 = (command_stream_t) checked_malloc(COMMAND_STREAM_SIZE);
                csTemp1->command_node = pop_command_stack( stack_of_cmd_ptrs);
                csTemp2 = combineStreams(csTemp2, csTemp1);
            }
            cmdTemp1 = NULL;
            word = NULL;
        }
        tsCur = tsNext;
    }
    
    if (countParens != 0 && countIfs != 0 && countWhiles != 0 && countUntils != 0){
        writeError("commandBuilder()", NULL);
    }
    
    push_command_stack(cmdTemp1, &size_of_stack, stack_of_cmd_ptrs);
    
    while (top_token_stack(stack_of_token_streams) != OTHER){
        cmdB = pop_command_stack( stack_of_cmd_ptrs);
        cmdA = pop_command_stack( stack_of_cmd_ptrs);
        cmdTemp1 = join(pop_token_stack(stack_of_token_streams), cmdA, cmdB);
        push_command_stack(cmdTemp1,  &size_of_stack, stack_of_cmd_ptrs);
    }
    
    csTemp1 = (command_stream_t) checked_malloc(COMMAND_STREAM_SIZE);
    csTemp1->command_node = pop_command_stack( stack_of_cmd_ptrs);
    
    csTemp2 = combineStreams(csTemp2, csTemp1);
    cmdTemp1 = NULL;
    
    if (pop_command_stack( stack_of_cmd_ptrs) != NULL){
        writeError("commandBuilder()", NULL);
    }
    
    return csTemp2;
}

command_stream_t make_command_stream (int (*get_next_byte) (void *), void *get_next_byte_argument){
    char *buf = read_into_buffer(get_next_byte, get_next_byte_argument);
    token_stream_t tk_stream;
    tk_stream = make_tokens_from_bytes(buf);
    command_stream_t c_stream;
    if (tk_stream != NULL){
        checkSyntax(tk_stream);
        c_stream = commandBuilder(tk_stream);
        return c_stream;
    }
    return NULL;
}

command_t read_command_stream (command_stream_t s){
    int iter = s->count;
    struct command *node = s->command_node;
    if (s != NULL){
        if (iter == 0){
            s->count++;
            return node;
        }else if (s->next != NULL){
            return read_command_stream(s->next);
        }
    }
    return NULL;
}

