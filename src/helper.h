#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>

#define argCountError "[ERROR] Insufficient number of arguments.\n"
#define argError "[ERROR] Invalid arguments.\n"
#define serverError "[ERROR] Server not running.\n"
#define fifoError "[ERROR] Can't create fifo.\n"
#define requestError "Invalid request.\n"

#define MAX_BUFF_SIZE 1024

// Global variables
char fifo[] = "tmp/fifo";


// Function for viewing status information
void checkStatus(int reader, int writer){
    char buffer[MAX_BUFF_SIZE];
    int read_bytes;

    write(writer, "status", strlen("status"));

    while((read_bytes = read(reader, buffer, MAX_BUFF_SIZE)) > 0 )
        write(STDOUT_FILENO,buffer,read_bytes);
}

// Function for printing errors
void printError(char error[]){
    write(STDOUT_FILENO, error, strlen(error));
}

// Function for reading lines in files
int readLine(int src, char *dest){
    int bytesRead = 0;
    char local[1] = "";
    while(read(src, local, 1) > 0 && local[0] != '\n')
        dest[bytesRead++] = local[0];
    dest[bytesRead] = '\0';
    return bytesRead;
}