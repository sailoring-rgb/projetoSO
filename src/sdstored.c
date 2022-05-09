/* **** SERVER **** 
ARGUMENTS:
    -> path_config path_exec
        * path_config: relative path for configuration file
        * path_exec: relative path for transformations executables
*/

#include "helper.h"
#define path_SDStore "SDStore-transf/"
#define forkError "[ERROR] Fork unsuccessful.\n"
#define signalError "[ERROR] Signal handler not established.\n"

bool reading_allowed;
int total_nr_tasks;
// Transformation information
typedef struct Transformation{
    char operation_name[64];
    int max_operation_allowed;
    int currently_running;
    struct Trans * next;
} * Trans;

// Task information
typedef struct Task{
    int id;
    char command[128];
    char status[16];
    struct Task * next;
} * Task;

// Function to create a transformation
Trans makeTrans(char s[]){
    Trans t = malloc(sizeof(struct Transformation));
    char * token;
    token = strtok(s, " ");
    strcpy(t->operation_name, token);
    token = strtok(NULL, " ");
    t->max_operation_allowed = atoi(token);
    t->currently_running = 0;
    t->next = NULL;
    return t;
}

// Function to add a transformation
Trans addTransformation(Trans t, char s[]){
    Trans new = makeTrans(s);
    Trans * ptr = &t;
    while(*ptr && strcmp((*ptr)->operation_name, new->operation_name) < 0)
        ptr = & ((*ptr)->next);
    new->next = (*ptr);
    (*ptr) = new;
    return t;
}

// Function to create a task
Task makeTask(char name[]){
    Task t = malloc(sizeof(struct Task));
    strcpy(t->command, name);
    strcpy(t->status, "n/a");
    total_nr_tasks ++;
    t->id = total_nr_tasks;
    t->next = NULL;
    return t;
}

// Function to add tasks
Task addTask(Task t, char name[]){
    Trans new = makeTask(name);
    Trans * ptr = &t;
    while(*ptr)
        ptr = & ((*ptr)->next);
    new->next = (*ptr);
    (*ptr) = new;
    return t;
}
// Function to load a task
int loadTask(char name[], Task * tk){
    Task tmp_tk = NULL;

    tmp_tk = addTask(tmp_tk, name);
    while(* tk)
        tk = & ((*tk)->next);
    * tk = tmp_tk;
    return 1;
}

// Function to create pipes
void makePipes(int file_des[][2], int nrPipes){
    int i;
    for(i = 0; i < nrPipes; i++)
        pipe(file_des[i]);
}

// Function to close pipes
void closePipes(int file_des[][2], int nrPipes){
    int i;
    for (i= 0; i < nrPipes; i++){
        close(file_des[i][0]);
        close(file_des[i][1]);
    }
}

// Aply a given transformation
bool transformFile(char * file_path, char * transList[], int nrTrans){
    // OPEN File, apply transformations, close
    return true;
}

// Function for server configurations
int loadServer(char * path[], Trans * tr){
    char buffer[MAX_BUFF_SIZE];
    Trans tmp_tr = NULL;

    int config_fd = open(path[1], O_RDONLY);

    if(config_fd < 0)
        return 0;
    
    while(readLine(config_fd, buffer))
        tmp_tr = addTransformation(tmp_tr,buffer);
    
    close(config_fd);
    * tr = tmp_tr;
    return 1;
}

// Function for cheking if resources are free
bool checkResources(Trans * tr, char * name, int nrTrans){
    while(* tr && (strcmp((*tr)->operation_name, name) != 0))
        tr = & ((*tr)->next);
    
    if(* tr)
        if (((*tr)->currently_running + nrTrans) < (*tr)->max_operation_allowed)
            return true;

    return false;
}

// Routine for handling sigterm
void sigterm_handler(int sig){
    close(channel);
}

// Function to show server status
void sendStatus(int writer, Trans * tr, Task * t){
    char buff[MAX_BUFF_SIZE]= "";
    int total_bytes = 0;

    while(* tr){
    total_bytes = sprintf(
        buff, "[Transformation] %s: %d/%d running/max\n",
        (*tr)->operation_name, 
        (*tr)->currently_running,
        (*tr)->max_operation_allowed);

    write(writer, buff, total_bytes);
    buff[0]= "\0";

    tr = & ((*tr)->next);
    }

    while(* t){
    total_bytes = sprintf(
        buff, "[Task] %d: %s\n%s\n",
        (*t)->id,
        (*t)->command,
        (*t)->status);

    write(writer, buff, total_bytes);
    buff[0]= "\0";
    
    t = & ((*t)->next);
    }
}


int main(int argc, char *argv[]){
    // Checking for argc
    if(argc < 3 || argc > 3){
        printMessage(argCountError);
        return 0;
    }

    Trans sc = NULL;
    Task tasks = NULL;

    // Loading server configuration
    if(!loadServer(argv, &sc)){
        printMessage(serverError);
        return 0;
    }

    if (signal(SIGINT, sigterm_handler) == SIG_ERR)
        printMessage(signalError);

    // Creating communication channel
    if(mkfifo(fifo, 0666) == -1){
        printMessage(fifoError);
        return 0;
    }

    int pid, fifo_reader, fifo_writer, read_bytes, nr_requests;
    char pid_reading[32], pid_writing[32], buffer[MAX_BUFF_SIZE];
    char * requests[MAX_BUFF_SIZE];
    channel = open(fifo, O_RDWR);
    reading_allowed = true;
    total_nr_tasks = 0;

    while(read(channel, &pid, sizeof(pid)) > 0){
        switch(fork()){
        case -1:
            printMessage(forkError);
            return false;
        case 0:
            //[SON]
            sprintf(pid_reading,"../tmp/%d_reader",pid);
            sprintf(pid_writing,"../tmp/%d_writer",pid);
            fifo_reader = open(pid_writing, O_RDONLY);
            fifo_writer = open(pid_reading, O_WRONLY);
            read_bytes = read(fifo_reader, &buffer, MAX_BUFF_SIZE);
            buffer[read_bytes] = '\0';

            if(strcmp(buffer, "status") == 0){
                sendStatus(fifo_writer, &sc, &tasks);
            }
            else{
                char newTask[MAX_BUFF_SIZE] = "transform ";
                strcpy(newTask, buffer);
                nr_requests = lineSplitter(buffer, requests);

                /*
                executar transformações
                -> verificar se há recursos
                    * se não houver: escrever "pending" para o fifo
                    * quando houver avisar que está a ser executado
                */
            }
            close(fifo_reader);
            close(fifo_writer);
            _exit(0);
        }
    }

    unlink(fifo);
    return 0;
}

  /*
    // **** FOR TESTING ****
    Trans * tr = &sc;

    while(* tr && (strcmp((*tr)->operation_name, "bcompress") != 0))
        tr = & ((*tr)->next);
    if(* tr){
        // WRITE SMTHING HERE
    }
    */