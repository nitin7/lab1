// UCLA CS 111 Lab 1 command execution

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

#include <error.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>


void executeIf(command_t,int);
void executePipe(command_t,int);
void executeSequence(command_t,int);
void executeSimpleCmd(command_t,int);
void executeSubshellCmd(command_t,int);
void executeUntil(command_t,int);
void executeWhile(command_t,int);

void
printcmd (int profiling, command_t cmd){
    switch (cmd->type) {
        case IF_COMMAND:
            write(profiling, "if ", 3);
            printcmd(profiling, cmd->u.command[0]);
            write(profiling, "then ", 5);
            printcmd(profiling, cmd->u.command[1]);
            if (cmd->u.command[2]){
                write(profiling, "else ", 5);
                printcmd(profiling, cmd->u.command[2]);
                write(profiling, "fi ", 3);
            }
            break;
        case PIPE_COMMAND:
            printcmd(profiling, cmd->u.command[0]);
            write(profiling, "| ", 2);
            printcmd(profiling, cmd->u.command[1]);
            break;
        case SEQUENCE_COMMAND:
            printcmd(profiling, cmd->u.command[0]);
            write(profiling, "; ", 2);
            printcmd(profiling, cmd->u.command[1]);
            break;
        case SIMPLE_COMMAND:{
            char **w = cmd->u.word;
            write(profiling, *w, strlen(*w));
            write(profiling, " ", 1);
            while (*(++w)){
                write(profiling, *w, strlen(*w));
                write(profiling, " ", 1);
            }
            break;
        }
        case SUBSHELL_COMMAND:
            write(profiling, "( ", 2);
            printcmd(profiling, cmd->u.command[0]);
            write(profiling, ") ", 2);
            break;
        case UNTIL_COMMAND:
            write(profiling, "until ", 6);
            printcmd(profiling, cmd->u.command[0]);
            write(profiling, "do ", 3);
            printcmd(profiling, cmd->u.command[1]);
            write(profiling, "done ", 5);
            break;
        case WHILE_COMMAND:
            write(profiling, "while ", 6);
            printcmd(profiling, cmd->u.command[0]);
            write(profiling, "do ", 3);
            printcmd(profiling, cmd->u.command[1]);
            write(profiling, "done ", 5);
            break;
        default:
            break;
    }
    if (cmd->input){
        write(profiling, "<", 1);
        write(profiling, cmd->input, strlen(cmd->input));
        write(profiling, " ", 1);
    }
    if (cmd->output){
        write(profiling, ">", 1);
        write(profiling, cmd->output, strlen(cmd->output));
        write(profiling, " ", 1);
    }
}


int
prepare_profiling (char const *name)
{
  /* FIXME: Replace this with your implementation.  You may need to
     add auxiliary functions and otherwise modify the source code.
     You can also use external functions defined in the GNU C Library.  */
  error (0, 0, "warning: profiling not yet implemented");
  return -1;
}

int
command_status(command_t c)
{
    int status = c->status;
    return status;
}

void selectCommand(command_t c, int i, int profiling){

    struct command *cmd = c->u.command[i];
    enum command_type c_type = cmd->type;
    if(c_type == IF_COMMAND){
        executeIf(cmd,profiling);
    }else if(c_type == PIPE_COMMAND){
        executePipe(cmd,profiling);
    }else if(c_type == SEQUENCE_COMMAND){
        executeSequence(cmd,profiling);
    }else if(c_type == SIMPLE_COMMAND){
        executeSimpleCmd(cmd,profiling);
    }else if(c_type == SUBSHELL_COMMAND){
        executeSubshellCmd(cmd,profiling);
    }else if(c_type == WHILE_COMMAND){
        executeWhile(cmd,profiling);
    }else if(c_type == UNTIL_COMMAND){
        executeUntil(cmd,profiling);
    }else{
        error(1,0,"Command not found!\n");
    }
}

void
executeIf(command_t c, int profiling)
{

    selectCommand(c, 0, profiling);
    if (c->u.command[0]->status == EXIT_SUCCESS)
        selectCommand(c, 1, profiling);
    else if (c->u.command[2])
        selectCommand(c, 2, profiling);
}

void
executePipe(command_t c, int profiling2)
{
    int status;
    int profiling[2];
    if (pipe(profiling) < 0)
        error(1, 0, "Pipe failed!\n");
    pid_t pid = fork();
    if (pid < 0)
        error(1, 0, "Fork failed!\n");
    if (pid == 0){
        pid_t pid2 = fork();
        if (pid2 < 0) error(1, 0, "Fork failed!\n");
        if (pid2==0){
            close(profiling[0]);
            if(dup2(profiling[1], 1)<0) error(1,0,"dup2 failed!\n");
            selectCommand(c, 0, profiling2);
            _exit(c->u.command[0]->status);
        }else{
            waitpid(pid2, &status, 0);
            close(profiling[1]);
            if(dup2(profiling[0], 0)<0) error(1,0,"dup2 failed!\n");
            selectCommand(c, 1, profiling2);
            _exit(c->u.command[1]->status);
        }
    }
    else{
        close(profiling[0]);
        close(profiling[1]);
        waitpid(pid, &status, 0);

    }
}



void
executeSequence(command_t c, int profiling)
{
    selectCommand(c, 0, profiling);
    selectCommand(c, 1, profiling);
}
//changes done 
void
executeSimpleCmd(command_t c, int profiling)
{
    pid_t pid = fork();
    int status;

    if (pid > 0)
    {
        waitpid(pid, &status, 0);
        c->status = WEXITSTATUS(status);
    }
    else if (pid < 0)
    {
        error(1, 0, "Failed to fork. \n");
    }
    else 
    {
        char* input = c->input;
        char* output = c->output;

        if (input != NULL) {
            int input_file = open(input, O_RDONLY, 0666);

            if (input_file < 0) 
                error(1, 0, "Failed to open input file. \n");
            if (dup2(input_file, 0) < 0) 
                error(1, 0, "Failed dup2. \n");

            close(input_file);
        }
        if (output != NULL) {
            int output_file = open(output, O_WRONLY|O_CREAT|O_TRUNC, 0666);

            if (output_file < 0) 
                error(1,0,"Failed to open output file. \n");
            if (dup2(output_file, 1) < 0) 
                error(1, 0, "Failed dup2 \n");

            close(output_file);
        }

        if (strcmp(c->u.word[0], "exec") != 0) 
            execvp(c->u.word[0], c->u.word);
        else 
            execvp(c->u.word[1], c->u.word + 1);
    }
}

void
executeSubshellCmd(command_t c, int profiling)
{
    if(c->input){
        c->u.command[0]->input=c->input;
    }
    if(c->output){
        c->u.command[0]->output=c->output;
    }
    selectCommand(c, 0, profiling);
}

void
executeUntil(command_t c, int profiling)
{
    selectCommand(c, 0, profiling);
    while (c->u.command[0]->status != EXIT_SUCCESS)
    {
        selectCommand(c, 1, profiling);
        selectCommand(c, 0, profiling);
    }
}

void
executeWhile(command_t c, int profiling)
{

    selectCommand(c, 0, profiling);
    while (c->u.command[0]->status == EXIT_SUCCESS)
    {
        selectCommand(c, 1, profiling);
        selectCommand(c, 0, profiling);
    }

}


void execute_command(command_t c, int profiling)
{

    enum command_type c_type = c->type;   

    if (c_type == IF_COMMAND){
        executeIf(c, profiling);
    }else if (c_type == PIPE_COMMAND){
        executePipe(c, profiling);
    }else if (c_type == SEQUENCE_COMMAND){
        executeSequence(c, profiling);
    }else if (c_type == SIMPLE_COMMAND){
        executeSimpleCmd(c, profiling);
    }else if (c_type == SUBSHELL_COMMAND){
        executeSubshellCmd(c, profiling);
    }else if (c_type == WHILE_COMMAND){
        executeWhile(c, profiling);
    }else if (c_type == UNTIL_COMMAND){
        executeUntil(c, profiling);
    }else{
        error(1,0,"Command not found!\n");
    }
}