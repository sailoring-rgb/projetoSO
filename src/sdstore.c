/* **** CLIENT **** 
ARGUMENTS:
    -> status
        * check transformation status
    -> proc-file path_input_file path_output_file transformationList[]
        * path_input_file: relative path to file to transform
        * path_output_file: relative path to save transformed file
        * transformationList[]: list of transformations
*/

#include "helper.h"

// Function to check if the desired transformations are valid
bool validateRequest(int argc, char *argv[]){
    bool res = true;
    for(int i = 4; i < argc && res; i++)
        if(strcmp(argv[i], "bcompress")!= 0 && strcmp(argv[i], "bdecompress")!= 0 
            && strcmp(argv[i], "decrypt")!= 0 && strcmp(argv[i], "encrypt")!= 0 
            && strcmp(argv[i], "gcompress")!= 0 && strcmp(argv[i], "gdecompress")!= 0
            && strcmp(argv[i], "nop")!= 0)
            res = false;
    return res;
}

// Function for viewing status information
void checkStatus(int reader, int writer){
    char buffer[MAX_BUFF_SIZE];
    int read_bytes;

    write(writer, "status", strlen("status"));

    read_bytes = read(reader, buffer, MAX_BUFF_SIZE);
    write(STDOUT_FILENO, buffer ,read_bytes);
}

int main(int argc, char *argv[]){
    if(argc < 2){
        printMessage(argCountError);
        return 0;
    }

    int read_bytes, pid, fifo_writer, fifo_reader;
    char pid_reader[32];
    char pid_writer[32];
    char buffer[MAX_BUFF_SIZE];

    pid = getpid();

    sprintf(pid_reader, "../tmp/%d_reader", pid);
    sprintf(pid_writer, "../tmp/%d_writer", pid);

    if(mkfifo(pid_reader, 0666) == -1 || mkfifo(pid_writer, 0666) == -1){
        printMessage(fifoError);
        return 0;
    }
    
    if((channel = open(fifo, O_WRONLY)) < 0){
        printMessage(serverError);
        return 0;
    }

    write(channel, &pid, sizeof(pid));

    fifo_writer = open(pid_writer, O_WRONLY);
    fifo_reader = open(pid_reader, O_RDONLY);

    if(argc == 2){
        if(strcmp(argv[1], "status") == 0){
            checkStatus(fifo_reader,fifo_writer);
            }
        else printMessage(argError);
        close(fifo_reader);
        close(fifo_writer);
        unlink(pid_reader);
        unlink(pid_writer);
        return 0;
    }

    if(argc < 5){
        printMessage(argError);
        close(fifo_reader);
        close(fifo_writer);
        unlink(pid_reader);
        unlink(pid_writer);
        return 0;
    }

    if(strcmp(argv[1], "proc-file") != 0 || validateRequest(argc, argv)){
        printMessage(requestError);
        close(fifo_reader);
        close(fifo_writer);
        unlink(pid_reader);
        unlink(pid_writer);
        return 0;
    }

    // Passing information into buffer
    strcpy(buffer, *(argv + 2));

    write(fifo_writer, buffer, strlen(buffer));

    while((read_bytes = read(fifo_reader, buffer, MAX_BUFF_SIZE)) > 0){
        write(STDOUT_FILENO, buffer, read_bytes);
        fflush(stdout);
        buffer[0] = '\0';
    }

    close(fifo_reader);
    close(fifo_writer);
    unlink(pid_reader);
    unlink(pid_writer);
    close(channel);

    return 0;
}