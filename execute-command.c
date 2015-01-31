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
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <time.h>

void executeSimple(command_t cmd, char* input, char* output, int profiling);
void executeSubshell(command_t cmd, char* input, char* output, int profiling);
void executePipe(command_t cmd, int profiling);
void executeIf(command_t cmd, int profiling);
void executeSequence(command_t cmd, int profiling);
void executeWhile(command_t cmd, int profiling);
void executeUntil(command_t cmd, int profiling);
void execute_command(command_t c, int profiling);
void selectCommand(command_t c, int i, int profiling);
double calctime(double sec, double nORusec, double size);
void checkTime(struct timespec *time, int mono);
void checkUsage(struct rusage *r, int self);
void writeProfiling(command_t c, int profiling, struct timespec* times, struct rusage r_usg, struct rusage r_usg_ch, int simple);


double calctime(double sec, double nORusec, double size) {
    return sec+(nORusec/size);
}


void checkTime(struct timespec *time, int mono) {
    int ret = -1;

    if (mono)
        ret = clock_gettime(CLOCK_MONOTONIC, time);
    else
        ret = clock_gettime(CLOCK_REALTIME, time);

    if (ret == -1) {
        fprintf(stderr, "Error in clock_gettime!\n");
        exit(1);
    }
}

void checkUsage(struct rusage *r, int self) {
    int ret = -1;

    if (self) 
        ret = getrusage(RUSAGE_SELF, r);
    else 
        ret = getrusage(RUSAGE_CHILDREN, r);
    
    if (ret == -1) {
        fprintf(stderr, "Error in getrusage!\n");
        exit(1);
    }
}

void writeProfiling(command_t c, int profiling, struct timespec* times, struct rusage r_usg, struct rusage r_usg_ch, int simple) {

    int buf_size = 1023;
    char buf[buf_size];

    double size = 1000000000.0;
    double exec_time = calctime(times[1].tv_sec, times[1].tv_nsec, size) - calctime(times[0].tv_sec, times[0].tv_nsec, size);
    double finish_time = calctime(times[2].tv_sec, times[2].tv_nsec, size);

    size = 1000000.0;
    double user_cpu_time = calctime(r_usg.ru_utime.tv_sec, r_usg.ru_utime.tv_usec, size);
    double system_cpu_time = calctime(r_usg.ru_stime.tv_sec, r_usg.ru_stime.tv_usec, size);    
    
    if (simple) {
        double user_cpu_time_ch = calctime(r_usg_ch.ru_utime.tv_sec, r_usg_ch.ru_utime.tv_usec, size);
        double system_cpu_time_ch = calctime(r_usg_ch.ru_stime.tv_sec, r_usg_ch.ru_stime.tv_usec, size);

        user_cpu_time += user_cpu_time_ch;
        system_cpu_time +=system_cpu_time_ch;
    }

    int num_chars = snprintf(buf, buf_size, "%.9f %.9f %.9f %.9f ", finish_time, exec_time, user_cpu_time, system_cpu_time);
    write(profiling, buf, num_chars);

    if (c != NULL) {
      char **w = c->u.word;
      write(profiling, *w, strlen(*w));
      while (*(++w)) {
          write(profiling, " ", 1);
          write(profiling, *w, strlen(*w));
      }
      write(profiling, "\n", 1);
    }
    else {
      int pid = getpid();
      num_chars = snprintf(buf, buf_size, "[%d]\n", pid); 
      write(profiling, buf, num_chars);
    }
}


int prepare_profiling(char const *name) {
  /* FIXME: Replace this with your implementation.  You may need to
     add auxiliary functions and otherwise modify the source code.
     You can also use external functions defined in the GNU C Library.  */
    int file = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (file == -1) {
        fprintf(stderr, "Error. File: %s could not be written to!\n", name);
        exit(1);
    }
    return file;
}

int command_status(command_t c) {
    int status = c->status;
    return status;
}

void selectCommand(command_t c, int i, int profiling) {

    struct command *cmd = c->u.command[i];
    char* input = cmd->input;
    char* output = cmd->output;
    enum command_type c_type = cmd->type;
    if (c_type == IF_COMMAND) {
        executeIf(cmd,profiling);
    } else if (c_type == PIPE_COMMAND) {
        executePipe(cmd,profiling);
    } else if (c_type == SEQUENCE_COMMAND) {
        executeSequence(cmd,profiling);
    } else if (c_type == SIMPLE_COMMAND) {
        executeSimple(cmd, input, output, profiling);
    } else if (c_type == SUBSHELL_COMMAND) {
        executeSubshell(cmd, input, output, profiling);
    } else if (c_type == WHILE_COMMAND) {
        executeWhile(cmd,profiling);
    } else if (c_type == UNTIL_COMMAND) {
        executeUntil(cmd,profiling);
    } else {
        error(1,0,"Command not found!\n");
    }
    return;
}

void executeSimple(command_t c, char* input, char* output, int profiling)
{
    struct rusage r_usg;
    struct rusage r_usg_ch;
    struct timespec times[3];
    checkTime(&times[0], 1);

    int status;
    pid_t pid = fork();

    if (pid > 0)
    {
        waitpid(pid, &status, 0);
        c->status = WEXITSTATUS(status);
        checkTime(&times[2], 0);
        checkTime(&times[1], 1); 
        checkUsage(&r_usg, 1);
        checkUsage(&r_usg_ch, 0);
        writeProfiling(c, profiling, times, r_usg, r_usg_ch, 1);
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
                error(1, 0,"Failed to open output file. \n");
            if (dup2(output_file, 1) < 0) 
                error(1, 0, "Failed dup2 \n");

            close(output_file);
        } 

        if (strcmp(c->u.word[0], "exec") == 0)
            execvp(c->u.word[1], c->u.word+1);
        else
            execvp(c->u.word[0], c->u.word);
    }
    return;
}

void executePipe(command_t c, int profiling)
{
    struct rusage r_usg;
    struct timespec times[3];
    checkTime(&times[0], 1);

    int status;
    int file_d[2];
    int *t0 = &file_d[0];
    int *t1 = &file_d[1];
    pid_t pid1 = fork();

    if (pipe(file_d) < 0) {
        error(1, 0, "executePipe(): Failed pipe.\n");
    } else if (pid1 < 0) {
        error(1, 0, "executePipe(): Failed fork.\n");
    } else if (pid1 == 0) {
        pid_t pid2 = fork();
        if (pid2 < 0) { 
            error(1, 0, "executePipe(): Failed fork.\n");
        } else if (pid2 == 0) {
            close(*t0);
            if (dup2(*t1, 1) < 0) 
                error(1,0,"executePipe(): Failed dup2.\n");
            selectCommand(c, 0, profiling);
            _exit(c->u.command[0]->status);
        } else {
            waitpid(pid2, &status, 0);
            close(*t1);
            if (dup2(*t0, 0) < 0) 
                error(1,0,"executePipe(): Failed dup2.\n");
            selectCommand(c, 1, profiling);
            _exit(c->u.command[1]->status);
        }
    } else {
        close(*t0);
        close(*t1);
        waitpid(pid1, &status, 0);
    }

    checkTime(&times[2], 0);
    checkTime(&times[1], 1); 
    checkUsage(&r_usg, 1);

    writeProfiling(NULL, profiling, times, r_usg, r_usg, 0);

    return;
}


void executeIf(command_t c, int profiling) {

    selectCommand(c, 0, profiling);
    if (c->u.command[0]->status == EXIT_SUCCESS)
        selectCommand(c, 1, profiling);
    else if (c->u.command[2])
        selectCommand(c, 2, profiling);

    return;
}

void executeSequence(command_t c, int profiling) {

    selectCommand(c, 0, profiling);
    selectCommand(c, 1, profiling);

    return;
}

void executeSubshell(command_t c, char* input, char* output, int profiling) {

    struct rusage r_usg;
    struct timespec times[3];
    checkTime(&times[0], 1);

    if (input != NULL) {
        c->u.command[0]->input = input;
    }
    if (output != NULL) {
        c->u.command[0]->output = output;
    }

    selectCommand(c, 0, profiling);

    checkTime(&times[2], 0);
    checkTime(&times[1], 1); 
    checkUsage(&r_usg, 1);
    writeProfiling(NULL, profiling, times, r_usg, r_usg, 0);

    return;
}

void executeWhile(command_t c, int profiling) {

    selectCommand(c, 0, profiling);
    while (c->u.command[0]->status == EXIT_SUCCESS)
    {
        selectCommand(c, 1, profiling);
        selectCommand(c, 0, profiling);
    }

    return;
}

void executeUntil(command_t c, int profiling) {

    selectCommand(c, 0, profiling);
    while (c->u.command[0]->status != EXIT_SUCCESS)
    {
        selectCommand(c, 1, profiling);
        selectCommand(c, 0, profiling);
    }

    return;
}

void execute_command(command_t c, int profiling) {

    struct rusage r_usg;
    struct timespec times[3];
    checkTime(&times[0], 1);

    enum command_type c_type = c->type;
    char* input = c->input;
    char* output = c->output;

    if (c_type == IF_COMMAND) {
        executeIf(c, profiling);
    } else if (c_type == PIPE_COMMAND) {
        executePipe(c, profiling);
    } else if (c_type == SEQUENCE_COMMAND) {
        executeSequence(c, profiling);
    } else if (c_type == SIMPLE_COMMAND) {
        executeSimple(c, input, output, profiling);
    } else if (c_type == SUBSHELL_COMMAND) {
        executeSubshell(c, input, output, profiling);
    } else if (c_type == WHILE_COMMAND) {
        executeWhile(c, profiling);
    } else if (c_type == UNTIL_COMMAND) {
        executeUntil(c, profiling);
    } else {
        error(1,0,"Command not found!\n");
    }

    checkTime(&times[2], 0);
    checkTime(&times[1], 1); 
    checkUsage(&r_usg, 1);
    writeProfiling(NULL, profiling, times, r_usg, r_usg, 0);

    return;
}