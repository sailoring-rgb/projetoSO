/* ******************************** CLIENT ********************************
ARGUMENTS:
    -> status
        * check transformation status
    -> proc-file priority path_input_file path_output_file transformationList[]
        * transformation priority (number from 0 to 5, being 5 the max priority)
        * path_input_file: relative path to file to transform
        * path_output_file: relative path to save transformed file
        * transformationList[]: list of transformations
*/

/* ******************************** COMMANDS ********************************
    ./sdstore status
    ./sdstore proc-file 0 ../docs/enunciado.pdf ../docs/teste/ nop    <---- COM PRIORIDADE
    ./sdstore proc-file 2 ../docs/enunciado.pdf ../docs/teste/ nop    <---- COM PRIORIDADE
    ./sdstore proc-file ../docs/enunciado.pdf ../docs/teste/ nop
*/

// ******************************** INCLUDES ********************************
#include "helper.h"

// ******************************** FUNCTIONS ********************************
// Function to check if the desired transformations are valid
bool validateRequest(int argc, char *argv[]){
    bool res = true;
    int index = 0;
    if (strcmp(argv[2], "0") == 0 || strcmp(argv[2], "1") == 0 || strcmp(argv[2], "2") == 0 ||
            strcmp(argv[2], "3") == 0 || strcmp(argv[2], "4") == 0 || strcmp(argv[2], "5") == 0)
            index = 5;
    else
        index = 4;
    for(int i = index ; i < argc && res; i++)
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

    while((read_bytes = read(reader, buffer, MAX_BUFF_SIZE)) > 0 )
        write(STDOUT_FILENO, buffer, read_bytes);
}

// ******************************** MAIN ********************************
int main(int argc, char *argv[]){
    if(argc < 2){
        printMessage(argCountError);
        return 0;
    }

    int read_bytes, pid, fifo_writer, fifo_reader, channel;
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
    close(channel);

    fifo_writer = open(pid_writer, O_WRONLY);
    fifo_reader = open(pid_reader, O_RDONLY);

    if(argc == 2){
        if(strcmp(argv[1], "status") == 0)
            checkStatus(fifo_reader, fifo_writer);
        else
            printMessage(argError);
        close(fifo_reader);
        close(fifo_writer);
        unlink(pid_reader);
        unlink(pid_writer);
        return 0;
    }

    if(argc < 5 || strcmp(argv[1], "proc-file") != 0 || !validateRequest(argc, argv)){
        if (argc < 5)
            printMessage(argError);
        else
            printMessage(requestError);
        close(fifo_reader);
        close(fifo_writer);
        unlink(pid_reader);
        unlink(pid_writer);
        return 0;
    }

    // Passing information into buffer
    buffer[0] = '\0';
    for(int i = 1; i < argc; i++){
        strcat(buffer, argv[i]);
        if(i != argc -1)
            strcat(buffer, " ");
    }

    write(fifo_writer, buffer, strlen(buffer));

    while((read_bytes = read(fifo_reader, buffer, MAX_BUFF_SIZE)) > 0){
        buffer[read_bytes] = '\0';
        write(STDOUT_FILENO, buffer, read_bytes);
        fflush(stdout);
        buffer[0] = '\0';
    }

    close(fifo_reader);
    close(fifo_writer);
    unlink(pid_reader);
    unlink(pid_writer);

    return 0;
}