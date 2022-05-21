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
#define requestError "[ERROR] Invalid request.\n"

#define MAX_BUFF_SIZE 1024

// Global variables
char fifo[] = "../tmp/fifo";
int channel;

// Function for printing errors
void printMessage(char msg[]){
    write(STDOUT_FILENO, msg, strlen(msg));
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

// Function for splitting lines into tokens
int lineSplitter(char src[], char *dest[]){
    char * token;
    int size_dest = 0;

    token = strtok(src, " ");
    while(token){
        dest[size_dest] = token;
        token = strtok(NULL, " ");
        size_dest++;
    }

    return size_dest;

}