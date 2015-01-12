// UCLA CS 111 Lab 1 command internals

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

enum command_type
  {
    IF_COMMAND,		 // if A then B else C fi
    PIPE_COMMAND,        // A | B
    SEQUENCE_COMMAND,    // A ; B
    SIMPLE_COMMAND,      // a simple command
    SUBSHELL_COMMAND,    // ( A )
    UNTIL_COMMAND,	 // until A do B done
    WHILE_COMMAND,	 // while A do B done
  };

enum token_type
  {
    IF,
    FI,
    ELSE,
    THEN,
    WHILE,
    DO,
    DONE,
    UNTIL,
    SEMICOLON,
    PIPE,
    OPEN_PAREN,
    CLOSE_PAREN,
    LESS_THAN,
    GREATER_THAN,
    WORD,
    NEWLINE,
    OTHER
  };

// Data associated with a command.
struct command
{
  enum command_type type;

  // Exit status, or -1 if not known (e.g., because it has not exited yet).
  int status;

  // I/O redirections, or null if none.
  char *input;
  char *output;

  union
  {
    // For SIMPLE_COMMAND.
    char **word;

    // For all other kinds of commands.  Trailing entries are unused.
    // Only IF_COMMAND uses all three entries.
    struct command *command[3];
  } u;
};

struct command_stream
{
  //change these two later
  int number;
  int iterator;

  command_t node;
  command_node_t next;
  command_node_t prev;
};

struct token 
{
  char* data;
  enum type_of_token type;
  int line_no;
  int token_len;
};

struct token_stream 
{
  token_t node;
  token_stream_t next;
  token_stream_t prev;
};
