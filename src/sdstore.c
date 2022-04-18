/*
ARGUMENTOS CLIENTE:
    -> status
        * verifica o estado do pedido de transformações
    -> proc-file
        * path para o ficheiro a transformar
        * path onde é guardada a nova versão
        * sequencia de ids das transformações a executar
*/

// Client
#include "helper.h"

int main(int argc, char *argv[]){
    if(argc < 2){
        printError(argCountError);
        return 0;
    }
    else{
        if(argc == 2){
            if(strcmp(argv[1],"status") == 0){
                // code for status option
                // creation of named pipe
                // checkStatus(PIPE_READ,PIPE_WRITE);
            }
            else printError(argError);
        }
        else if (strcmp(argv[1],"proc-file") == 0){
            // code for proc-file option
            // creation of named pipe
        }
        else printError(argError);
    }
    return 0;
}