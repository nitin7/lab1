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


void executeIf(command_t cmd, int profilingSwitch);
void executePipe(command_t cmd, int profilingSwitch);
void executeSequence(command_t cmd, int profilingSwitch);
void executeSimple(command_t cmd, char* input, char* output, int profilingSwitch);
void executeSubshell(command_t cmd, char* input, char* output, int profilingSwitch);
void executeUntil(command_t cmd, int);
void executeWhile(command_t cmd, int);


int prepare_profiling(char const *name){
  /* FIXME: Replace this with your implementation.  You may need to
     add auxiliary functions and otherwise modify the source code.
     You can also use external functions defined in the GNU C Library.  */
  error (0, 0, "warning: profiling not yet implemented");
  return -1;
}

int command_status(command_t c){
    int status = c->status;
    return status;
}

void selectCommand(command_t c, int i, int profilingSwitch) {

    struct command *cmd = c->u.command[i];
    char* input = cmd->input;
    char* output = cmd->output;
    enum command_type c_type = cmd->type;
    if (c_type == IF_COMMAND) {
        executeIf(cmd,profilingSwitch);
    } else if (c_type == PIPE_COMMAND) {
        executePipe(cmd,profilingSwitch);
    } else if (c_type == SEQUENCE_COMMAND) {
        executeSequence(cmd,profilingSwitch);
    } else if (c_type == SIMPLE_COMMAND) {
        executeSimple(cmd, input, output, profilingSwitch);
    } else if (c_type == SUBSHELL_COMMAND) {
        executeSubshell(cmd, input, output, profilingSwitch);
    } else if (c_type == WHILE_COMMAND) {
        executeWhile(cmd,profilingSwitch);
    } else if (c_type == UNTIL_COMMAND) {
        executeUntil(cmd,profilingSwitch);
    } else {
        error(1,0,"Command not found!\n");
    }
    return;
}

void executeIf(command_t c, int profilingSwitch){
    selectCommand(c, 0, profilingSwitch);
    if (c->u.command[0]->status == EXIT_SUCCESS)
        selectCommand(c, 1, profilingSwitch);
    else if (c->u.command[2])
        selectCommand(c, 2, profilingSwitch);
    return;
}

void executePipe(command_t c, int profiling){
    int status;
    int file_d[2];
    pid_t pid1 = fork();
    int t0 = file_d[0];
    int t1 = file_d[1];
    if (pipe(file_d) < 0){
        error(1, 0, "executePipe(): Failed pipe.\n");
    }else if (pid1 < 0){
        error(1, 0, "executePipe(): Failed fork.\n");
    }else if (pid1 == 0){
        pid_t pid2 = fork();
        if (pid2 < 0){ 
            error(1, 0, "executePipe(): Failed fork.\n");
        } else if (pid2 == 0) {
            close(t0);
            if (dup2(t1, 1) < 0) 
                error(1,0,"executePipe(): Failed dup2.\n");
            selectCommand(c, 0, profiling);
            _exit(c->u.command[0]->status);
        } else {
            waitpid(pid2, &status, 0);
            close(t1);
            if (dup2(t0, 0) < 0) 
                error(1,0,"executePipe(): Failed dup2.\n");
            selectCommand(c, 1, profiling);
            _exit(c->u.command[1]->status);
        }
    }else{
        close(t0);
        close(t1);
        waitpid(pid1, &status, 0);
    }

    return;
}



void executeSequence(command_t c, int profilingSwitch){
    selectCommand(c, 0, profilingSwitch);
    selectCommand(c, 1, profilingSwitch);
    return;
}

void executeSimple(command_t c, char* input, char* output, int profilingSwitch){
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
        if (input != NULL) {
            int input_file = open(input, O_RDONLY, 0666);

            if (input_file < 0) 
                error(1, 0, "Failed to open input file. \n");
            if (dup2(input_file, 0) < 0) 
                error(1, 0, "Failed dup2. \n");

            close(input_file);
        }
        if (output != NULL) {
            int output_file = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0666);

            if (output_file < 0) 
                error(1,0,"Failed to open output file. \n");
            if (dup2(output_file, 1) < 0) 
                error(1, 0, "Failed dup2 \n");

            close(output_file);
        }

        if (strcmp(c->u.word[0], "exec") == 0) 
            execvp(c->u.word[1], c->u.word + 1);
        else 
            execvp(c->u.word[0], c->u.word);
    }
    return;
}

void executeSubshell(command_t c, char* input, char* output, int profilingSwitch){

    if (input != NULL) {
        c->u.command[0]->input = input;
    }
    if (output != NULL) {
        c->u.command[0]->output = output;
    }

    selectCommand(c, 0, profilingSwitch);
    return;
}

void executeUntil(command_t c, int profilingSwitch){
    selectCommand(c, 0, profilingSwitch);
    while (c->u.command[0]->status != EXIT_SUCCESS)
    {
        selectCommand(c, 1, profilingSwitch);
        selectCommand(c, 0, profilingSwitch);
    }
    return;
}

void executeWhile(command_t c, int profilingSwitch){

    selectCommand(c, 0, profilingSwitch);
    while (c->u.command[0]->status == EXIT_SUCCESS)
    {
        selectCommand(c, 1, profilingSwitch);
        selectCommand(c, 0, profilingSwitch);
    }
    return;
}


void execute_command(command_t c, int profilingSwitch){
    enum command_type c_type = c->type;
    char* input = c->input;
    char* output = c->output;

    if (c_type == IF_COMMAND) {
        executeIf(c, profilingSwitch);
    } else if (c_type == PIPE_COMMAND) {
        executePipe(c, profilingSwitch);
    } else if (c_type == SEQUENCE_COMMAND) {
        executeSequence(c, profilingSwitch);
    } else if (c_type == SIMPLE_COMMAND) {
        executeSimple(c, input, output, profilingSwitch);
    } else if (c_type == SUBSHELL_COMMAND) {
        executeSubshell(c, input, output, profilingSwitch);
    } else if (c_type == WHILE_COMMAND) {
        executeWhile(c, profilingSwitch);
    } else if (c_type == UNTIL_COMMAND) {
        executeUntil(c, profilingSwitch);
    } else {
        error(1,0,"Command not found!\n");
    }
    return;
}