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
    ---- COMANDOS FUNCIONAIS ---- 
    ./sdstore status
    ./sdstore proc-file -p 2 ../docs/enunciado.pdf ../docs/teste.pdf nop 
    ./sdstore proc-file -p 3 ../docs/enunciado.pdf ../docs/teste1 nop bcompress bdecompress encrypt decrypt
    ./sdstore proc-file -p 1 ../docs/enunciado.pdf ../docs/teste2 encrypt bcompress
    ./sdstore proc-file ../docs/teste2 ../docs/teste3.pdf nop bdecompress decrypt
    ./sdstore proc-file ../docs/enunciado.pdf ../docs/teste4 gcompress nop bcompress
    ./sdstore proc-file ../docs/enunciado.pdf ../docs/teste5 nop bcompress gdecompress

    ---- COMANDOS COM ERROS ---- 
    FICHEIRO NÃO EXISTE            |    ./sdstore proc-file ../docs/enunciado ../docs/teste6.pdf nop
    TRANSFORMAÇÃO INVÁLIDA         |    ./sdstore proc-file ../docs/enunciado.pdf ../docs/teste7.pdf nada 
    EXCEDE CONFIGURAÇÕES           |    ./sdstore proc-file ../docs/enunciado.pdf ../docs/teste8.pdf nop nop nop nop nop
    NÚMERO DE ARGUMENTOS INVÁLIDOS |    ./sdstore proc-file ../docs/enunciado.pdf ../docs/teste9.pdf
    ARGUMENTOS INVÁLIDOS           |    ./sdstore sta 
*/

// ******************************** INCLUDES ********************************
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <stdlib.h>

// ******************************** DEFINES ********************************
#define argCountError "[ERROR] Insufficient number of arguments.\n"
#define argError "[ERROR] Invalid arguments.\n"
#define serverError "[ERROR] Server not running.\n"
#define fifoError "[ERROR] Can't create fifo.\n"
#define requestError "[ERROR] Invalid request.\n"
#define MAX_BUFF_SIZE 1024

// ******************************** GLOBAL VARIABLES ********************************
char fifo[] = "../tmp/fifo";

// ******************************** FUNCTIONS ********************************
// Function for printing messsages
void printMessage(char msg[]){
    write(STDOUT_FILENO, msg, strlen(msg));
}

// Function to check if command has priority
int priorityCheck(char *argv[]){
    int priority = -1;
    if (strcmp(argv[2], "-p") == 0)
            priority = atoi(argv[3]);
    return priority;
}

// Function to check if the desired transformations are valid
bool validateRequest(int argc, char *argv[]){
    int priority;
    int index = 0;

    if ((priority = priorityCheck(argv)) != -1){
        if(priority > 5 || priority < 0)
            return false;
        index = 6;
    }
    else
        index = 4;

    for(int i = index ; i < argc ; i++)
        if(strcmp(argv[i], "bcompress")!= 0 && strcmp(argv[i], "bdecompress")!= 0 
            && strcmp(argv[i], "decrypt")!= 0 && strcmp(argv[i], "encrypt")!= 0 
            && strcmp(argv[i], "gcompress")!= 0 && strcmp(argv[i], "gdecompress")!= 0
            && strcmp(argv[i], "nop")!= 0)
            return false;
    return true;
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
        return -1;
    }

    int read_bytes, pid, fifo_writer, fifo_reader, channel;
    char pid_reader[32];
    char pid_writer[32];
    char buffer[MAX_BUFF_SIZE];
    bool status = false, proc_file = false;

    pid = getpid();

    sprintf(pid_reader, "../tmp/%d_reader", pid);
    sprintf(pid_writer, "../tmp/%d_writer", pid);

    if(strcmp(argv[1], "status") != 0 && strcmp(argv[1], "proc-file") != 0){
        status = false;
        proc_file = false;
    }
    else if(strcmp(argv[1], "status") == 0)
        status = true;
    else if(strcmp(argv[1], "proc-file") == 0)
        proc_file = true;
    
    if((!status && !proc_file) || (argc < 5 && proc_file) || (status && argc != 2)){
        printMessage(requestError);
        return -1;
        }
        
    if(proc_file && !validateRequest(argc, argv)){
        printMessage(requestError);
        return -1;
    }

    if(mkfifo(pid_reader, 0666) == -1 || mkfifo(pid_writer, 0666) == -1){
        printMessage(fifoError);
        return -1;
    }
    
    if((channel = open(fifo, O_WRONLY)) < 0){
        printMessage(serverError);
        return -1;
    }

    write(channel, &pid, sizeof(pid));
    close(channel);

    fifo_writer = open(pid_writer, O_WRONLY);
    fifo_reader = open(pid_reader, O_RDONLY);

    if(status){
        checkStatus(fifo_reader, fifo_writer);
        close(fifo_reader);
        close(fifo_writer);
        unlink(pid_reader);
        unlink(pid_writer);
        return 0;
    }
    if(proc_file){
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
}