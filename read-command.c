  
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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <error.h>

//-----------------------------------------------------------------------------
// 
// The major functions:
//
//    (1) make_command_stream(): returns a command stream from tokenized shell
//        commands. The shell commands are fed in through an input file
//            Logic:
//              (a) Read file input into a char buffer
//              (b) Tokenize buffer into a token_stream list
//              (c) Validate tokens in token_stream
//              (d) Turn token_stream into a command_stream
//
//    (2) read_command_stream(): returns a pointer to the current command 
//        stream node so that the commands get printed out in main()
//        recursively
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// 
// To build the command stream, two "stacks" (a token and command stack) were 
// used to determine the correct order of the commands by precedence. Please // see tokens_to_command_stream() for the general algorithm, and the comments
// before the stack function implementation for more details.
// 
//-----------------------------------------------------------------------------

token_stream_t ts_stack = NULL;     // struct token_stream *ts_stack
command_t *c_stack = NULL;          // struct command **c_stack

void ts_push(token_stream_t item);
enum token_type ts_peek();
token_stream_t ts_pop();

void c_push(command_t item, int *top, size_t *size);
command_t c_pop(int *top);

int stack_precedence(enum token_type type);
int stream_precedence(enum token_type type);

//-----------------------------------------------------------------------------
//
// Helper functions used in make_command_stream()
//
//-----------------------------------------------------------------------------

// Reads in the input file stream into a buffer for easier processing
char* read_into_buffer(int (*get_next_byte) (void *), 
                       void *get_next_byte_argument);

// Parses the input file commands into tokens
token_stream_t tokenize(char *buffer);

// Outputs the tokens that are parsed
void display_tokens(token_stream_t tstream);

// Checks for syntax errors
void validate_tokens(token_stream_t tstream);

command_stream_t
tokens_to_command_stream(token_stream_t tstream);

//Checks to see whether the input character is a valid word character
int check_if_word(char ch);

//
command_t
new_command();

//
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

  tstream = tokenize(buffer);
  
  if (tstream == NULL)
  { 
    fprintf(stderr, "%s\n", "Error in make_command_stream(): returning an empty token stream");
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
    return (s->m_command);
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
  char ch;
  size_t size = 1000;
  size_t index = 0;

  // Although could have read the file stream directly into a token stream to // save space, in the end, decided to first read it into a a char buffer 
  // because it is less error prone and array indices are easier to work with.
  char *buffer = (char *) checked_malloc(sizeof(char) * size);

  while ((ch = get_next_byte(get_next_byte_argument)) != EOF) 
  {
    buffer[index] = ch;
    index++;

    if (index == size)
      buffer = (char *) checked_grow_alloc(buffer, &size);
  }

  // Case where size = 10, index = 9. Have to make room for a '\0' at the end
  if (index == size - 1)
    buffer = (char *) checked_grow_alloc(buffer, &size);

  // Close off string
  buffer[index] = '\0';

  return buffer;

} // END of read_into_buffer()


token_stream_t tokenize(char *buffer)
{
  int index = 0;
  int lineNumber = 1;

  enum token_type type;
  
  // UPDATE to typedef later for readability
  struct token_stream *ts_head = NULL;
  struct token_stream *ts_cur = ts_head;

  // If buffer is empty, we can't tokenize
  if (buffer[index] == '\0')
    return NULL;

  // Remove leading newlines
  while (buffer[index] == '\n')
  {
    lineNumber++;
    index++;
  }

  char firstChar;  

  // Identify the tokens
  while (buffer[index] != '\0')
  {
    firstChar = buffer[index];
    switch (firstChar)
    {
      case '#':
              if (index != 0 && check_if_word(buffer[index-1]))
              {
                fprintf(stderr, "Error in tokenize(): '#' at line %i can't be proceeded by an ordinary token\n", lineNumber);
                exit(1);
              }
              else
              {  
                int skip = 1;
                while ((buffer[index + skip] != '\n') && 
                       (buffer[index + skip] != '\0'))
                  skip++;
                
                // Plus one to skip the '\n' at the end of the comment  
                index = index + skip + 1; 
                continue;
                break;
              }

      case '(':
              type = LEFT_PAREN_TOKEN;
              break;
      case ')':
              type = RIGHT_PAREN_TOKEN;
              break;
      case '<':
              type = LESS_TOKEN;
              break;
      case '>':
              type = GREATER_TOKEN;
              break;
      case ';':
              type = SEMICOLON_TOKEN;
              break;
      case '|':
              type = PIPE_TOKEN;
              break;

      case ' ': // Skip whitespace
      case '\t':
              index++;
              continue;  
              break;

      case '\n':
              type = NEWLINE_TOKEN;
              lineNumber++;
              
              if (buffer[index + 1] != '\0' && buffer[index + 1] == '\n')
              {
                index++; // Remove adjacent newlines
                continue; 
              }
              break;    

      // At this point, all words and special words such as 'if', 'else', etc.
      // are not differentiated. They are sorted out below.
      default:
              type = UNKNOWN_TOKEN;

    } //end switch statement

    int wordLength = 1;
    int oldIndex = index;

    // If it is a valid word character, find out its length so we can move
    // past it in the next iteration of the loop
    if (check_if_word(firstChar))
    {
      type = WORD_TOKEN;

      while (check_if_word(buffer[index + wordLength]))
        wordLength++;

      index += wordLength - 1;
   }

    // Finally, insert the token into the token stream
    // UPDATE to typedef later for readability
    struct token_stream *ts_temp = (struct token_stream *) 
                                  checked_malloc(sizeof(struct token_stream));
    ts_temp->prev = NULL;
    ts_temp->next = NULL;
    ts_temp->m_token.type = type;
    ts_temp->m_token.length = wordLength;

    // Whenever we see a newline token, we increment the line number by one
    // so other tokens are on the next line. However, the newline itself
    // must be on the previous lineNumber.
    if (type == NEWLINE_TOKEN)
      ts_temp->m_token.line_num = lineNumber - 1;
    else
      ts_temp->m_token.line_num = lineNumber;

    // Differentiate between the word tokens. Either a regular word token or a // special compound command token such as 'if', 'then', 'while', etc.

    // COMPLETED in validate_tokens()
    // Possible issue: will need to differentiate special words in two 
    // scenarios
    //    (a) 'if' of compound commands 
    //    (b) 'if' as arguments to simple commands like in 'cat if done'
    if (type == WORD_TOKEN)
    {
      ts_temp->m_token.t_word = (char *) checked_malloc(sizeof(char) * wordLength + 1);
      
      int i = 0;
      while (i < wordLength)
      {
        ts_temp->m_token.t_word[i] = buffer[oldIndex + i];
        i++;
      }

      ts_temp->m_token.t_word[i] = '\0';

      if (strstr(ts_temp->m_token.t_word, "if") != NULL && wordLength == 2)
        ts_temp->m_token.type = IF_TOKEN;

      else if (strstr(ts_temp->m_token.t_word, "then") != NULL && wordLength == 4) 
        ts_temp->m_token.type = THEN_TOKEN;

      else if (strstr(ts_temp->m_token.t_word, "else") != NULL && wordLength == 4) 
        ts_temp->m_token.type = ELSE_TOKEN;

      else if (strstr(ts_temp->m_token.t_word, "fi") != NULL && wordLength == 2) 
        ts_temp->m_token.type = FI_TOKEN;

      else if (strstr(ts_temp->m_token.t_word, "while") != NULL && wordLength == 5) 
        ts_temp->m_token.type = WHILE_TOKEN;

      else if (strstr(ts_temp->m_token.t_word, "do") != NULL && wordLength == 2) 
        ts_temp->m_token.type = DO_TOKEN;

      else if (strstr(ts_temp->m_token.t_word, "done") != NULL && wordLength == 4) 
        ts_temp->m_token.type = DONE_TOKEN;\
      
      else if (strstr(ts_temp->m_token.t_word, "until") != NULL && wordLength == 5) 
        ts_temp->m_token.type = UNTIL_TOKEN;
    
    } // finished identifying WORD_TOKENS and special word tokens

    else if (type == UNKNOWN_TOKEN)
    {
      fprintf(stderr, "Error in tokenize(): unidentified token %c at line %i\n", firstChar, lineNumber);
      ts_temp->m_token.t_word = NULL;
      exit(1);
    }
    else
      ts_temp->m_token.t_word = NULL;

    // Insert node token into token stream
    if (ts_cur == NULL)
    {
      ts_head = ts_temp;
      ts_cur = ts_head;
    }
    else
    {
      ts_temp->prev = ts_cur;
      ts_temp->next = NULL;
      
      ts_cur->next = ts_temp;
      ts_cur = ts_temp;
    }

    index++;
  } // end while loop

  return ts_head; 

} // END of tokenize()


void display_tokens(token_stream_t tstream)
{
  token_stream_t ts_cur = tstream;
  char token_type[20];

  while (ts_cur != NULL) 
  {
    switch (ts_cur->m_token.type)
    {
      case 0: strcpy(token_type, "WORD_TOKEN");break;
      case 1: strcpy(token_type, "SEMICOLON_TOKEN");break;
      case 2: strcpy(token_type, "PIPE_TOKEN");break;
      case 3: strcpy(token_type, "LEFT_PAREN_TOKEN");break;
      case 4: strcpy(token_type, "RIGHT_PAREN_TOKEN");break;
      case 5: strcpy(token_type, "LESS_TOKEN");break;
      case 6: strcpy(token_type, "GREATER_TOKEN");break;
      case 7: strcpy(token_type, "IF_TOKEN");break;
      case 8: strcpy(token_type, "THEN_TOKEN");break;
      case 9: strcpy(token_type, "ELSE_TOKEN");break;
      case 10: strcpy(token_type, "FI_TOKEN");break;
      case 11: strcpy(token_type, "WHILE_TOKEN");break;
      case 12: strcpy(token_type, "DO_TOKEN");break;
      case 13: strcpy(token_type, "DONE_TOKEN");break;
      case 14: strcpy(token_type, "UNTIL_TOKEN");break;
      case 15: strcpy(token_type, "NEWLINE_TOKEN");break;
      case 16: strcpy(token_type, "UNKNOWN_TOKEN");break;
      case 17: strcpy(token_type, "UNKNOWN_TOKEN");break;
      default: strcpy(token_type, "ERROR in display()");break;
    }
    
    printf("line %i: %s", ts_cur->m_token.line_num, token_type);

    if (ts_cur->m_token.type == WORD_TOKEN)
      printf(" - %s", ts_cur->m_token.t_word);
    
    printf("\n");

    ts_cur = ts_cur->next;
  }

} // END of display_tokens()


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
    tNext = (tsNext == NULL) ? UNKNOWN_TOKEN : tsNext->m_token.type;
    tsPrev = tsCur->prev;
    tPrev = (tsPrev == NULL) ? UNKNOWN_TOKEN : tsPrev->m_token.type;

    int wordCheck = tNext == IF_TOKEN || tNext == FI_TOKEN || tNext == THEN_TOKEN || tNext == ELSE_TOKEN || tNext == DO_TOKEN || tNext == DONE_TOKEN || tNext == UNTIL_TOKEN || tNext == WHILE_TOKEN;
    int semicolonCheck = ((tNext == LEFT_PAREN_TOKEN || tNext == RIGHT_PAREN_TOKEN 
              || tNext == WORD_TOKEN) && ((numParens != 0 && tPrev != LEFT_PAREN_TOKEN) ||

                (numIf != 0 && !(tPrev == IF_TOKEN || 
                  tPrev == THEN_TOKEN ||
                  tPrev == ELSE_TOKEN)) ||

                (numDone != 0 && !(tPrev == WHILE_TOKEN || 
                  tPrev == UNTIL_TOKEN || tPrev == DO_TOKEN)))); 

    enum token_type ttCur = tsCur->m_token.type;
    if (ttCur == WORD_TOKEN){
      if (wordCheck){
            tNext = WORD_TOKEN;
          }
    }else if (ttCur == SEMICOLON_TOKEN){
      if (tNext == SEMICOLON_TOKEN) {
        fprintf(stderr, "Line %i in function validate() has an error: SEMICOLON_TOKEN cannot be followed by another semicolon\n", tsCur->m_token.line_num);
        exit(1);
      }
      if (tsCur == tStream) {
        fprintf(stderr, "Line %i in function validate() has an error: SEMICOLON_TOKEN cannot be the first token\n", tsCur->m_token.line_num);
        exit(1);
      }
    }else if (ttCur == PIPE_TOKEN){
      if (tsCur->m_token.type == tNext) {
        fprintf(stderr, "Line %i in function validate() has an error: PIPE op cannot have repeated token of type %i\n", tsCur->m_token.line_num, tNext);
        exit(1);
      }
      if (tsCur == tStream) {
        fprintf(stderr, "Line %i in function validate() has an error: PIPE op cannot be first token\n", tsCur->m_token.line_num);
        exit(1);
      }
      if (tNext == SEMICOLON_TOKEN) {
        fprintf(stderr, "Line %i in function validate() has an error: PIPE op cannot be followed by another PIPE op\n", tsCur->m_token.line_num);
        exit(1);
      } 
    }else if (ttCur == LEFT_PAREN_TOKEN){
      if (tNext == RIGHT_PAREN_TOKEN)
      {
        fprintf(stderr, "Line %i in function validate() has an error: shouldn't have an empty subcommand\n", tsCur->m_token.line_num);
        exit(1);
      }
      numParens++;
    }else if (ttCur == RIGHT_PAREN_TOKEN){
      numParens--;
    }else if (ttCur == LESS_TOKEN || ttCur == GREATER_TOKEN){
      if (tNext != WORD_TOKEN || tsNext == NULL) {
        fprintf(stderr, "Line %i in function validate() has an error: expected word following redirection op\n", tsCur->m_token.line_num);
        exit(1);
      }
    }else if (ttCur == NEWLINE_TOKEN && tsNext != NULL){
      if (semicolonCheck){ 
        tsCur->m_token.type = SEMICOLON_TOKEN;
      }else{
        if (tNext != IF_TOKEN && tNext != FI_TOKEN && tNext != THEN_TOKEN && tNext != ELSE_TOKEN && tNext != LEFT_PAREN_TOKEN && tNext != RIGHT_PAREN_TOKEN && tNext != WORD_TOKEN && tNext != DO_TOKEN && tNext != DONE_TOKEN && tNext != WHILE_TOKEN && tNext != UNTIL_TOKEN){
          fprintf(stderr, "Line %i in function validate() has an error: Newlines can only appear before certain tokens\n", tsCur->m_token.line_num);
          exit(1);
        }
      }
    }else if (ttCur == THEN_TOKEN || ttCur == ELSE_TOKEN){
      if (tsNext == NULL || tNext == FI_TOKEN) {
        fprintf(stderr, "Line %i in function validate() has an error: special words can't have tokens of type %i right after/next token is null\n", tsCur->m_token.line_num, tNext);
        exit(1);
      }
      if(ttCur == DO_TOKEN){
        if (tPrev == WORD_TOKEN){
          tsCur->m_token.type = WORD_TOKEN;
        }
        if (tsCur->m_token.type == tNext) {
          fprintf(stderr, "Line %i in function validate() has an error: can't have repeated token of type %i\n", tsCur->m_token.line_num, tNext);
          exit(1);
        }   
        if (tPrev != NEWLINE_TOKEN && tPrev != SEMICOLON_TOKEN) {
          fprintf(stderr, "Line %i in function validate() has an error: special words must have a ';' or '\\n' after\n", tsCur->m_token.line_num);
          exit(1);
        }         
        if (tsNext == NULL || tNext == SEMICOLON_TOKEN ) {
          fprintf(stderr, "Line %i in function validate() has an error: special words can't have tokens of type %i right after/next token is null\n", tsCur->m_token.line_num, tNext);
          exit(1);
        }
      }
    }else if (ttCur == DO_TOKEN){
      if (tPrev == WORD_TOKEN){
        tsCur->m_token.type = WORD_TOKEN;
      }
      if (tsCur->m_token.type == tNext) {
        fprintf(stderr, "Line %i in function validate() has an error: can't have repeated token of type %i\n", tsCur->m_token.line_num, tNext);
        exit(1);
      }   
      if (tPrev != NEWLINE_TOKEN && tPrev != SEMICOLON_TOKEN) {
        fprintf(stderr, "Line %i in function validate() has an error: special words must have a ';' or '\\n' after\n", tsCur->m_token.line_num);
        exit(1);
      }         
      if (tsNext == NULL || tNext == SEMICOLON_TOKEN ) {
        fprintf(stderr, "Line %i in function validate() has an error: special words can't have tokens of type %i right after/next token is null\n", tsCur->m_token.line_num, tNext);
        exit(1);
      }
    }else{
      switch (ttCur){
        case DONE_TOKEN:
        case FI_TOKEN:
          if (tNext == WORD_TOKEN){
            fprintf(stderr, "Line %i in function validate() has an error: can't have a word right after DONE, FI\n", tsCur->m_token.line_num);
            exit(1);
          }
        case IF_TOKEN:      
        case WHILE_TOKEN:
        case UNTIL_TOKEN:
          if (tNext == SEMICOLON_TOKEN || tsNext == NULL) {
            fprintf(stderr, "Line %i in function validate() has an error: special words can't have tokens of type %i right after/next token is null\n", tsCur->m_token.line_num, tNext);
            exit(1);
          }
          if (tPrev == WORD_TOKEN){
            tsCur->m_token.type = WORD_TOKEN;
          }
          if (tsCur->m_token.type == IF_TOKEN)
            numIf++;
          else if (tsCur->m_token.type == FI_TOKEN)
            numIf--;
          else if (tsCur->m_token.type == DONE_TOKEN)
            numDone--;
          else if (tsCur->m_token.type == UNTIL_TOKEN)
            numDone++;
          else if (tsCur->m_token.type == WHILE_TOKEN)
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

command_stream_t tokens_to_command_stream(token_stream_t tStream)
{
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
      tokenNext = tsNext->m_token.type;

    enum token_type ttCur = tsCur->m_token.type;

    if(ttCur == WORD_TOKEN){
      if (cmdTemp1 == NULL){
        countWords = 0;
        cmdTemp1 = new_command();
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
      *word = tsCur->m_token.t_word;
      countWords++;
      *(++word) = NULL; 

    }else if (ttCur == SEMICOLON_TOKEN){
      c_push(cmdTemp1, &top, &c_stackSize);

      while (stack_precedence(ts_peek()) > stream_precedence(tsCur->m_token.type)){
        cmdB = c_pop(&top);
        cmdA = c_pop(&top);
        cmdTemp1 = combine_commands(cmdA, cmdB, ts_pop());
        c_push(cmdTemp1, &top, &c_stackSize);
      }
      cmdTemp1 = NULL;
      word = NULL;
      if (tokenNext != THEN_TOKEN && tokenNext != ELSE_TOKEN && tokenNext != FI_TOKEN && tokenNext != DO_TOKEN &&
        tokenNext != DONE_TOKEN){
        ts_push(tsCur);
      }

    }else if (ttCur == PIPE_TOKEN){
      c_push(cmdTemp1, &top, &c_stackSize);
      while (stack_precedence(ts_peek()) > stream_precedence(tsCur->m_token.type)){
        cmdB = c_pop(&top);
        cmdA = c_pop(&top);
        cmdTemp1 = combine_commands(cmdA, cmdB, ts_pop());
        c_push(cmdTemp1, &top, &c_stackSize);
      }
      cmdTemp1 = cmdB = cmdA = NULL;
      word = NULL;
      ts_push(tsCur);

    }else if (ttCur == LEFT_PAREN_TOKEN){
      c_push(cmdTemp1, &top, &c_stackSize);
      cmdTemp1 = NULL;
      word = NULL;
      countParens++;
      ts_push(tsCur);

    }else if (ttCur == RIGHT_PAREN_TOKEN){
      if (countParens != 0) {
        c_push(cmdTemp1, &top, &c_stackSize);
        countParens--;
        while (ts_peek() != LEFT_PAREN_TOKEN){
          if (ts_peek() != UNKNOWN_TOKEN){
            cmdB = c_pop(&top);
            cmdA = c_pop(&top);
            cmdTemp1 = combine_commands(cmdA, cmdB, ts_pop());  
            c_push(cmdTemp1, &top, &c_stackSize);
            cmdTemp1 = NULL;         
          }else{
            fprintf(stderr, "Error in tokens_to_command_stream(): Unknown token returned in RIGHT_PAREN_TOKEN case\n");
            exit(1);
          }      
        }
        cmdTemp2 = new_command();
        cmdTemp2->type = SUBSHELL_COMMAND;
        cmdTemp2->u.command[0] = c_pop(&top); 
        c_push(cmdTemp2, &top, &c_stackSize);
        cmdTemp1 = cmdTemp2 = NULL;
        word = NULL;
        ts_pop(); 
      }else{
        fprintf(stderr, "Error in tokens_to_command_stream(): unmatched ')'\n");
        exit(1);
      }
      
    }else if (ttCur == LESS_TOKEN || ttCur == GREATER_TOKEN){
      c_push(cmdTemp1, &top, &c_stackSize);
      if (tsNext == NULL || tsNext->m_token.type != WORD_TOKEN){
        fprintf(stderr, "Error in tokens_to_command_stream(): wrong token type after redirection operator\n");
        exit(1);   
      }else{
        cmdTemp1 = c_pop(&top);
        if (cmdTemp1 != NULL) {
          if (tsCur->m_token.type == GREATER_TOKEN)
            cmdTemp1->output = tsNext->m_token.t_word;
          else if (tsCur->m_token.type == LESS_TOKEN)
            cmdTemp1->input = tsNext->m_token.t_word;
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

    }else if (ttCur == WHILE_TOKEN || ttCur == UNTIL_TOKEN || ttCur == IF_TOKEN){
      c_push(cmdTemp1, &top, &c_stackSize);
      cmdTemp1 = NULL;
      word = NULL;
      if (tsCur->m_token.type == UNTIL_TOKEN)
        countUntils++;
      else if (tsCur->m_token.type == WHILE_TOKEN)
        countWhiles++;
      else
        countIfs++;
      ts_push(tsCur);

    }else if (ttCur == THEN_TOKEN || ttCur == ELSE_TOKEN || ttCur == DO_TOKEN){
      c_push(cmdTemp1, &top, &c_stackSize);
      cmdTemp1 = NULL;
      word = NULL;
      ts_push(tsCur);

    }else if (ttCur == DONE_TOKEN){
      if (countUntils == 0 && countWhiles == 0) {
        fprintf(stderr, "Error in tokens_to_command_stream(): unmatched 'done' for while' or 'until' command\n");
        exit(1);
      }
      c_push(cmdTemp1, &top, &c_stackSize);

      enum token_type ttTemp1 = ts_peek();
      if (ttTemp1 == UNKNOWN_TOKEN){
        fprintf(stderr, "Error in tokens_to_command_stream(): null token returned in DONE case\n");
        exit(1);
      }else if (ttTemp1 == DO_TOKEN){
        cmdTemp1 = c_pop(&top);
        ts_pop();
      }

      ttTemp1 = ts_peek();
      if (ttTemp1 == UNKNOWN_TOKEN ){
        fprintf(stderr, "Error in tokens_to_command_stream(): null token returned in DONE case\n");
        exit(1);
      }else if (ttTemp1 == WHILE_TOKEN || ttTemp1 == UNTIL_TOKEN){
        cmdTemp2 = c_pop(&top);
      }
      
      cmdTemp3 = new_command();
      ttTemp1 = ts_peek();
      if (ttTemp1 != WHILE_TOKEN){
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
      ts_pop(); 

    }else if (ttCur == FI_TOKEN){
      if (countIfs == 0){
        fprintf(stderr, "Error in tokens_to_command_stream(): unmatched 'fi' for 'IF' command\n");
        exit(1);
      }
      c_push(cmdTemp1, &top, &c_stackSize);

      enum token_type ttTemp2 = ts_peek();
      if (ttTemp2 == UNKNOWN_TOKEN ){
        fprintf(stderr, "Error in tokens_to_command_stream(): unknown token returned in FI case\n");
        exit(1);
      }else if (ttTemp2 == ELSE_TOKEN){
        cmdC = c_pop(&top);
        ts_pop();
      }

      ttTemp2 = ts_peek();
      if (ttTemp2 == UNKNOWN_TOKEN){
        fprintf(stderr, "Error in tokens_to_command_stream(): unknown token returned in FI case\n");
        exit(1);
      }else if (ttTemp2 == THEN_TOKEN){
        cmdB = c_pop(&top);
        ts_pop();
      }
      
      ttTemp2 = ts_peek();
      if (ttTemp2 == UNKNOWN_TOKEN){
        fprintf(stderr, "Error in tokens_to_command_stream(): unknown token returned in FI case\n");
        exit(1);
      }else if (ttTemp2 == IF_TOKEN){
        cmdA = c_pop(&top);
      }      

      cmdTemp1 = new_command();
      cmdTemp1->type = IF_COMMAND;
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
      ts_pop(); 

    }else if (ttCur == NEWLINE_TOKEN){
      c_push(cmdTemp1, &top, &c_stackSize);
      while (stack_precedence(ts_peek()) > stream_precedence(tsCur->m_token.type)){
        cmdB = c_pop(&top);
        cmdA = c_pop(&top);
        cmdTemp1 = combine_commands(cmdA, cmdB, ts_pop());
        c_push(cmdTemp1, &top, &c_stackSize);
      }
      if (countParens == 0 && countIfs == 0 && countWhiles == 0 && countUntils == 0){
        csTemp1 = (command_stream_t) checked_malloc(sizeof(struct command_stream));
        csTemp1->m_command = c_pop(&top);
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
  
  while (ts_peek() != UNKNOWN_TOKEN){
    cmdB = c_pop(&top);
    cmdA = c_pop(&top);
    cmdTemp1 = combine_commands(cmdA, cmdB, ts_pop());
    c_push(cmdTemp1, &top, &c_stackSize);
  }

  csTemp1 = (command_stream_t) checked_malloc(sizeof(struct command_stream));
  csTemp1->m_command = c_pop(&top);

  csTemp2 = append_to_cstream(csTemp2, csTemp1);
  cmdTemp1 = NULL;

  if (c_pop(&top) != NULL){
    fprintf(stderr, "%s\n","Error in tokens_to_command_stream(): c_stack should be empty");
    exit(1);
  }

  return csTemp2;

} 

//-----------------------------------------------------------------------------
//
// "Stack" Function Implementations: 
//
// ts_stack:
//
// Used a list to implement a token "stack". Instead of creating a stack
// data struct, used a list to mimic a stack's FILO behavior. Push in a new 
// item by appending the node to the end of the list. Pop out
// items from the "stack" removing the tail node.
//
//    |_______|
//    |_______| <= next = NULL
//    |__...__| <= top item (tail node)
//    |__...__| <= prev
//    |__...__|
//    
// c_stack:
//
// An array of **pointers were used to create a command "stack". Again, a list
// was used to mimic a stack's FILO behavior. Due to the fact that the c_stack
// had indices, the top for an empty stack had to start at -1, a reference to
// the top location index, and the c_stack size had to be fed in as arguments 
// for its push and pop functions. That, and the fact that the stack is 
 //dynamically allocated.
//             __
//            |__|-----> X
//            |__|-----> X
//            |__|-----> X
//            |__|-----> cmd_B
//   __       |__|-----> cmd_A
//  |__|----->|__|-----> cmd_temp
// 
// Resources:
// - http://stackoverflow.com/questions/16371101/double-pointers-in-stack-
//   implementation-of-linked-list
// - http://www.c4learn.com/c-programs/c-program-to-perform-stack-operations-
//   using-pointer.html
// - http://stackoverflow.com/questions/7638612/c-double-pointer-to-structure
// - CS 31/32 textbook, and other various online sources
//-----------------------------------------------------------------------------

void ts_push(token_stream_t item)
{
  if (item == NULL)
    return;

  if (ts_stack == NULL)     // If ts_stack is empty, just set it to item
  {
    ts_stack = item;
    ts_stack->prev = ts_stack->next = NULL;
  }
  else
  {
    item->next = NULL;      // Goes to the top
    item->prev = ts_stack;

    ts_stack->next = item;
    ts_stack = item;        // ts_stack points to the top
  }
  // printf("PUSHED IN a TOKEN of type %d into ts_stack\n", 
  // item->m_token.type);
}


enum token_type 
ts_peek()
{
  if (ts_stack == NULL)     // If ts_stack is empty
    return UNKNOWN_TOKEN;
  else
    return ts_stack->m_token.type;
}


token_stream_t 
ts_pop()
{
  if (ts_stack == NULL)
    return NULL;

  token_stream_t top = ts_stack;

  ts_stack = ts_stack->prev;

  if (ts_stack != NULL)
    ts_stack->next = NULL;

  top->prev = top->next = NULL;

  // printf("POPPED OUT a TOKEN of type %d out of ts_stack\n", 
  // top->m_token.type);

  return top;
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

// If there is a token with higher precedence on the TOKEN stack in comparison // to the token currently being processed on the stream, pop it out and use it 
// to create a new command by combining two commands in the COMMAND stack. 
//
// Example: Tokens '|', ';' on the token stack require popping two commands 
// out from the command stack to form a new command because these tokens will 
// have higher precedence.
// 
// Also, stack_precedence values > Stream_precedence values due to left-
// association... These value assignments determine the order in which the 
// commands are stored in the command stream. They are the results of much 
// trial-and-error/drawing-of-streams/frustration/hair-pulling.
//
int stack_precedence(enum token_type type)
{
  switch (type)
  {
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


int stream_precedence(enum token_type type)
{
  switch (type)
  {
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

//-----------------------------------------------------------------------------

int check_if_word(char c) {
    if (isalnum(c) || (c == '!') || (c == '%') || (c == '!') || (c == '+') || (c == '-') || (c == ',') || (c == '.') || (c == '/') || (c == ':') || (c == '@') || (c == '^') || (c == '_'))
        return 1;

    return 0;
}


command_t
new_command()
{
  // Creates a simple command by default
  command_t item = (command_t) checked_malloc(sizeof(struct command));
  
  item->type = SIMPLE_COMMAND;
  item->status = -1;
  item->input = item->output = NULL;
  item->u.word = NULL;

  return item;  
}


command_t
combine_commands(command_t cmd_A, command_t cmd_B, token_stream_t tstream)
{
  if (tstream == NULL)
  {
    fprintf(stderr, "Error in combine_commands(): attempting to pop from empty ts_stack\n");
    exit(1);
  }

  command_t combined = NULL;

  // Make sure that there exists a LHS and a RHS before combining commands
  if (cmd_A == NULL || cmd_B == NULL)
  {
    char token_type[20];
    switch (tstream->m_token.type)
    {
      case 0: strcpy(token_type, "WORD_TOKEN");break;
      case 1: strcpy(token_type, "SEMICOLON_TOKEN");break;
      case 2: strcpy(token_type, "PIPE_TOKEN");break;
      case 3: strcpy(token_type, "LEFT_PAREN_TOKEN");break;
      case 4: strcpy(token_type, "RIGHT_PAREN_TOKEN");break;
      case 5: strcpy(token_type, "LESS_TOKEN");break;
      case 6: strcpy(token_type, "GREATER_TOKEN");break;
      case 7: strcpy(token_type, "IF_TOKEN");break;
      case 8: strcpy(token_type, "THEN_TOKEN");break;
      case 9: strcpy(token_type, "ELSE_TOKEN");break;
      case 10: strcpy(token_type, "FI_TOKEN");break;
      case 11: strcpy(token_type, "WHILE_TOKEN");break;
      case 12: strcpy(token_type, "DO_TOKEN");break;
      case 13: strcpy(token_type, "DONE_TOKEN");break;
      case 14: strcpy(token_type, "UNTIL_TOKEN");break;
      case 15: strcpy(token_type, "NEWLINE_TOKEN");break;
      case 16: strcpy(token_type, "UNKNOWN_TOKEN");break;
      case 17: strcpy(token_type, "UNKNOWN_TOKEN");break;
      default: strcpy(token_type, "ERROR in combine()");break;
    }
    fprintf(stderr, 
      "Error in combine_commands(): Either cmd_A or cmd_B wrong\n");
    if (cmd_A == NULL)
      fprintf(stderr, "Poop. cmd_A is NULL\n");
    else
      fprintf(stderr, "Crap. cmd_B is NULL\n");
    exit(1);
  }
  // Finallu combine the commands
  combined = new_command();
  combined->u.command[0] = cmd_A;
  combined->u.command[1] = cmd_B;

    switch (tstream->m_token.type)
    {
      // By default, new_command() creates a simple command so we have to 
      // manually change the new command's type
      case SEMICOLON_TOKEN: combined->type = SEQUENCE_COMMAND; break;
      case PIPE_TOKEN: combined->type = PIPE_COMMAND; break;
      case RIGHT_PAREN_TOKEN: combined->type = SUBSHELL_COMMAND; break;
      default: 
        fprintf(stderr, "Error in combine_commands(): unidentified type token of type %i in 2nd switch statement\n", tstream->m_token.type); 
        exit(1);
    }

  return combined;
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
