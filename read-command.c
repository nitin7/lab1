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

int check_if_word(char c)
{
  if (isalnum(c) || (c == '!') || (c == '%') || (c == '!') || (c == '+') || (c == '-') || (c == ',') || (c == '.') || (c == '/') || (c == ':') || (c == '@') || (c == '^') ||  (c == '_')) 
    return 1;

  return 0;
}

token_stream_t
tokenize_bytes(char* buf) 
{
  enum token_type type_of_token;
  struct token_stream *head = NULL;
  struct token_stream *cur = head;

  //case where buffer empty
  int i = 0;
  int line = 1;
  char ch;
  while ((ch = buf[i]) != '\0') 
  {
    switch (ch)
    {
      //comments check
      case '\n':
        line++;
        type = NEWLINE_TOKEN;
        nextCh = buffer[i+i];
        if (nextCh == '\n' && nextCh != '\0')
        {
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

    if (check_if_word(ch) == 1)
    {
      while (is_word_char(buffer[nChars + 1 + i]) == 1)
        nChars++;
      type = WORD_TOKEN;
      i+=nChars;
    }

    struct token_stream* ts = (struct token_stream *) checked_malloc(sizeof(struct token_stream));
    ts->m_token.type = type;
    ts->m_token.length = nChars;
    ts->m_token.line_num = line;
    //default initialize to null in command-internals.h
    ts->prev = NULL;
    ts->next = NULL;

    if (type == NEWLINE_TOKEN)
      ts->m_token.line_num--;
    else if (type == UNKNOWN_TOKEN)
    {
      ts->m_token.t_word = NULL;
      fprintf(stderr, "Error: found unknown token %c on line %i\n", ch, line);
      exit(1);
    }
    else if (type = WORD_TOKEN)
    {
      ts->m_token.t_word = (char *) checked_malloc(sizeof(char) * nChars + 1);
      
      for (int k = 0; k < nChars; k++)
        ts->m_token.t_word[k] = buffer[startChar + k];

      ts->m_token.t_word[nChars] = '\0';

      if (strcmp(ts->m_token.t_word, "if") == 0) {
        ts->m_token.type = IF_TOKEN;
      }
      else if (strcmp(ts->m_token.t_word, "then") == 0) {
        ts->m_token.type = THEN_TOKEN;
      }
      else if (strcmp(ts->m_token.t_word, "else") == 0) {
        ts->m_token.type = ELSE_TOKEN;
      }
      else if (strcmp(ts->m_token.t_word, "fi") == 0) {
        ts->m_token.type = FI_TOKEN;
      }
      else if (strcmp(ts->m_token.t_word, "while") == 0) {
        ts->m_token.type = WHILE_TOKEN;
      }
      else if (strcmp(ts->m_token.t_word, "do") == 0) {
        ts->m_token.type = DO_TOKEN;
      }
      else if (strcmp(ts->m_token.t_word, "done") == 0) {
        ts->m_token.type = DONE_TOKEN;
      }
      else if (strcmp(ts->m_token.t_word, "until") == 0) {
        ts->m_token.type = UNTIL_TOKEN;
      }
    }
    else
      ts->m_token.t_word = NULL;

    if (cur != NULL)
    {
      ts->prev = cur;
      ts->next = NULL;
      cur->next = ts;
      cur = ts;
    }
    else
    {
      head = ts;
      cur = head;
    }
    i++;
  }
  return head;
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
