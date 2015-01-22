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


void execute_if_command(command_t,int);
void execute_pipe_command(command_t,int);
void execute_sequence_command(command_t,int);
void execute_simple_command(command_t,int);
void execute_subshell_command(command_t,int);
void execute_until_command(command_t,int);
void execute_while_command(command_t,int);

void
printcmd (int fd, command_t cmd){
    switch (cmd->type) {
        case IF_COMMAND:
            write(fd, "if ", 3);
            printcmd(fd, cmd->u.command[0]);
            write(fd, "then ", 5);
            printcmd(fd, cmd->u.command[1]);
            if (cmd->u.command[2]){
                write(fd, "else ", 5);
                printcmd(fd, cmd->u.command[2]);
                write(fd, "fi ", 3);
            }
            break;
        case PIPE_COMMAND:
            printcmd(fd, cmd->u.command[0]);
            write(fd, "| ", 2);
            printcmd(fd, cmd->u.command[1]);
            break;
        case SEQUENCE_COMMAND:
            printcmd(fd, cmd->u.command[0]);
            write(fd, "; ", 2);
            printcmd(fd, cmd->u.command[1]);
            break;
        case SIMPLE_COMMAND:{
            char **w = cmd->u.word;
            write(fd, *w, strlen(*w));
            write(fd, " ", 1);
            while (*(++w)){
                write(fd, *w, strlen(*w));
                write(fd, " ", 1);
            }
            break;
        }
        case SUBSHELL_COMMAND:
            write(fd, "( ", 2);
            printcmd(fd, cmd->u.command[0]);
            write(fd, ") ", 2);
            break;
        case UNTIL_COMMAND:
            write(fd, "until ", 6);
            printcmd(fd, cmd->u.command[0]);
            write(fd, "do ", 3);
            printcmd(fd, cmd->u.command[1]);
            write(fd, "done ", 5);
            break;
        case WHILE_COMMAND:
            write(fd, "while ", 6);
            printcmd(fd, cmd->u.command[0]);
            write(fd, "do ", 3);
            printcmd(fd, cmd->u.command[1]);
            write(fd, "done ", 5);
            break;
        default:
            break;
    }
    if (cmd->input){
        write(fd, "<", 1);
        write(fd, cmd->input, strlen(cmd->input));
        write(fd, " ", 1);
    }
    if (cmd->output){
        write(fd, ">", 1);
        write(fd, cmd->output, strlen(cmd->output));
        write(fd, " ", 1);
    }
}

double
calctime(double tv_sec, double tv_nsec){
    tv_nsec/=1000000000;
    return tv_sec+tv_nsec;
}

double
calctime2(double tv_sec, double tv_usec){
    tv_usec/=1000000;
    return tv_sec+tv_usec;
}
int
prepare_profiling(char const *name)
{
    /* FIXME: Replace this with your implementation.  You may need to
     add auxiliary functions and otherwise modify the source code.
     You can also use external functions defined in the GNU C Library.  */
    //error (0, 0, "warning: profiling not yet implemented");
    int fd;
    fd = open(name, O_WRONLY|O_CREAT|O_TRUNC, 0666);
    if(fd < 0) {
        fprintf(stderr, "Cannot write to file: %s", name);
        exit(1);
    }
    return fd;
}

int
command_status(command_t c)
{
    return c->status;
}

void temp(command_t c, int i, int fd){

    struct command *cmd = c->u.command[i];
    enum command_type c_type = cmd->type;
    if(c_type == IF_COMMAND){
        execute_if_command(cmd,fd);
    }else if(c_type == PIPE_COMMAND){
        execute_pipe_command(cmd,fd);
    }else if(c_type == SEQUENCE_COMMAND){
        execute_sequence_command(cmd,fd);
    }else if(c_type == SIMPLE_COMMAND){
        execute_simple_command(cmd,fd);
    }else if(c_type == SUBSHELL_COMMAND){
        execute_subshell_command(cmd,fd);
    }else if(c_type == WHILE_COMMAND){
        execute_while_command(cmd,fd);
    }else if(c_type == UNTIL_COMMAND){
        execute_until_command(cmd,fd);
    }
}

void
execute_if_command(command_t c,int fd){


    temp(c, 0, fd);
    

    //if condition executes successfully, execute the command[1]
    if (c->u.command[0]->status == EXIT_SUCCESS)
    {
        temp(c, 1, fd);
    }
    //if condition executes unsuccessfully, execute the command[2] if it exits, else return 0 (successfully executes if command)
    else if (c->u.command[2])
    {
        temp(c, 2, fd);
    }
}

void
execute_pipe_command(command_t c, int fd2)
{
    int status;
    int fd[2];
    if (pipe(fd) < 0)
        error(1, 0, "Pipe failed!\n");
    pid_t pid = fork();
    if (pid < 0)
        error(1, 0, "Fork failed!\n");
    if (pid == 0){
        pid_t pid2 = fork();
        if (pid2 < 0) error(1, 0, "Fork failed!\n");
        if (pid2==0){
            close(fd[0]);
            if(dup2(fd[1], 1)<0) error(1,0,"dup2 failed!\n");
            temp(c, 0, fd2);
            _exit(c->u.command[0]->status);
        }else{
            waitpid(pid2, &status, 0);
            close(fd[1]);
            if(dup2(fd[0], 0)<0) error(1,0,"dup2 failed!\n");
            temp(c, 1, fd2);
            _exit(c->u.command[1]->status);
        }
    }
    else{
        close(fd[0]);
        close(fd[1]);
        waitpid(pid, &status, 0);

    }
}



void
execute_sequence_command(command_t c,int fd)
{
    temp(c, 0, fd);
    temp(c, 1, fd);
 
}

void
execute_simple_command(command_t c,int fd)
{
    pid_t p = fork();
    int status;
    if (p == 0) {
        if (c->input) {
            int infile;
            if ((infile = open(c->input, O_RDONLY, 0666)) == -1) error(1,0,"Cannnot open file!\n");
            if (dup2(infile, 0) < 0) error(1,0,"I/O redirecting failed!\n");
            close(infile);
        }
        if (c->output) {
            int outfile;
            if ((outfile = open(c->output, O_WRONLY|O_CREAT|O_TRUNC,0666)) == -1) error(1,0,"Cannnot open file!\n");
            if (dup2(outfile, 1) < 0) error(1,0,"I/O redirecting failed!\n");
            close(outfile);
        }
        if (strcmp(c->u.word[0], "exec")!= 0) execvp(c->u.word[0], c->u.word);
        else execvp(c->u.word[1], c->u.word + 1);
    }
    else{
        waitpid(p, &status, 0);
        c->status = WEXITSTATUS(status);
    }
}

void
execute_subshell_command(command_t c, int fd)
{
    if(c->input){
        c->u.command[0]->input=c->input;
    }
    if(c->output){
        c->u.command[0]->output=c->output;
    }
    temp(c, 0, fd);
}

void
execute_until_command(command_t c, int fd)
{
    temp(c, 0, fd);
    while (c->u.command[0]->status != EXIT_SUCCESS)
    {
        temp(c, 1, fd);
        temp(c, 0, fd);
    }
}

void
execute_while_command(command_t c, int fd)
{

    temp(c, 0, fd);
    while (c->u.command[0]->status == EXIT_SUCCESS)
    {
        temp(c, 1, fd);
        temp(c, 0, fd);
    }

}


void execute_command(command_t c, int profiling)
{

    enum command_type c_type = c->type;   

    if (c_type == IF_COMMAND){
        execute_if_command(c, profiling);
    }else if (c_type == PIPE_COMMAND){
        execute_pipe_command(c, profiling);
    }else if (c_type == SEQUENCE_COMMAND){
        execute_sequence_command(c, profiling);
    }else if (c_type == SIMPLE_COMMAND){
        execute_simple_command(c, profiling);
    }else if (c_type == SUBSHELL_COMMAND){
        execute_subshell_command(c, profiling);
    }else if (c_type == WHILE_COMMAND){
        execute_while_command(c, profiling);
    }else if (c_type == UNTIL_COMMAND){
        execute_until_command(c, profiling);
    }else{
        error(1,0,"Command not found!\n");
    }
}