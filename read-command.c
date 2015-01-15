  
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

#include "command.h"
#include "command-internals.h"
#include "alloc.h"

#include <string.h>
#include <error.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

command_t *c_stack = NULL;  

#define OFFSET 2


#define STACK_MAX 100

struct TStack {
    token_stream_t tks[STACK_MAX];
    int n_items;
};

typedef struct TStack TStack;

enum token_type ts_peek(TStack *stack_t)
{
    if (stack_t->n_items == 0) {
        // fprintf(stderr, "Error. The stack is empty.\n");
        return OTHER;
    } 
    return stack_t->tks[stack_t->n_items - 1]->token_node.type;
}

void ts_push(TStack *stack_t, token_stream_t tks)
{
    if (tks == NULL)
    return;

    if (stack_t->n_items < STACK_MAX) {
        stack_t->tks[stack_t->n_items++] = tks;
    }
    else
      return;
}

token_stream_t ts_pop(TStack *stack_t)
{

    token_stream_t top;
    if (stack_t->n_items > 0) {
        top = stack_t->tks[stack_t->n_items - 1];
        stack_t->n_items--;
    }
    else {
      fprintf(stderr, "Error. Popping from empty stack.\n");
    }
    return top;
}

struct CStack {
    command_t *c_stack;
    int top;
};

typedef struct CStack CStack;

void c_push(command_t item, size_t *size, struct CStack *cstack)
{
  if (item == NULL)
    return;
  if (*size <= (cstack->top) * sizeof(command_t))
    cstack->c_stack = (command_t *) checked_grow_alloc(cstack->c_stack, size);
  (cstack->top)++;
  cstack->c_stack[cstack->top] = item;

}


command_t c_pop(struct CStack *cstack)
{
  if (cstack->top == 0)
    return NULL;
  command_t item = NULL;
  item = cstack->c_stack[cstack->top];
  (cstack->top)--;
  return item;
}



//Parsing
char* read_into_buffer(int (*get_next_byte) (void *), void *get_next_byte_argument);
token_stream_t make_tokens_from_bytes(char *buffer);

//Helper function to write errors during validation
void writeError(char* funcName, token_stream_t cur);

//Syntax checking
void checkSyntax(token_stream_t tstream);

//Used to determine precedence
int calcPVal(int multiplier, int stack);

//Converting to command stream
command_stream_t commandBuilder(token_stream_t tstream);

//Word validation
int check_if_word(char ch);

command_t combine_commands(command_t cmd_A, command_t cmd_B, token_stream_t tstream);

command_stream_t combineStreams(command_stream_t cstream, command_stream_t item);

command_stream_t make_command_stream (int (*get_next_byte) (void *), void *get_next_byte_argument){
  char *buffer = read_into_buffer(get_next_byte, get_next_byte_argument);
  struct token_stream *tstream;
  struct command_stream *cstream;
  tstream = make_tokens_from_bytes(buffer);
  if (tstream != NULL){ 
    checkSyntax(tstream);
    cstream = commandBuilder(tstream);
    return cstream;   
  }
  return NULL;
}


command_t read_command_stream (command_stream_t s){
  int iter = s->iterator;
  struct command *node = s->command_node;
  if (s != NULL){
    if (iter == 0){
      s->iterator++;
      return node;
    }else if (s->next != NULL){
      return read_command_stream(s->next);
    }
  }
  return NULL;
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
        tk_stream->token_node.t_word = NULL;
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
      tk_stream->token_node.t_word = (char *) checked_malloc(word_size);
      int k;
      for (k = 0; k < word_n_chars; k++)
          tk_stream->token_node.t_word[k] = *(word_start_itr + k);

      tk_stream->token_node.t_word[word_n_chars] = '\0';

      if (strcmp(tk_stream->token_node.t_word, "if") == 0) {
          tk_stream->token_node.type = IF;
      } else if (strcmp(tk_stream->token_node.t_word, "then") == 0) {
          tk_stream->token_node.type = THEN;
      } else if (strcmp(tk_stream->token_node.t_word, "else") == 0) {
          tk_stream->token_node.type = ELSE;
      } else if (strcmp(tk_stream->token_node.t_word, "fi") == 0) {
          tk_stream->token_node.type = FI;
      } else if (strcmp(tk_stream->token_node.t_word, "while") == 0) {
          tk_stream->token_node.type = WHILE;
      } else if (strcmp(tk_stream->token_node.t_word, "do") == 0) {
          tk_stream->token_node.type = DO;
      } else if (strcmp(tk_stream->token_node.t_word, "done") == 0) {
          tk_stream->token_node.type = DONE;
      } else if (strcmp(tk_stream->token_node.t_word, "until") == 0) {
          tk_stream->token_node.type = UNTIL;
      }
    
    } 
    else
      tk_stream->token_node.t_word = NULL;

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

void writeError(char* funcName, token_stream_t cur){
  if (cur!= NULL){
  fprintf(stderr, "%i: Function %s has an error\n", cur->token_node.line_no, funcName);
  }else{
    fprintf(stderr, "Function %s has an error\n", funcName);
  }
  exit(1);
}


void checkSyntax(token_stream_t tStream){
  enum token_type tNext; 
  enum token_type tPrev;
  token_stream_t tsCur;
  token_stream_t tsNext;
  token_stream_t tsPrev;

  tsCur = tStream;

  int numParens = 0;
  int numIf = 0;
  int numDone = 0; 

  while(tsCur != NULL){
    tsNext = tsCur->next;
    tNext = (tsNext == NULL) ? OTHER : tsNext->token_node.type;
    tsPrev = tsCur->prev;
    tPrev = (tsPrev == NULL) ? OTHER : tsPrev->token_node.type;

    int wordCheck = tNext == IF || tNext == FI || tNext == THEN || tNext == ELSE || tNext == DO || tNext == DONE || tNext == UNTIL || tNext == WHILE;
    int semicolonCheck1 = ((tNext == OPEN_PAREN || tNext == CLOSE_PAREN || tNext == WORD) && ((numParens != 0 && tPrev != OPEN_PAREN) ||
                          (numIf != 0 && !(tPrev == IF || tPrev == THEN || tPrev == ELSE)) || (numDone != 0 && !(tPrev == WHILE || tPrev == UNTIL || tPrev == DO)))); 
    int semicolonCheck2 = tNext != IF && tNext != FI && tNext != THEN && tNext != ELSE && tNext != OPEN_PAREN && tNext != CLOSE_PAREN && tNext != WORD && tNext != DO && tNext != DONE && tNext != WHILE && tNext != UNTIL;

    enum token_type ttCur = tsCur->token_node.type;
    if (ttCur == WORD && wordCheck)
        tNext = WORD;
    else if (ttCur == SEMICOLON && (tNext == SEMICOLON || tsCur == tStream))
        writeError("checkSyntax()", tsCur);
    else if (ttCur == PIPE && (tsCur->token_node.type == tNext || tsCur == tStream || tNext == SEMICOLON))
        writeError("checkSyntax()", tsCur);
    else if (ttCur == OPEN_PAREN){
      if (tNext == CLOSE_PAREN)
        writeError("checkSyntax()", tsCur);
      numParens++;
    }else if (ttCur == CLOSE_PAREN)
      numParens--;
    else if ((ttCur == LESS_THAN || ttCur == GREATER_THAN) && (tNext != WORD || tsNext == NULL))
        writeError("checkSyntax()", tsCur);
    else if (ttCur == NEW_LINE && tsNext != NULL){
      if (semicolonCheck1)
        tsCur->token_node.type = SEMICOLON;
      else if(semicolonCheck2)
        writeError("checkSyntax()", tsCur);
    }else if ((ttCur == THEN || ttCur == ELSE) && (tsNext == NULL || tNext == FI))
        writeError("checkSyntax()", tsCur);
    else if (ttCur == DO){
      if (tPrev == WORD){
        tsCur->token_node.type = WORD;
      }
      if (tsCur->token_node.type == tNext || (tPrev != NEW_LINE && tPrev != SEMICOLON) || (tsNext == NULL || tNext == SEMICOLON))
        writeError("checkSyntax()", tsCur);
    }else{
      switch (ttCur){
        case DONE:
        case FI:
          if (tNext == WORD)
            writeError("checkSyntax()", tsCur);
        case IF:      
        case WHILE:
        case UNTIL:
          if (tNext == SEMICOLON || tsNext == NULL)
            writeError("checkSyntax()", tsCur);
          if (tPrev == WORD)
            tsCur->token_node.type = WORD;
          if (tsCur->token_node.type == IF)
            numIf++;
          else if (tsCur->token_node.type == FI)
            numIf--;
          else if (tsCur->token_node.type == DONE)
            numDone--;
          else if (tsCur->token_node.type == UNTIL)
            numDone++;
          else if (tsCur->token_node.type == WHILE)
            numDone++;
          break;
        default:
          break;
      }
    }
    tsCur = tsNext;
  }

  if(numDone != 0 || numIf != 0 || numParens != 0)
    writeError("checkSyntax()", tsCur);
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

command_stream_t commandBuilder(token_stream_t tStream){
  TStack* ts_stack = checked_malloc(sizeof(TStack));
  ts_stack->n_items = 0;

  char **word = NULL;
  enum token_type tokenNext;
  
  token_stream_t tsCur = tStream;
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

  size_t c_stackSize = 10 * sizeof(command_t);
  CStack *cstack = checked_malloc(sizeof(CStack));
  cstack->c_stack = (command_t *) checked_malloc(c_stackSize);
  cstack->top = 0;

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
      *word = tsCur->token_node.t_word;
      countWords++;
      *(++word) = NULL; 

    }else if (ttCur == SEMICOLON){
      c_push(cmdTemp1, &c_stackSize, cstack);

      while (precedence(ts_peek(ts_stack), 1) > precedence(tsCur->token_node.type, 0)){
        cmdB = c_pop( cstack);
        cmdA = c_pop(cstack);
        cmdTemp1 = combine_commands(cmdA, cmdB, ts_pop(ts_stack));
        c_push(cmdTemp1, &c_stackSize, cstack);
      }
      cmdTemp1 = NULL;
      word = NULL;
      if (tokenNext != THEN && tokenNext != ELSE && tokenNext != FI && tokenNext != DO &&
        tokenNext != DONE){
        ts_push(ts_stack, tsCur);
      }

    }else if (ttCur == PIPE){
      c_push(cmdTemp1, &c_stackSize, cstack);
      while (precedence(ts_peek(ts_stack), 1) > precedence(tsCur->token_node.type, 0)){
        cmdB = c_pop( cstack);
        cmdA = c_pop( cstack);
        cmdTemp1 = combine_commands(cmdA, cmdB, ts_pop(ts_stack));
        c_push(cmdTemp1,  &c_stackSize, cstack);
      }
      cmdTemp1 = cmdB = cmdA = NULL;
      word = NULL;
      ts_push(ts_stack, tsCur);

    }else if (ttCur == OPEN_PAREN){
      c_push(cmdTemp1, &c_stackSize, cstack);
      cmdTemp1 = NULL;
      word = NULL;
      countParens++;
      ts_push(ts_stack, tsCur);

    }else if (ttCur == CLOSE_PAREN){
      if (countParens != 0) {
        c_push(cmdTemp1, &c_stackSize, cstack);
        countParens--;
        while (ts_peek(ts_stack) != OPEN_PAREN){
          if (ts_peek(ts_stack) != OTHER){
            cmdB = c_pop( cstack);
            cmdA = c_pop( cstack);
            cmdTemp1 = combine_commands(cmdA, cmdB, ts_pop(ts_stack));  
            c_push(cmdTemp1,  &c_stackSize, cstack);
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
        cmdTemp2->u.command[0] = c_pop( cstack); 
        c_push(cmdTemp2,  &c_stackSize, cstack);
        cmdTemp1 = cmdTemp2 = NULL;
        word = NULL;
        ts_pop(ts_stack); 
      }else{
        writeError("commandBuilder()", NULL);
      }
      
    }else if (ttCur == LESS_THAN || ttCur == GREATER_THAN){
      c_push(cmdTemp1,  &c_stackSize, cstack);
      if (tsNext == NULL || tsNext->token_node.type != WORD){
        writeError("commandBuilder()", NULL);   
      }else{
        cmdTemp1 = c_pop( cstack);
        if (cmdTemp1 != NULL) {
          if (tsCur->token_node.type == GREATER_THAN)
            cmdTemp1->output = tsNext->token_node.t_word;
          else if (tsCur->token_node.type == LESS_THAN)
            cmdTemp1->input = tsNext->token_node.t_word;
          c_push(cmdTemp1,  &c_stackSize, cstack);
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
      c_push(cmdTemp1,  &c_stackSize, cstack);
      cmdTemp1 = NULL;
      word = NULL;
      if (tsCur->token_node.type == UNTIL)
        countUntils++;
      else if (tsCur->token_node.type == WHILE)
        countWhiles++;
      else
        countIfs++;
      ts_push(ts_stack, tsCur);

    }else if (ttCur == THEN || ttCur == ELSE || ttCur == DO){
      c_push(cmdTemp1, &c_stackSize, cstack);
      cmdTemp1 = NULL;
      word = NULL;
      ts_push(ts_stack, tsCur);

    }else if (ttCur == DONE){
      if (countUntils == 0 && countWhiles == 0) {
        writeError("commandBuilder()", NULL);
      }
      c_push(cmdTemp1, &c_stackSize, cstack);

      enum token_type ttTemp1 = ts_peek(ts_stack);
      if (ttTemp1 == OTHER){
        writeError("commandBuilder()", NULL);
      }else if (ttTemp1 == DO){
        cmdTemp1 = c_pop( cstack);
        ts_pop(ts_stack);
      }

      ttTemp1 = ts_peek(ts_stack);
      if (ttTemp1 == OTHER ){
        writeError("commandBuilder()", NULL);
      }else if (ttTemp1 == WHILE || ttTemp1 == UNTIL){
        cmdTemp2 = c_pop( cstack);
      }
      
      size_t struct_command_size = sizeof(struct command);
      cmdTemp3 = (command_t) checked_malloc(struct_command_size);
      cmdTemp3->status = -1;
      cmdTemp3->u.word = NULL;
      cmdTemp3->input = NULL;
      cmdTemp3->output = NULL;
      ttTemp1 = ts_peek(ts_stack);
      if (ttTemp1 != WHILE){
        cmdTemp3->type = UNTIL_COMMAND;
        countUntils--;  
      }else{
        cmdTemp3->type = WHILE_COMMAND;
        countWhiles--;
      }
      cmdTemp3->u.command[0] = cmdTemp2;
      cmdTemp3->u.command[1] = cmdTemp1;
            
      c_push(cmdTemp3, &c_stackSize, cstack);

      cmdTemp1 = cmdTemp2 = cmdTemp3 = NULL;
      word = NULL;
      ts_pop(ts_stack); 

    }else if (ttCur == FI){
      if (countIfs == 0){
        writeError("commandBuilder()", NULL);
      }
      c_push(cmdTemp1,  &c_stackSize, cstack);

      enum token_type ttTemp2 = ts_peek(ts_stack);
      if (ttTemp2 == OTHER ){
        writeError("commandBuilder()", NULL);
      }else if (ttTemp2 == ELSE){
        cmdC = c_pop( cstack);
        ts_pop(ts_stack);
      }

      ttTemp2 = ts_peek(ts_stack);
      if (ttTemp2 == OTHER){
        writeError("commandBuilder()", NULL);
      }else if (ttTemp2 == THEN){
        cmdB = c_pop( cstack);
        ts_pop(ts_stack);
      }
      
      ttTemp2 = ts_peek(ts_stack);
      if (ttTemp2 == OTHER){
        writeError("commandBuilder()", NULL);
      }else if (ttTemp2 == IF){
        cmdA = c_pop( cstack);
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

      c_push(cmdTemp1, &c_stackSize, cstack);
      countIfs--; 

      cmdA = cmdB = cmdC = cmdTemp1 = NULL;
      word = NULL;
      ts_pop(ts_stack); 

    }else if (ttCur == NEW_LINE){
      c_push(cmdTemp1, &c_stackSize, cstack);
      while (precedence(ts_peek(ts_stack), 1) > precedence(tsCur->token_node.type, 0)){
        cmdB = c_pop( cstack);
        cmdA = c_pop(cstack);
        cmdTemp1 = combine_commands(cmdA, cmdB, ts_pop(ts_stack));
        c_push(cmdTemp1, &c_stackSize, cstack);
      }
      if (countParens == 0 && countIfs == 0 && countWhiles == 0 && countUntils == 0){
        csTemp1 = (command_stream_t) checked_malloc(sizeof(struct command_stream));
        csTemp1->command_node = c_pop( cstack);
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

  c_push(cmdTemp1, &c_stackSize, cstack);
  
  while (ts_peek(ts_stack) != OTHER){
    cmdB = c_pop( cstack);
    cmdA = c_pop( cstack);
    cmdTemp1 = combine_commands(cmdA, cmdB, ts_pop(ts_stack));
    c_push(cmdTemp1,  &c_stackSize, cstack);
  }

  csTemp1 = (command_stream_t) checked_malloc(sizeof(struct command_stream));
  csTemp1->command_node = c_pop( cstack);

  csTemp2 = combineStreams(csTemp2, csTemp1);
  cmdTemp1 = NULL;

  if (c_pop( cstack) != NULL){
    writeError("commandBuilder()", NULL);
  }

  return csTemp2;

} 




int check_if_word(char c) {
    if (isalnum(c) || (c == '!') || (c == '%') || (c == '!') || (c == '+') || (c == '-') || (c == ',') || (c == '.') || (c == '/') || (c == ':') || (c == '@') || (c == '^') || (c == '_'))
        return 1;

    return 0;
}


command_t
combine_commands(command_t cmd_A, command_t cmd_B, token_stream_t tstream)
{

  size_t command_struct_size = sizeof(struct command);
  command_t join_commands = (command_t) checked_malloc(command_struct_size);


  join_commands->status = -1;
  join_commands->type = SIMPLE_COMMAND;
  join_commands->u.word = NULL;
  join_commands->input = NULL;
  join_commands->output = NULL;
  join_commands->u.command[0] = cmd_A;
  join_commands->u.command[1] = cmd_B;

  enum token_type tstream_type = tstream->token_node.type;
  if (tstream == NULL)
  {
    fprintf(stderr, "Cannot pop() an empty stack. Error\n");
    exit(1);
  }
  else if (tstream_type == PIPE) 
    join_commands->type = PIPE_COMMAND;
  else if (tstream_type == CLOSE_PAREN)
    join_commands->type = SUBSHELL_COMMAND;
  else if (tstream_type == SEMICOLON)
    join_commands->type = SEQUENCE_COMMAND;
  else 
  {
    fprintf(stderr, "Joining commands has an error; the token %i is invalid", tstream_type); 
    exit(1);
  }

  return join_commands;
}


command_stream_t combineStreams(command_stream_t cStream1, command_stream_t cStream2){
  command_stream_t final = NULL;
  if (cStream2 == NULL){
    final = cStream1;
  }else if (cStream1 != NULL){
    command_stream_t cTemp1 = cStream1 -> prev;
    int temp = cTemp1->number;
    cStream2->number = temp + 1;
    cStream2->prev = cTemp1;
    (cStream1->prev)->next = cStream2;
    cStream1->prev = cStream2;
    final = cStream1;
  }else{
    cStream1 = cStream2;
    cStream1->prev = cStream2;
    cStream1->number = 1;
    final = cStream1;
  }
  return final;
}
