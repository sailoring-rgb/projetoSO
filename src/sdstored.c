/*
ARGUMENTOS SERVDOR:
    -> path do ficheiro de configuração
    -> path dos executáveis das transformações
*/

// Server
#include "helper.h"
#define serverError "[ERROR] Server not running.\n"
#define SIZE 256

// Transformation information
typedef struct Transformation{
    char operation_name[SIZE];
    int max_operation_allowed;
    int currently_running;
} Trans;

// Server configuration
typedef struct ServerConfig{
    Trans config[7];
} * ServerConfig;



// Function for loading server
int loadServer(char const * path, ServerConfig * sc){
    char buffer[MAX_BUFF_SIZE];
    int configured = 0;
    ServerConfig tmp_sc = NULL;

    int config_fd = open(path, O_RDONLY);

    if(config_fd < 0)
        return 0;
    
    tmp_sc = malloc(sizeof(struct Transformation) * 7);
    
    while(readln(config_fd, buffer, MAX_BUFF_SIZE) && configured < 7){
        // process line and add info to struct
        configured++;
    }

    close(config_fd);
    * sc = tmp_sc;
    return 1;
}

int main(int argc, char *argv[]){
    if(argc != 2){
        printError(argCountError);
        return 0;
    }
    else{
        // OPEN AND LOAD SERVER CONFIGURATION
        ServerConfig sc = NULL;
        if(!loadServer(argv[1], &sc)){
            printError(serverError);
            return 0;
        }
        // PERFORM WORK ON THE SERVER
        // OPEN TRANSFORMATION FOLDERS
    }
    return 0;
}