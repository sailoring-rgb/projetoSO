/*
ARGUMENTOS SERVDOR:
    -> path do ficheiro de configuração
    -> path dos executáveis das transformações
*/

// Server
#include "helper.h"
#include <stdlib.h>
#include <string.h>
#define serverError "[ERROR] Server not running.\n"
#define SIZE 128

// Transformation information
typedef struct Transformation{
    char operation_name[SIZE];
    int max_operation_allowed;
    int currently_running;
    struct Trans * prox;
} * Trans;

// Function to create a transformation
Trans makeTrans(char s[], char const *path){
    Trans t = malloc(sizeof(struct Transformation));
    char * token;
    token = strtok(s, " ");
    strcpy(t->operation_name, token);
    token = strtok(NULL, " ");
    t->max_operation_allowed = atoi(token);
    t->currently_running = 0;
    t->prox = NULL;
    return t;
}

// Function to add a transformation
Trans addTransformation(Trans t, char s[], char const * path){
    Trans new = makeTrans(s, path);
    Trans *ptr = &t;
    while(*ptr && strcmp((*ptr)->operation_name, new->operation_name) < 0)
        ptr = & ((*ptr)->prox);
    new->prox = (*ptr);
    (*ptr) = new;
    return t;
}


// Function for server configurations
int loadServer(char const * path[], Trans * tr){
    char buffer[MAX_BUFF_SIZE];
    Trans tmp_tr = NULL;

    int config_fd = open(path[1], O_RDONLY);

    if(config_fd < 0)
        return 0;
    
    while(readLine(config_fd, buffer))
        tmp_tr = addTransformation(tmp_tr,buffer,path[2]);
    
    close(config_fd);
    * tr = tmp_tr;
    return 1;
}

int main(int argc, char *argv[]){
    if(argc != 3){
        printError(argCountError);
        return 0;
    }
    else{
        // Loading server configuration
        Trans sc = NULL;
        if(!loadServer(argv, &sc)){
            printError(serverError);
            return 0;
        }
    }
    return 0;
}