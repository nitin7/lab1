  
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
        // fprintf(stderr, "Error. The stack is full.\n");
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



void c_push(command_t item, int *top, size_t *size);
command_t c_pop(int *top);

//-----------------------------------------------------------------------------
//
// Helper functions used in make_command_stream()
//
//-----------------------------------------------------------------------------

// Reads in the input file stream into a buffer for easier processing
char* read_into_buffer(int (*get_next_byte) (void *), 
                       void *get_next_byte_argument);

// Parses the input file commands into tokens
token_stream_t make_tokens_from_bytes(char *buffer);

// Checks for syntax errors
void validate_tokens(token_stream_t tstream);

command_stream_t
tokens_to_command_stream(token_stream_t tstream);

//Checks to see whether the input character is a valid word character
int check_if_word(char ch);

command_t
combine_commands(command_t cmd_A, command_t cmd_B, token_stream_t tstream);

//
command_stream_t
append_to_cstream(command_stream_t cstream, command_stream_t item);

//-----------------------------------------------------------------------------
// 
// The major functions
//
//-----------------------------------------------------------------------------

command_stream_t
make_command_stream (int (*get_next_byte) (void *),
         void *get_next_byte_argument)
{
  char *buffer = read_into_buffer(get_next_byte, get_next_byte_argument);
  
  // UPDATE to typedef later for readability
  struct token_stream *tstream;
  struct command_stream *cstream;

  tstream = make_tokens_from_bytes(buffer);
  
  if (tstream == NULL)
  { 
    fprintf(stderr, "%s\n", "Function make_command_stream() has an error; no tokens found");
    return NULL;
  }
  
  // Before changing some '\n' to ';'

  validate_tokens(tstream);

  // After changing some '\n' to ';'

  cstream = tokens_to_command_stream(tstream);

  return cstream;
}


command_t
read_command_stream (command_stream_t s)
{
  if (s == NULL)
    return NULL;
  else if (s->iterator == 0)  // 0 = node has not been iterated over yet
  {
    s->iterator = 1;
    return (s->command_node);
  }
  else if (s->next != NULL)
    return read_command_stream(s->next);
  else
    return NULL;
}

//-----------------------------------------------------------------------------
//
// Function definitions
//
//-----------------------------------------------------------------------------

char* read_into_buffer(int (*get_next_byte) (void *), 
                       void *get_next_byte_argument)
{
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


token_stream_t make_tokens_from_bytes(char *buf)
{
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

      if (check_if_word(ch))
      {
        while (check_if_word(*(itr + word_n_chars)))
          word_n_chars++;
        itr += word_n_chars - 1;
        tk_type = WORD;
      }
      else
      {
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
    else if (tk_type == WORD)
    {
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


void validate_tokens(token_stream_t tStream)
{
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
    int semicolonCheck = ((tNext == OPEN_PAREN || tNext == CLOSE_PAREN 
              || tNext == WORD) && ((numParens != 0 && tPrev != OPEN_PAREN) ||

                (numIf != 0 && !(tPrev == IF || 
                  tPrev == THEN ||
                  tPrev == ELSE)) ||

                (numDone != 0 && !(tPrev == WHILE || 
                  tPrev == UNTIL || tPrev == DO)))); 

    enum token_type ttCur = tsCur->token_node.type;
    if (ttCur == WORD){
      if (wordCheck){
            tNext = WORD;
          }
    }else if (ttCur == SEMICOLON){
      if (tNext == SEMICOLON) {
        fprintf(stderr, "Line %i in function validate() has an error: SEMICOLON cannot be followed by another semicolon\n", tsCur->token_node.line_no);
        exit(1);
      }
      if (tsCur == tStream) {
        fprintf(stderr, "Line %i in function validate() has an error: SEMICOLON cannot be the first token\n", tsCur->token_node.line_no);
        exit(1);
      }
    }else if (ttCur == PIPE){
      if (tsCur->token_node.type == tNext) {
        fprintf(stderr, "Line %i in function validate() has an error: PIPE op cannot have repeated token of type %i\n", tsCur->token_node.line_no, tNext);
        exit(1);
      }
      if (tsCur == tStream) {
        fprintf(stderr, "Line %i in function validate() has an error: PIPE op cannot be first token\n", tsCur->token_node.line_no);
        exit(1);
      }
      if (tNext == SEMICOLON) {
        fprintf(stderr, "Line %i in function validate() has an error: PIPE op cannot be followed by another PIPE op\n", tsCur->token_node.line_no);
        exit(1);
      } 
    }else if (ttCur == OPEN_PAREN){
      if (tNext == CLOSE_PAREN)
      {
        fprintf(stderr, "Line %i in function validate() has an error: shouldn't have an empty subcommand\n", tsCur->token_node.line_no);
        exit(1);
      }
      numParens++;
    }else if (ttCur == CLOSE_PAREN){
      numParens--;
    }else if (ttCur == LESS_THAN || ttCur == GREATER_THAN){
      if (tNext != WORD || tsNext == NULL) {
        fprintf(stderr, "Line %i in function validate() has an error: expected word following redirection op\n", tsCur->token_node.line_no);
        exit(1);
      }
    }else if (ttCur == NEW_LINE && tsNext != NULL){
      if (semicolonCheck){ 
        tsCur->token_node.type = SEMICOLON;
      }else{
        if (tNext != IF && tNext != FI && tNext != THEN && tNext != ELSE && tNext != OPEN_PAREN && tNext != CLOSE_PAREN && tNext != WORD && tNext != DO && tNext != DONE && tNext != WHILE && tNext != UNTIL){
          fprintf(stderr, "Line %i in function validate() has an error: Newlines can only appear before certain tokens\n", tsCur->token_node.line_no);
          exit(1);
        }
      }
    }else if (ttCur == THEN || ttCur == ELSE){
      if (tsNext == NULL || tNext == FI) {
        fprintf(stderr, "Line %i in function validate() has an error: special words can't have tokens of type %i right after/next token is null\n", tsCur->token_node.line_no, tNext);
        exit(1);
      }
      if(ttCur == DO){
        if (tPrev == WORD){
          tsCur->token_node.type = WORD;
        }
        if (tsCur->token_node.type == tNext) {
          fprintf(stderr, "Line %i in function validate() has an error: can't have repeated token of type %i\n", tsCur->token_node.line_no, tNext);
          exit(1);
        }   
        if (tPrev != NEW_LINE && tPrev != SEMICOLON) {
          fprintf(stderr, "Line %i in function validate() has an error: special words must have a ';' or '\\n' after\n", tsCur->token_node.line_no);
          exit(1);
        }         
        if (tsNext == NULL || tNext == SEMICOLON ) {
          fprintf(stderr, "Line %i in function validate() has an error: special words can't have tokens of type %i right after/next token is null\n", tsCur->token_node.line_no, tNext);
          exit(1);
        }
      }
    }else if (ttCur == DO){
      if (tPrev == WORD){
        tsCur->token_node.type = WORD;
      }
      if (tsCur->token_node.type == tNext) {
        fprintf(stderr, "Line %i in function validate() has an error: can't have repeated token of type %i\n", tsCur->token_node.line_no, tNext);
        exit(1);
      }   
      if (tPrev != NEW_LINE && tPrev != SEMICOLON) {
        fprintf(stderr, "Line %i in function validate() has an error: special words must have a ';' or '\\n' after\n", tsCur->token_node.line_no);
        exit(1);
      }         
      if (tsNext == NULL || tNext == SEMICOLON ) {
        fprintf(stderr, "Line %i in function validate() has an error: special words can't have tokens of type %i right after/next token is null\n", tsCur->token_node.line_no, tNext);
        exit(1);
      }
    }else{
      switch (ttCur){
        case DONE:
        case FI:
          if (tNext == WORD){
            fprintf(stderr, "Line %i in function validate() has an error: can't have a word right after DONE, FI\n", tsCur->token_node.line_no);
            exit(1);
          }
        case IF:      
        case WHILE:
        case UNTIL:
          if (tNext == SEMICOLON || tsNext == NULL) {
            fprintf(stderr, "Line %i in function validate() has an error: special words can't have tokens of type %i right after/next token is null\n", tsCur->token_node.line_no, tNext);
            exit(1);
          }
          if (tPrev == WORD){
            tsCur->token_node.type = WORD;
          }
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

  if(numDone != 0){
    fprintf(stderr, "Function validate() has an error: To many done tokens\n");
    exit(1);
  }

  if(numIf != 0){
    fprintf(stderr, "Function validate() has an error: Too many IFs\n");
    exit(1);
  }

  if(numParens != 0){
    fprintf(stderr, "Function validate() has an error: Parenthesis mismatch\n");
    exit(1);
  }
}

int precedence(enum token_type type, int stack)
{
  if(stack){
    if (type == ELSE){
      return 0*OFFSET;
    }else if (type == GREATER_THAN || type == LESS_THAN){
      return 5*OFFSET;
    }else if (type == PIPE){
      return 6*OFFSET;
    }else if (type == SEMICOLON || type == NEW_LINE){
      return 7*OFFSET;
    }else if (type == DO || type == THEN){
      return -1*OFFSET;
    }else if (type == IF || type == WHILE || type == UNTIL){
      return -2*OFFSET;
    }else if (type == OPEN_PAREN){
      return -3*OFFSET;
    }else{
      return -5*OFFSET;
    }
  }else{
    if (type == ELSE){
      return 6*OFFSET+1;
    }else if (type == GREATER_THAN || type == LESS_THAN){
      return 3*OFFSET+1;
    }else if (type == PIPE){
      return 2*OFFSET+1;
    }else if (type == SEMICOLON || type == NEW_LINE){
      return OFFSET+1;
    }else if (type == DO || type == THEN){
      return 7*OFFSET+1;
    }else if (type == IF || type == WHILE || type == UNTIL){
      return 8*OFFSET+1;
    }else if (type == OPEN_PAREN){
      return 9*OFFSET+1;
    }else{
      return -5*OFFSET;
    }
  }
}

command_stream_t tokens_to_command_stream(token_stream_t tStream)
{
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

  int top = -1;
  size_t c_stackSize = 10 * sizeof(command_t);
  c_stack = (command_t *) checked_malloc(c_stackSize);

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
      c_push(cmdTemp1, &top, &c_stackSize);

      while (precedence(ts_peek(ts_stack), 1) > precedence(tsCur->token_node.type, 0)){
        cmdB = c_pop(&top);
        cmdA = c_pop(&top);
        cmdTemp1 = combine_commands(cmdA, cmdB, ts_pop(ts_stack));
        c_push(cmdTemp1, &top, &c_stackSize);
      }
      cmdTemp1 = NULL;
      word = NULL;
      if (tokenNext != THEN && tokenNext != ELSE && tokenNext != FI && tokenNext != DO &&
        tokenNext != DONE){
        ts_push(ts_stack, tsCur);
      }

    }else if (ttCur == PIPE){
      c_push(cmdTemp1, &top, &c_stackSize);
      while (precedence(ts_peek(ts_stack), 1) > precedence(tsCur->token_node.type, 0)){
        cmdB = c_pop(&top);
        cmdA = c_pop(&top);
        cmdTemp1 = combine_commands(cmdA, cmdB, ts_pop(ts_stack));
        c_push(cmdTemp1, &top, &c_stackSize);
      }
      cmdTemp1 = cmdB = cmdA = NULL;
      word = NULL;
      ts_push(ts_stack, tsCur);

    }else if (ttCur == OPEN_PAREN){
      c_push(cmdTemp1, &top, &c_stackSize);
      cmdTemp1 = NULL;
      word = NULL;
      countParens++;
      ts_push(ts_stack, tsCur);

    }else if (ttCur == CLOSE_PAREN){
      if (countParens != 0) {
        c_push(cmdTemp1, &top, &c_stackSize);
        countParens--;
        while (ts_peek(ts_stack) != OPEN_PAREN){
          if (ts_peek(ts_stack) != OTHER){
            cmdB = c_pop(&top);
            cmdA = c_pop(&top);
            cmdTemp1 = combine_commands(cmdA, cmdB, ts_pop(ts_stack));  
            c_push(cmdTemp1, &top, &c_stackSize);
            cmdTemp1 = NULL;         
          }else{
            fprintf(stderr, "Error in tokens_to_command_stream(): Unknown token returned in CLOSE_PAREN case\n");
            exit(1);
          }      
        }
        size_t struct_command_size = sizeof(struct command);
        cmdTemp2 = (command_t) checked_malloc(struct_command_size);
        cmdTemp2->status = -1;
        cmdTemp2->u.word = NULL;
        cmdTemp2->input = NULL;
        cmdTemp2->output = NULL;
        cmdTemp2->type = SUBSHELL_COMMAND;
        cmdTemp2->u.command[0] = c_pop(&top); 
        c_push(cmdTemp2, &top, &c_stackSize);
        cmdTemp1 = cmdTemp2 = NULL;
        word = NULL;
        ts_pop(ts_stack); 
      }else{
        fprintf(stderr, "Error in tokens_to_command_stream(): unmatched ')'\n");
        exit(1);
      }
      
    }else if (ttCur == LESS_THAN || ttCur == GREATER_THAN){
      c_push(cmdTemp1, &top, &c_stackSize);
      if (tsNext == NULL || tsNext->token_node.type != WORD){
        fprintf(stderr, "Error in tokens_to_command_stream(): wrong token type after redirection operator\n");
        exit(1);   
      }else{
        cmdTemp1 = c_pop(&top);
        if (cmdTemp1 != NULL) {
          if (tsCur->token_node.type == GREATER_THAN)
            cmdTemp1->output = tsNext->token_node.t_word;
          else if (tsCur->token_node.type == LESS_THAN)
            cmdTemp1->input = tsNext->token_node.t_word;
          c_push(cmdTemp1, &top, &c_stackSize);
          cmdTemp1 = NULL;
          word = NULL;
          tsCur = tsNext; 
          if (tsCur != NULL)
            tsNext = tsCur->next; 
        }else{
          fprintf(stderr, "Error in tokens_to_command_stream(): wrong command type left of redirection operator\n");
          exit(1);
        }      
      }

    }else if (ttCur == WHILE || ttCur == UNTIL || ttCur == IF){
      c_push(cmdTemp1, &top, &c_stackSize);
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
      c_push(cmdTemp1, &top, &c_stackSize);
      cmdTemp1 = NULL;
      word = NULL;
      ts_push(ts_stack, tsCur);

    }else if (ttCur == DONE){
      if (countUntils == 0 && countWhiles == 0) {
        fprintf(stderr, "Error in tokens_to_command_stream(): unmatched 'done' for while' or 'until' command\n");
        exit(1);
      }
      c_push(cmdTemp1, &top, &c_stackSize);

      enum token_type ttTemp1 = ts_peek(ts_stack);
      if (ttTemp1 == OTHER){
        fprintf(stderr, "Error in tokens_to_command_stream(): null token returned in DONE case\n");
        exit(1);
      }else if (ttTemp1 == DO){
        cmdTemp1 = c_pop(&top);
        ts_pop(ts_stack);
      }

      ttTemp1 = ts_peek(ts_stack);
      if (ttTemp1 == OTHER ){
        fprintf(stderr, "Error in tokens_to_command_stream(): null token returned in DONE case\n");
        exit(1);
      }else if (ttTemp1 == WHILE || ttTemp1 == UNTIL){
        cmdTemp2 = c_pop(&top);
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
            
      c_push(cmdTemp3, &top, &c_stackSize);

      cmdTemp1 = cmdTemp2 = cmdTemp3 = NULL;
      word = NULL;
      ts_pop(ts_stack); 

    }else if (ttCur == FI){
      if (countIfs == 0){
        fprintf(stderr, "Error in tokens_to_command_stream(): unmatched 'fi' for 'IF' command\n");
        exit(1);
      }
      c_push(cmdTemp1, &top, &c_stackSize);

      enum token_type ttTemp2 = ts_peek(ts_stack);
      if (ttTemp2 == OTHER ){
        fprintf(stderr, "Error in tokens_to_command_stream(): unknown token returned in FI case\n");
        exit(1);
      }else if (ttTemp2 == ELSE){
        cmdC = c_pop(&top);
        ts_pop(ts_stack);
      }

      ttTemp2 = ts_peek(ts_stack);
      if (ttTemp2 == OTHER){
        fprintf(stderr, "Error in tokens_to_command_stream(): unknown token returned in FI case\n");
        exit(1);
      }else if (ttTemp2 == THEN){
        cmdB = c_pop(&top);
        ts_pop(ts_stack);
      }
      
      ttTemp2 = ts_peek(ts_stack);
      if (ttTemp2 == OTHER){
        fprintf(stderr, "Error in tokens_to_command_stream(): unknown token returned in FI case\n");
        exit(1);
      }else if (ttTemp2 == IF){
        cmdA = c_pop(&top);
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

      c_push(cmdTemp1, &top, &c_stackSize);
      countIfs--; 

      cmdA = cmdB = cmdC = cmdTemp1 = NULL;
      word = NULL;
      ts_pop(ts_stack); 

    }else if (ttCur == NEW_LINE){
      c_push(cmdTemp1, &top, &c_stackSize);
      while (precedence(ts_peek(ts_stack), 1) > precedence(tsCur->token_node.type, 0)){
        cmdB = c_pop(&top);
        cmdA = c_pop(&top);
        cmdTemp1 = combine_commands(cmdA, cmdB, ts_pop(ts_stack));
        c_push(cmdTemp1, &top, &c_stackSize);
      }
      if (countParens == 0 && countIfs == 0 && countWhiles == 0 && countUntils == 0){
        csTemp1 = (command_stream_t) checked_malloc(sizeof(struct command_stream));
        csTemp1->command_node = c_pop(&top);
        csTemp2 = append_to_cstream(csTemp2, csTemp1);
      }
      cmdTemp1 = NULL;
      word = NULL;
    }
    tsCur = tsNext;
  } 

  if (countParens != 0 && countIfs != 0 && countWhiles != 0 && countUntils != 0){
    fprintf(stderr, "%s\n", "Error in tokens_to_command_stream(): out of while loop, closing pairs don't match");
    exit(1);
  }

  c_push(cmdTemp1, &top, &c_stackSize);
  
  while (ts_peek(ts_stack) != OTHER){
    cmdB = c_pop(&top);
    cmdA = c_pop(&top);
    cmdTemp1 = combine_commands(cmdA, cmdB, ts_pop(ts_stack));
    c_push(cmdTemp1, &top, &c_stackSize);
  }

  csTemp1 = (command_stream_t) checked_malloc(sizeof(struct command_stream));
  csTemp1->command_node = c_pop(&top);

  csTemp2 = append_to_cstream(csTemp2, csTemp1);
  cmdTemp1 = NULL;

  if (c_pop(&top) != NULL){
    fprintf(stderr, "%s\n","Error in tokens_to_command_stream(): c_stack should be empty");
    exit(1);
  }

  return csTemp2;

} 


void c_push(command_t item, int *top, size_t *size)
{
  if (item == NULL)
    return;

  // Empty stack has top at -1
  if (*size <= (*top + 1) * sizeof(command_t))
    c_stack = (command_t *) checked_grow_alloc(c_stack, size);

  (*top)++;
  c_stack[*top] = item;

  // printf("PUSHED in a COMMAND of type %d into c_stack[%d]\n", item->type, 
  // *top);
}


command_t 
c_pop(int *top)
{
  if (*top == -1) // If c_stack is empty
    return NULL;

  command_t item = NULL;
  item = c_stack[*top];
  
  // printf("POPPED out a COMMAND of type %d out of c_stack[%d]\n", 
  // item->type, *top);

  (*top)--;
  return item;
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

command_stream_t
append_to_cstream(command_stream_t cstream, command_stream_t item)
{
  if (item == NULL)
    return cstream;

  if (cstream == NULL)
  {
    cstream = item;
    cstream->prev = item;
    cstream->next = NULL;
    cstream->number = 1;
    cstream->iterator = 0;
  }
  else // cstream already has command_stream_t nodes in it so append to end
  {
    item->number = (cstream->prev->number) + 1;
    item->prev = cstream->prev;
    item->next = NULL;
    item->iterator = 0;

    (cstream->prev)->next = item;
    cstream->prev = item;
  }
  return cstream;
}
