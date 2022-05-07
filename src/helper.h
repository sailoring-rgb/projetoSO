#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define argCountError "[ERROR] Insufficient number of arguments.\n"
#define argError "[ERROR] Invalid arguments.\n"
#define serverError "[ERROR] Server not running.\n"
#define fifoError "[ERROR] Can't create fifo.\n"
#define requestError "Invalid request.\n"

#define MAX_BUFF_SIZE 1024

// Global variables
char fifo[] = "../tmp/fifo";
int channel;

// Function for viewing status information
void checkStatus(int reader, int writer){
    char buffer[MAX_BUFF_SIZE];
    int read_bytes;

    write(writer, "status", strlen("status"));

    while((read_bytes = read(reader, buffer, MAX_BUFF_SIZE)) > 0 )
        write(STDOUT_FILENO, buffer ,read_bytes);
}

// Function for printing errors
void printError(char error[]){
    write(STDOUT_FILENO, error, strlen(error));
}

// Function for reading lines in files
int readLine(int src, char *dest){
    int read_bytes = 0;
    char tmp[1] = "";
    while(read(src, tmp, 1) > 0 && tmp[0] != '\n')
        dest[read_bytes++] = tmp[0];
    dest[read_bytes] = '\0';
    return read_bytes;
}