/*  INF3173 - TP2
*  Session : automne 2021
*  Tous les groupes
*  
*  IDENTIFICATION.
*
*      Nom : Hamel Bourdeau
*      Prénom : Alexandre
*      Code permanent : HAMA12128907
*      Groupe : 20
*/

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

#define DELIMITER       ":"
#define OUTPUT_FILENAME "count"

bool stats_file_created = false;
int pipeline_cmd_to_count[2];
int pipeline_count_to_cmd[2];
const int *CMD_COUNT_INPUT = &pipeline_cmd_to_count[0];
const int *CMD_COUNT_OUTPUT = &pipeline_cmd_to_count[1];
const int *COUNT_CMD_INPUT = &pipeline_count_to_cmd[0];
const int *COUNT_CMD_OUTPUT = &pipeline_count_to_cmd[1];    

void handle_signal(int signal) {
    
    printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Attrapé signal %i\n", signal);
    
}

void quit(int exit_code) {
    exit(exit_code);
}

int count_delimiter_in_string(const int argc, const char **argv) {
    int nb_delimiter = 0;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], DELIMITER) == 0) { nb_delimiter++; }
    }
    
    return nb_delimiter;
}

int count_bytes(const int pipeline_input, const int pipeline_output) {
    const int BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE];
    int read_bytes, total_bytes_read = 0;
    
    while (true) {
        read_bytes = read(pipeline_input, buffer, BUFFER_SIZE);
        if (read_bytes == -1) { quit(1); }
        if (read_bytes == 0) { break; }
        total_bytes_read += read_bytes;
        if (write(pipeline_output, buffer, read_bytes) == -1) { quit(1); }
    }

    return total_bytes_read;
}

void write_stats_to_file(const int pipe, const int bytes) {
    if (bytes > 0) {
        FILE * stats_file_fd;
        char line_to_write[32];
        sprintf(line_to_write, "%i : %i\n", pipe, bytes);
        
        if (stats_file_created == false) {
            stats_file_fd = fopen(OUTPUT_FILENAME, "w");
        } else {
            stats_file_fd = fopen(OUTPUT_FILENAME, "a");
        }
        
        if (stats_file_fd == NULL) { quit(1); }
        fprintf(stats_file_fd, "%s", line_to_write);
        fclose(stats_file_fd);
        stats_file_created = true;
    }
}

void get_cmd_pos_in_string(int array[], int *array_size, const int argc, char **argv) {
    if (argc > 0) {
        array[0] = 1;   // First command always at pos 1
        
        int j = 1;
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], DELIMITER) == 0) {
                array[j] = i + 1;
                j++;
            }
        }
        
        *array_size = j;
    }
}

void replace_all_delimiter_with_null(const int argc, char **argv) {
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], DELIMITER) == 0) {
            argv[i] = NULL;
        }
    }
}

int main(int argc, char **argv) {
    // Get the number of commands to execute and their position in argv
    int nb_cmd_to_execute = 0;
    int cmd_pos_in_argv[argc];
    get_cmd_pos_in_string(cmd_pos_in_argv, &nb_cmd_to_execute, argc, argv);
    replace_all_delimiter_with_null(argc, argv);
    
    for (int cmd_pos = 0; cmd_pos < nb_cmd_to_execute; cmd_pos++) {
        const int CURRENT_CMD_POS_IN_ARGV = cmd_pos_in_argv[cmd_pos];
        
        // Count the bytes from cmd_to_count pipeline
        // New count_to_cmd pipeline to transfer data to next command
        if (nb_cmd_to_execute >= 2 && cmd_pos > 0) {
            if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipeline_count_to_cmd) == -1) { quit(1); }
            const int TOTAL_BYTES_READ = count_bytes(*CMD_COUNT_INPUT, *COUNT_CMD_OUTPUT);
            close(*CMD_COUNT_INPUT);
            close(*COUNT_CMD_OUTPUT);
            write_stats_to_file(cmd_pos, TOTAL_BYTES_READ);
        }
    
        // New cmd_to_count pipeline to transfer data to next count
        if (nb_cmd_to_execute >= 2 && cmd_pos != (nb_cmd_to_execute - 1)) {
            if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipeline_cmd_to_count) == -1) { quit(1); }
        }
        
        int exec_status = 0;
        const pid_t CMD_PROC_PID = fork();
        (CMD_PROC_PID < 0) ? quit(1) : NULL;        
        
        // ===== CHILD PROCESS =====
        if (CMD_PROC_PID == 0) {

            // Duplicate standard input to count_to_cmd pipeline
            if (nb_cmd_to_execute >= 2 && cmd_pos > 0) {
                dup2(*COUNT_CMD_INPUT, STDIN_FILENO);
                close(*COUNT_CMD_INPUT);
            }
            
            // Duplicate standard output to cmd_to_count pipeline
            if (nb_cmd_to_execute >= 2 && cmd_pos != (nb_cmd_to_execute - 1)) {
                dup2(*CMD_COUNT_OUTPUT, STDOUT_FILENO);
                close(*CMD_COUNT_INPUT);
                close(*CMD_COUNT_OUTPUT);
            }
            
            execvp(argv[CURRENT_CMD_POS_IN_ARGV], &argv[CURRENT_CMD_POS_IN_ARGV]);
            
        // ===== PARENT PROCESS =====
        } else {
            waitpid(CMD_PROC_PID, &exec_status, 0);
            if (WIFSIGNALED(exec_status) == 1) { quit(128 + exec_status); }
            close(*CMD_COUNT_OUTPUT);
        }
    
    if (exec_status != 0) { quit(1); }
    }
    
    return 0;
}
