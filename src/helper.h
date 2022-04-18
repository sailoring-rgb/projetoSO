#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define argCountError "[ERROR] Insufficient number of arguments.\n"
#define argError "[ERROR] Invalid arguments.\n"
#define MAX_BUFF_SIZE 1024


// Function for viewing status information
void checkStatus(int r_fd, int w_fd){
    char buffer[MAX_BUFF_SIZE];
    int read_bytes;

    write(w_fd, "status", strlen("status"));
    while((read_bytes = read(r_fd, buffer, MAX_BUFF_SIZE)) > 0 )
        write(STDOUT_FILENO,buffer,read_bytes);
}

// Function for printing errors
void printError(char error[]){
    write(STDOUT_FILENO, error, strlen(error));
}

// Function for reading lines in files
ssize_t readln(int fd, char *line, size_t size){
    ssize_t read_bytes, total_read_bytes = 0;
    
    while(total_read_bytes < size -1){
        read_bytes = read(fd,&line[total_read_bytes],sizeof(char));
        if (read_bytes < 0) break;
        if(line[total_read_bytes++] == '\n') break;
    }
    line[total_read_bytes] = '\0';

    return total_read_bytes;
}