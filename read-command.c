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

enum type_of_token
{

}

typedef struct token *token_t;
typedef struct token_stream *token_stream_t;

struct token 
{
  char* data;
  enum type_of_token type;
  token_t next;
};

struct token_stream 
{
  token_t head;
  token_stream_t next;
};

char*
read_bytes (int (*get_next_byte) (void *),
         void *get_next_byte_argument)
{
  size_t n, buf_size;
  char next_byte;

  buf_size = 1024;
  n = 0;

  char *buf = (char *) checked_malloc(sizeof(char) * buf_size);

  char next_byte; ;

  while ((next_byte = get_next_byte(get_next_byte_argument)) != EOF) 
  {
    //case where next_byte is a comment - ignore the rest of the line
    if (next_byte == '#') 
    {
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

  if (n < buf_size - 1)
  {
    buf[n] = '\0';
  }
  else 
  {
    buf = (char *) checked_grow_alloc(buf, &buf_size);
    buf[n] = '\0';
  }
  
  return buf;
}

token_stream_t
tokenize_bytes(char* buf) 
{



}

command_stream_t
make_command_stream (int (*get_next_byte) (void *),
		     void *get_next_byte_argument)
{

  char* buf = read_bytes(get_next_byte, get_next_byte_argument);

  token_stream_t ts = tokenize_bytes(buf);


  error (1, 0, "command reading not yet implemented");
  return 0;
}

command_t
read_command_stream (command_stream_t s)
{
  /* FIXME: Replace this with your implementation too.  */
  error (1, 0, "command reading not yet implemented");
  return 0;
}
