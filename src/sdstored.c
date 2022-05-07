/* **** SERVER **** 
ARGUMENTS:
    -> path_config path_exec
        * path_config: relative path for configuration file
        * path_exec: relative path for transformations executables
*/

#include "helper.h"

// Transformation information
typedef struct Transformation{
    char operation_name[64];
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
    Trans * ptr = &t;
    while(*ptr && strcmp((*ptr)->operation_name, new->operation_name) < 0)
        ptr = & ((*ptr)->prox);
    new->prox = (*ptr);
    (*ptr) = new;
    return t;
}


// Function for server configurations
int loadServer(char * path[], Trans * tr){
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

// Function for cheking if resources are free
bool checkResources(Trans * tr, char * name){
    while(* tr && (strcmp((*tr)->operation_name, name) != 0))
        tr = & ((*tr)->prox);
    
    if(* tr){
        if ((*tr)->currently_running < (*tr)->max_operation_allowed)
            return true;
    }
    return false;
}

// Function for updating resources
bool updateResource(Trans * tr, char * name, char * opt){
    bool res = false;
    while(* tr && (strcmp((*tr)->operation_name, name) != 0))
        tr = & ((*tr)->prox);
    
    if(* tr){
        if (strcmp(opt, "occupy") == 0){
            (*tr)->currently_running ++;
            res = true;
            }
        else if (strcmp(opt, "free") == 0){
            (*tr)->currently_running --;
            res = true;
            }
    }
    return res;
}

// Routine for handling sigterm
void sigterm_handler(int sig){
    close(channel);
}


int main(int argc, char *argv[]){
    // Checking for argc
    if(argc < 3 || argc > 3){
        printError(argCountError);
        return 0;
    }

    Trans sc = NULL;

    // Loading server configuration
    if(!loadServer(argv, &sc)){
        printError(serverError);
        return 0;
    }

    // Creating communication channel
    if(mkfifo(fifo, 0666) == -1){
        printError(fifoError);
        return 0;
    }

    channel = open(fifo, O_RDWR);

    signal(SIGTERM, sigterm_handler);


    /*
    GENERAL WORKFLOW:
    -> MAIN PROCESS: CHECK REQUEST
    -> SWITCH(REQUEST)
        -> CASE "STATUS":
            -> FORK 
        -> CASE "TRANSFORMATION":
            -> CHECK FOR AVAILABLE RESOURCES
                -> IF TRUE: FORK
                -> ELSE: PUT ON QUEUE
    */

    // [FATHER] VALIDATION OF REQUESTS
    // [CHILD] EXECUTE REQUEST

    /*
    // **** FOR TESTING ****
    Trans * tr = &sc;

    while(* tr && (strcmp((*tr)->operation_name, "bcompress") != 0))
        tr = & ((*tr)->prox);
    if(* tr){
        // WRITE SMTHING HERE
    }
    */

    close(channel);
    unlink(fifo);
    return 0;
}