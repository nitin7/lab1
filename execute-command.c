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

void executeSimple(command_t cmd, char* input, char* output, int profilingSwitch);
void executeSubshell(command_t cmd, char* input, char* output, int profilingSwitch);
void executePipe(command_t cmd, int profilingSwitch);
void executeIf(command_t cmd, int profilingSwitch);
void executeSequence(command_t cmd, int profilingSwitch);
void executeWhile(command_t cmd, int);
void executeUntil(command_t cmd, int);


void printcmd (int fd, command_t cmd){
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

double calctime(double tv_sec, double tv_nsec){
    tv_nsec/=1000000000;
    return tv_sec+tv_nsec;
}

double calctime2(double tv_sec, double tv_usec){
    tv_usec/=1000000;
    return tv_sec+tv_usec;
}

void checkTime(struct timespec *time, int mono){
    if (mono){
        if(clock_gettime(CLOCK_MONOTONIC, time) < 0){
            fprintf(stderr, "Get Clock Time Error!\n");
            exit(1);
        }
    }else{
        if(clock_gettime(CLOCK_REALTIME, time) < 0){
            fprintf(stderr, "Get Clock Time Error!\n");
            exit(1);
        }
    }
}

void checkUsage(struct rusage *r){
    if (getrusage(RUSAGE_SELF, r) < 0) {
        fprintf(stderr, "Get Clock Time Error!\n");
        exit(1);
    }
}

void writeProfiling(command_t c, int profilingSwitch, struct timespec start_m_time, struct timespec end_m_time, struct timespec end_real_time, struct rusage usage){
    double end_sec = end_real_time.tv_sec;
    double end_nsec = end_real_time.tv_nsec;
    double end_total = calctime(end_sec, end_nsec);
    double end_m_sec = end_m_time.tv_sec;
    double end_m_nsec = end_m_time.tv_nsec;
    double end_m_total = calctime(end_m_sec, end_m_nsec);
    double start_m_sec = start_m_time.tv_sec;
    double start_m_nsec = start_m_time.tv_nsec;
    double start_m_total = calctime(start_m_sec, start_m_nsec);
    double duration = end_m_total - start_m_total;
    
    double user_cpu_sec = usage.ru_utime.tv_sec;
    double user_cpu_usec = usage.ru_utime.tv_usec;
    double user_cpu = calctime2(user_cpu_sec, user_cpu_usec);
    double sys_cpu_sec = usage.ru_stime.tv_sec;
    double sys_cpu_usec = usage.ru_stime.tv_usec;
    double sys_cpu = calctime2(sys_cpu_sec, sys_cpu_usec);    
    
    
    char str[1023];
    sprintf(str, "%.2f ", end_total);
    write(profilingSwitch, str, strlen(str));
    sprintf(str, "%.3f ", duration);
    write(profilingSwitch, str, strlen(str));
    sprintf(str, "%.3f ", user_cpu);
    write(profilingSwitch, str, strlen(str));
    sprintf(str, "%.3f ", sys_cpu);
    write(profilingSwitch, str, strlen(str));
    printcmd(profilingSwitch,c);
    write(profilingSwitch, "[", 1);
    int pid=getpid();
    sprintf(str, "%d", pid);
    write(profilingSwitch, str, strlen(str));
    write(profilingSwitch, "]", 1);
    write(profilingSwitch, "\n", 1);
}

int prepare_profiling(char const *name){
  /* FIXME: Replace this with your implementation.  You may need to
     add auxiliary functions and otherwise modify the source code.
     You can also use external functions defined in the GNU C Library.  */
    int fd;
    fd = open(name, O_WRONLY|O_CREAT|O_TRUNC, 0666);
    if(fd < 0) {
        fprintf(stderr, "Cannot write to file: %s", name);
        exit(1);
    }
    return fd;
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

void executeSimple(command_t c, char* input, char* output, int profilingSwitch){
    int status;
    pid_t pid = fork();

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
                error(1, 0,"Failed to open output file. \n");
            if (dup2(output_file, 1) < 0) 
                error(1, 0, "Failed dup2 \n");

            close(output_file);
        } 

        if (execvp(*(c->u.word), c->u.word) < 0)
            error(1, 0, "Failed to execute command. \n");
    }
    return;
}

void executePipe(command_t c, int profiling){
    struct timespec times[3];
    struct rusage usage;
    checkTime(&times[0], 1);

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

    checkTime(&times[1], 0);
    checkTime(&times[2], 1);
    checkUsage(&usage);
    writeProfiling(c, profiling, times[0], times[1], times[2], usage);

    return;
}

void executeIf(command_t c, int profilingSwitch){
    struct timespec times[3];
    struct rusage usage;
    checkTime(&times[0], 1);

    selectCommand(c, 0, profilingSwitch);
    if (c->u.command[0]->status == EXIT_SUCCESS)
        selectCommand(c, 1, profilingSwitch);
    else if (c->u.command[2])
        selectCommand(c, 2, profilingSwitch);

    checkTime(&times[1], 0);
    checkTime(&times[2], 1);
    checkUsage(&usage);
    writeProfiling(c, profilingSwitch, times[0], times[1], times[2], usage);

    return;
}

void executeSequence(command_t c, int profilingSwitch){
    struct timespec times[3];
    struct rusage usage;
    checkTime(&times[0], 1);

    selectCommand(c, 0, profilingSwitch);
    selectCommand(c, 1, profilingSwitch);

    checkTime(&times[1], 0);
    checkTime(&times[2], 1);
    checkUsage(&usage);
    writeProfiling(c, profilingSwitch, times[0], times[1], times[2], usage);
    return;
}

void executeSubshell(command_t c, char* input, char* output, int profilingSwitch){
    struct timespec times[3];
    struct rusage usage;
    checkTime(&times[0], 1);

    if (input != NULL) {
        c->u.command[0]->input = input;
    }
    if (output != NULL) {
        c->u.command[0]->output = output;
    }

    selectCommand(c, 0, profilingSwitch);

    checkTime(&times[1], 0);
    checkTime(&times[2], 1);
    checkUsage(&usage);
    writeProfiling(c, profilingSwitch, times[0], times[1], times[2], usage);
    return;
}


void executeWhile(command_t c, int profilingSwitch){
    struct timespec times[3];
    struct rusage usage;
    checkTime(&times[0], 1);

    selectCommand(c, 0, profilingSwitch);
    while (c->u.command[0]->status == EXIT_SUCCESS)
    {
        selectCommand(c, 1, profilingSwitch);
        selectCommand(c, 0, profilingSwitch);
    }

    checkTime(&times[1], 0);
    checkTime(&times[2], 1);
    checkUsage(&usage);
    writeProfiling(c, profilingSwitch, times[0], times[1], times[2], usage);
    return;
}

void executeUntil(command_t c, int profilingSwitch){
    struct timespec times[3];
    struct rusage usage;
    checkTime(&times[0], 1);

    selectCommand(c, 0, profilingSwitch);
    while (c->u.command[0]->status != EXIT_SUCCESS)
    {
        selectCommand(c, 1, profilingSwitch);
        selectCommand(c, 0, profilingSwitch);
    }

    checkTime(&times[1], 0);
    checkTime(&times[2], 1);
    checkUsage(&usage);
    writeProfiling(c, profilingSwitch, times[0], times[1], times[2], usage);
    return;
}

void execute_command(command_t c, int profiling){
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
    return;
}